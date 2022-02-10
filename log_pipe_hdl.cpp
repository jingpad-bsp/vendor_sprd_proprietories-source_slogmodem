/*
 *  log_pipe_hdl.cpp - The CP log and dump handler class.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-3-2 Zhang Ziyi
 *  Initial version.
 */
#include <cassert>
#include <cstdlib>
#include <cstring>
#ifdef HOST_TEST_
#include "sock_test.h"
#else
#include "cutils/sockets.h"
#endif
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef SUPPORT_RATE_STATISTIC
#include <time.h>
#endif
#include <unistd.h>
#include "cp_dir.h"
#include "cp_set_dir.h"
#include "cp_stor.h"
#include "diag_dev_hdl.h"
#include "ext_wcn_dump.h"
#include "log_ctrl.h"
#include "log_pipe_hdl.h"
#include "media_stor_check.h"
#include "move_dir_to_dir.h"
#include "multiplexer.h"
#include "parse_utils.h"
#include "stor_mgr.h"
#include "trans_log_convey.h"

LogPipeHandler::LogPipeHandler(LogController* ctrl, Multiplexer* multi,
                               const LogConfig::ConfigEntry* conf,
                               StorageManager& stor_mgr, CpClass cp_class)
    : FdHandler(-1, ctrl, multi),
      m_max_buf{0},
      m_max_buf_num{0},
      m_log_commit_threshold{},
      m_buf_commit_threshold{},
      m_rate_statistic_{false},
      m_buffer{nullptr},
      convey_mgr_{new CpConveyManager()},
      log_mode_{LogConfig::LM_OFF},
      m_modem_name(conf->modem_name),
      m_type{conf->type},
      m_internal_max_size{static_cast<uint64_t>(conf->internal_limit) << 20},
      m_external_max_size{static_cast<uint64_t>(conf->external_limit) << 20},
      m_max_log_file_size{static_cast<uint64_t>(conf->file_size_limit) << 20},
      tmp_external_max_size_{},
      overwrite_{conf->overwrite},
      m_log_diag_same{false},
      m_reset_prop{nullptr},
      m_cp_state{CWS_WORKING},
      m_cp_state_hdl{nullptr},
      m_diag_handler{nullptr},
      cur_diag_trans_{nullptr},
      m_stor_mgr{stor_mgr},
      m_storage{nullptr},
      m_cp_class{cp_class},
      mean_start_{0, 0},
      mean_data_size_{},
      mean_timer_{},
      period_start_{0, 0},
      step_data_size_{},
      step_timer_{} {
  // Reset property
  switch (conf->type) {
    case CT_WCDMA:
    case CT_TD:
    case CT_3MODE:
    case CT_4MODE:
    case CT_5MODE:
      m_max_buf = 1024 * 256;
      m_max_buf_num = 12;
      m_log_commit_threshold = 1024 * 128;
      m_buf_commit_threshold = 1024 * 224;
#ifdef SUPPORT_RATE_STATISTIC
      m_rate_statistic_ = true;
#endif
      m_reset_prop = MODEMRESET_PROPERTY;
      m_save_dump_prop = MODEM_SAVE_DUMP_PROPERTY;
      break;
    case CT_WCN:
      // There is a bug in WCN log device driver (Bug 615457) so that the driver
      // read function didn't check the read buffer length.
      // The bug won't be fixed in Marlin/Marlin2.
      // To work around that, 12288 bytes' buffer is setup for WCN log reading,
      // so before the buffer is ready to write to file (when the WCN log size in
      // the buffer is less than 12288 * (5 / 8) bytes), there will always be at
      // least 12288 * ( 3 / 8) = 4608 bytes' free space, which is larger than
      // WCN log device driver's max read return size 4096 bytes.
      m_max_buf = 1024 * 160;
      m_max_buf_num = 3;
      m_log_commit_threshold = 1024 * 128;
      m_buf_commit_threshold = 1024 * 144;
#ifdef SUPPORT_RATE_STATISTIC
      m_rate_statistic_ = true;
#endif
      m_reset_prop = MODEM_WCN_DEVICE_RESET;
      break;
    case CT_GNSS:
      m_max_buf = 4096;
      m_max_buf_num = 4;
      m_log_commit_threshold = 4096;
      m_buf_commit_threshold = 1024 * 3;
      break;
    case CT_AGDSP:
      m_max_buf = 1024 * 160;
      m_max_buf_num = 3;
      m_log_commit_threshold = 1024 * 128;
      m_buf_commit_threshold = 1024 * 128;
      break;
    case CT_PM_SH:
      m_max_buf = 2048;
      m_max_buf_num = 2;
      m_log_commit_threshold = 1024;
      m_buf_commit_threshold = 1536;

      m_reset_prop = MODEMRESET_PROPERTY;
      break;
    case CT_ORCA_MINIAP:
    case CT_ORCA_DP:
      m_max_buf = 1024 * 160;
      m_max_buf_num = 4;
      m_log_commit_threshold = 1024 * 128;
      m_buf_commit_threshold = 1024 * 128;
  #ifdef SUPPORT_RATE_STATISTIC
        m_rate_statistic_ = true;
  #endif
      break;
    default:
      break;
  }
}

LogPipeHandler::~LogPipeHandler() {
  if (m_storage) {
    if (m_buffer) {
      m_storage->free_buffer(m_buffer);
    }
    m_stor_mgr.delete_storage(m_storage);
  }

  if (convey_mgr_) {
    delete convey_mgr_;
    convey_mgr_ = nullptr;
  }

  if (step_timer_) {
    multiplexer()->timer_mgr().del_timer(step_timer_);
    step_timer_ = nullptr;
  }
}

int LogPipeHandler::start_logging() {
  log_mode_ = LogConfig::LM_NORMAL;

  if (CWS_WORKING == m_cp_state) {
    if (-1 == open()) {
      err_log("open %s error", ls2cstring(m_modem_name));
      multiplexer()->timer_mgr().add_timer(300, reopen_log_dev, this);
    } else {
      multiplexer()->register_fd(this, POLLIN);
    }
  }

  if (!m_storage) {
    if (create_storage()) {
      err_log("create_storage error");
    }
  }

  if (m_storage) {
    if (!m_buffer) {
      m_buffer = m_storage->get_buffer();
    }
    m_storage->set_buf_avail_callback(this, buf_avail_callback);
  }

  if (m_rate_statistic_) {
    gettimeofday(&period_start_, 0);
    step_timer_ =
        multiplexer()->timer_mgr().create_timer(60000, data_rate_stat, this);
  }

  //User version and log configuration has not changed.
  if (tmp_external_max_size_) {
    m_external_max_size = tmp_external_max_size_;
    tmp_external_max_size_ = 0;
  }
//    mean_start_ = period_start_;
//    mean_timer_ =
//        multiplexer()->timer_mgr().create_timer(60000, mean_rate_stat, this);
//#endif

  return 0;
}

void LogPipeHandler::buf_avail_callback(void* client) {
  LogPipeHandler* log_pipe = static_cast<LogPipeHandler*>(client);

  if (LogConfig::LM_NORMAL == log_pipe->log_mode_ &&
      log_pipe->fd() >= 0) {
    bool addevt = false;

    switch (log_pipe->m_cp_state) {
      case CWS_DUMP:
      case CWS_SAVE_SLEEP_LOG:
      case CWS_SAVE_RINGBUF:
      case CWS_QUERY_VER:
        if (!log_pipe->m_log_diag_same) {
          addevt = true;
        }
        break;
      default:  // No transaction on diag device
        addevt = true;
        break;
    }
    if (addevt) {
      info_log("buf available, %s enable poll", ls2cstring(log_pipe->name()));
      log_pipe->add_events(POLLIN);
    }
  }
}

int LogPipeHandler::stop_logging() {
  flush();
  if (m_storage) {
    m_storage->stop();
  }

  log_mode_ = LogConfig::LM_OFF;
  delete m_diag_handler;
  m_diag_handler = nullptr;
  if (m_fd >= 0) {
    multiplexer()->unregister_fd(this);
    ::close(m_fd);
    m_fd = -1;
  }

  if (step_timer_) {
    multiplexer()->timer_mgr().del_timer(step_timer_);
    step_timer_ = nullptr;
  }
  return 0;
}

void LogPipeHandler::close_devices() {
  ::close(m_fd);
  m_fd = -1;
}

void LogPipeHandler::process(int /*events*/) {
  if (!m_buffer) {
    m_buffer = m_storage->get_buffer();
    if (!m_buffer) {  // No free buffers
      err_log("no buffer for %s", ls2cstring(m_modem_name));

      del_events(POLLIN);
      return;
    }
  }
  size_t wr_start = m_buffer->data_start + m_buffer->data_len;
  uint8_t* wr_ptr = m_buffer->buffer + wr_start;
  size_t rlen = m_buffer->buf_size - wr_start;
  ssize_t nr = read(fd(), wr_ptr, rlen);

  if (nr > 0) {
    if (m_rate_statistic_) {
      step_data_size_ += nr;
    }

    m_buffer->data_len += nr;

    if (m_buffer->data_len >= m_buf_commit_threshold) {
      int err = m_storage->write(m_buffer);

      if (err < 0) {
        err_log("enqueue CP %s log error, %u bytes discarded",
                ls2cstring(m_modem_name),
                static_cast<unsigned>(m_buffer->data_len));
        m_buffer->data_start = m_buffer->data_len = 0;
      } else {
        m_buffer = nullptr;
      }
    }
  } else {
    if (-1 == nr) {
      err_log("read %s error", ls2cstring(m_modem_name));
      if (EAGAIN == errno || EINTR == errno || ENODATA == errno) {
        err_log("read data error, errno:%d", errno);
        return;
      }
      // Other errors: try to reopen the device
      multiplexer()->unregister_fd(this);
      close_devices();
      if (open() >= 0) {  // Success
        multiplexer()->register_fd(this, POLLIN);
        info_log("%s reopened", ls2cstring(m_modem_name));
      } else {  // Failure: arrange a check callback
        multiplexer()->timer_mgr().add_timer(300, reopen_log_dev, this);
      }
    } else {
      err_log("read %s returns 0, fd = %d", ls2cstring(m_modem_name), fd());
    }
  }
}

void LogPipeHandler::reopen_log_dev(void* param) {
  LogPipeHandler* log_pipe = static_cast<LogPipeHandler*>(param);

  if (LogConfig::LM_NORMAL == log_pipe->log_mode_ &&
      CWS_WORKING == log_pipe->m_cp_state) {
    if (log_pipe->open() >= 0) {
      log_pipe->multiplexer()->register_fd(log_pipe, POLLIN);
      info_log("%s opened, fd = %d", ls2cstring(log_pipe->m_modem_name), log_pipe->fd());
    } else {
      TimerManager& tmgr = log_pipe->multiplexer()->timer_mgr();
      info_log("%s is not opened", ls2cstring(log_pipe->m_modem_name));
      tmgr.add_timer(300, reopen_log_dev, param);
    }
  }
}

void LogPipeHandler::process_reset() {
  if (m_storage) {
    m_storage->stop();
  }
}

void LogPipeHandler::process_alive() {
  if (LogConfig::LM_NORMAL == log_mode_ && -1 == m_fd) {
    multiplexer()->timer_mgr().del_timer(reopen_log_dev);
    if (open() >= 0) {
      multiplexer()->register_fd(this, POLLIN);
    } else {
      multiplexer()->timer_mgr().add_timer(300, reopen_log_dev, this);
    }
  }
  m_cp_state = CWS_WORKING;
}

void LogPipeHandler::close_on_assert() {
  if (m_fd >= 0) {
    multiplexer()->unregister_fd(this);
  }
  close_devices();

  flush();
  truncate_log();
  m_cp_state = CWS_NOT_WORKING;
}

bool LogPipeHandler::will_be_reset() const {
  bool do_reset = false;

  if (m_reset_prop) {
    char reset[PROPERTY_VALUE_MAX];
    unsigned long n;
    char* endp;

    property_get(m_reset_prop, reset, "");
    n = strtoul(reset, &endp, 0);
    do_reset = (1 == n);
  }

  return do_reset;
}

bool LogPipeHandler::shall_save_dump() const {
  bool do_save_dump = false;

  if (m_save_dump_prop) {
    char save_dump[PROPERTY_VALUE_MAX];
    unsigned long n;
    char* endp;

    property_get(m_save_dump_prop,save_dump,"");
    n = strtoul(save_dump,&endp,0);
    do_save_dump = (1 == n);
  }

  return do_save_dump;
}

int LogPipeHandler::write_reset_prop(bool enable/* = true*/) {
  int ret{-1};

  if (m_reset_prop) {
    char arst[2];

    if (enable) {
      arst[0] = '1';
    } else {
      arst[0] = '0';
    }
    arst[1] = '\0';

    property_set(m_reset_prop, arst);

    ret = 0;
  }

  return ret;
}

LogFile* LogPipeHandler::open_dump_mem_file(const struct tm& lt) {
  char log_name[64];

  snprintf(log_name, sizeof log_name,
           "_%04d%02d%02d-%02d%02d%02d.mem", lt.tm_year + 1900,
           lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
  LogString mem_file_name = m_modem_name + log_name;
  auto f = m_storage->create_file(mem_file_name, LogFile::LT_DUMP).lock();

  if (f) {
    return f.get();
  } else {
    err_log("create dump mem file %s failed", ls2cstring(mem_file_name));
    return nullptr;
  }
}

void LogPipeHandler::end_dump() {
  close_on_assert();
  dump_end_subs_.notify();
}

int LogPipeHandler::create_storage() {
  int ret = -1;
  m_storage = m_stor_mgr.create_storage(*this,
                                        m_max_buf,
                                        m_max_buf_num,
                                        m_log_commit_threshold);
  if (m_storage) {
    ret = 0;
  } else {
    err_log("create %s CpStorage failed", ls2cstring(m_modem_name));
  }

  return ret;
}

int LogPipeHandler::copy_files(LogFile::LogType file_type,
                               const LogVector<LogString>& src_files) {
  time_t t = time(0);
  struct tm lt;
  int err_code = LCR_ERROR;

  // generate time to append to copied files' name.
  if (static_cast<time_t>(-1) == t || !localtime_r(&t, &lt)) {
    return err_code;
  }

  char ts[32];
  snprintf(ts, sizeof ts, "_%04d%02d%02d-%02d%02d%02d", lt.tm_year + 1900,
           lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);

  if (!m_storage) {
    if (create_storage()) {
      err_log("create storage error.");
      return err_code;
    }
  }

  for (auto src_file : src_files) {

    if (m_storage->check_quota() < 0) {
      err_log("No space left for copying files.");
      err_code = LCR_ERROR;
      break;
    }

    // Get the file's name in the /path/to/the/file_name.
    const char* file_name =
        static_cast<char*>(
            const_cast<void*>(
                memrchr(ls2cstring(src_file), '/', src_file.length())));

    if (file_name) {
      LogString file_name_dest;
      ++file_name;

      size_t file_name_len =
           src_file.length() -
               static_cast<size_t>(file_name - ls2cstring(src_file));

      const char* file_extension =
          static_cast<char*>(
              const_cast<void*>(memrchr(file_name , '.', file_name_len)));

      if (file_extension) {
        str_assign(file_name_dest, file_name, file_extension - file_name);
        file_name_dest += ts;

        LogString file_extension_name;

        size_t file_extension_len =
             file_name_len - static_cast<size_t>(file_extension - file_name);
        str_assign(file_extension_name, file_extension, file_extension_len);

        file_name_dest += file_extension_name;
      } else {
        str_assign(file_name_dest, file_name, file_name_len);
        file_name_dest += ts;
      }

      auto copy_dest_file =
          m_storage->create_file(file_name_dest, file_type).lock();

      if (!copy_dest_file) {
        err_log("Destination file: %s creation is failed.",
                ls2cstring(file_name_dest));
        err_code = LCR_ERROR;
        break;
      } else if (copy_dest_file->copy(ls2cstring(src_file))) {
        err_log("Copy %s to %s failed.", ls2cstring(src_file),
                ls2cstring(file_name_dest));
        copy_dest_file->dir()->remove(copy_dest_file);
        err_code = LCR_ERROR;
        break;
      } else {
        err_code = LCR_SUCCESS;
      }
    } else {
      // the original file path should be the full path.
      err_log("File: %s path is not valid.", ls2cstring(src_file));
      err_code = LCR_PARAM_INVALID;
      break;
    }
  }

  return err_code;
}

void LogPipeHandler::flush() {
  info_log("LogPipeHandler::flush() enter m_storage= %lu,m_buffer =%lu",
           m_storage,m_buffer);
  if (m_storage) {
    if (m_buffer) {
      if (m_buffer->data_len) {
        if (m_storage->write(m_buffer)) {  // write failed
          m_storage->free_buffer(m_buffer);
        }
      } else {  // Buffer empty
        m_storage->free_buffer(m_buffer);
      }
      m_buffer = nullptr;
    }
    m_storage->flush();
  }
}

void LogPipeHandler::set_wcn_dump_prop() {
  property_set(MODEM_WCN_DUMP_LOG_COMPLETE, "1");
}

int LogPipeHandler::register_dump_start(void* client,
                                        EventNotifier::CallbackFun cb) {
  return dump_start_subs_.add_client(client, cb);
}

void LogPipeHandler::unregister_dump_start(void* client) {
  dump_start_subs_.remove_client(client);
}

int LogPipeHandler::register_dump_end(void* client,
                                      EventNotifier::CallbackFun cb) {
  return dump_end_subs_.add_client(client, cb);
}

int LogPipeHandler::register_dump_ongoing(void* client,
                                          EventNotifier::CallbackFun cb) {
  return dump_ongoing_subs_.add_client(client, cb);
}

void LogPipeHandler::unregister_dump_ongoing(void* client) {
  dump_ongoing_subs_.remove_client(client);
}

void LogPipeHandler::notify_dump_ongoing() {
  dump_ongoing_subs_.notify();
}

void LogPipeHandler::unregister_dump_end(void* client) {
  dump_end_subs_.remove_client(client);
}

void LogPipeHandler::notify_dump_start() {
  dump_start_subs_.notify();
}

void LogPipeHandler::notify_dump_end() {
  dump_end_subs_.notify();
}

int LogPipeHandler::start_diag_trans(TransDiagDevice& trans,
                                     CpWorkState state) {
  if (m_diag_handler) {
    err_log("The diag handler is not null");
    return -1;
  }

  int err{0};

  if (m_log_diag_same) {
    m_diag_handler =
        new DiagDeviceHandler(fd(), &trans, controller(), multiplexer());
    del_events(POLLIN);
  } else {
    m_diag_handler = new DiagDeviceHandler(m_diag_dev_path, &trans,
                                           controller(), multiplexer());
    int fd = m_diag_handler->open();
    if (fd < 0) {
      delete m_diag_handler;
      m_diag_handler = nullptr;
      err_log("open %s error", ls2cstring(m_diag_dev_path));
      err = -1;
    } else {
      info_log("open %s, fd= %d succ", ls2cstring(m_diag_dev_path), fd);
    }
  }
  if (m_diag_handler) {
    trans.bind(m_diag_handler);
    multiplexer()->register_fd(m_diag_handler, POLLIN);
    cur_diag_trans_ = &trans;
    m_cp_state = state;
  }

  return err;
}

void LogPipeHandler::stop_diag_trans() {
  if (cur_diag_trans_) {
    delete m_diag_handler;
    m_diag_handler = nullptr;
    if (m_log_diag_same && fd() >= 0) {
      add_events(POLLIN);
    }

    TransModem::Type t = cur_diag_trans_->type();
    if (TransModem::DUMP_RECEIVER == t) {
      m_cp_state = CWS_NOT_WORKING;
    } else {
      m_cp_state = CWS_WORKING;
    }
    cur_diag_trans_ = nullptr;
  }
}

void LogPipeHandler::data_rate_stat(void* param) {
  time_t t;
  struct tm lt;
  int r_fd = -1;
  char fn[50] = {0};
  double rate = 0;
  int write_num = 0;
  int write_back = 0;
  LogPipeHandler* cp = static_cast<LogPipeHandler*>(param);
  struct timeval tnow;

  cp->step_timer_ = nullptr;
  gettimeofday(&tnow, 0);
  rate = static_cast<double>(cp->step_data_size_) /
                (static_cast<double>(tnow - cp->period_start_) / 1000);
  info_log("%s rate %f", ls2cstring(cp->m_modem_name), rate);

  // save to /data/ylog/rate_statistic.txt
  r_fd = cp->controller()->get_ratefile();
  if (r_fd < 0) {
    err_log("rate_statistic file open error.");
    goto finish;
  }
  t = time(0);
  localtime_r(&t, &lt);
  write_num = snprintf(fn, sizeof fn, "%04d%02d%02d-%02d%02d%02d---%s rate %f\n",
                       lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour,
                       lt.tm_min, lt.tm_sec, ls2cstring(cp->m_modem_name), rate);
  write_back = write(r_fd, fn, write_num);
  if (write_back == -1) {
    err_log("write fail");
  } else if (write_back != write_num) {
    err_log("write had lost %d", write_num - write_back);
  }

finish:
  cp->period_start_ = tnow;
  cp->step_data_size_ = 0;
  cp->step_timer_ =
      cp->multiplexer()->timer_mgr().create_timer(60000, data_rate_stat, param);
}

void LogPipeHandler::mean_rate_stat(void* param) {
  LogPipeHandler* cp = static_cast<LogPipeHandler*>(param);
  struct timeval tnow;

  gettimeofday(&tnow, 0);
  info_log("Mean rate %f",
           static_cast<double>(cp->mean_data_size_) /
               (static_cast<double>(tnow - cp->mean_start_) / 1000));
  cp->mean_start_ = tnow;
  cp->mean_data_size_ = 0;
  cp->mean_timer_ =
      cp->multiplexer()->timer_mgr().create_timer(60000, mean_rate_stat, param);
}

int LogPipeHandler::start_log_convey(void* client,
    Transaction::ResultCallback cb, TransLogConvey*& trans) {
  int ret = Transaction::TRANS_E_SUCCESS;

  LogVector<CpDirectory*> cp_dirs;
  m_stor_mgr.sync_cp_directory(m_type, cp_dirs);

  LogVector<std::unique_ptr<ConveyUnitBase>> units;
  for (auto dir : cp_dirs) {
    auto cpset = dir->cp_set_dir();
    info_log("%s will be push into units", ls2cstring(cpset->path()));
    units.push_back(std::unique_ptr<MoveDirToDirUnit>{
        new MoveDirToDirUnit(&m_stor_mgr, m_type,
                             cpset->cp_class(), dir, cpset->priority())});
  }

  trans = new TransLogConvey(convey_mgr_, TransModem::CONVEY_LOG,
                             controller()->convey_workshop());
  trans->set_client(client, cb);

  trans->add_units(units);
  info_log("number of transaction is %d", convey_mgr_->number());
  ret = convey_mgr_->start_trans(trans);

  return ret;
}

void LogPipeHandler::cancel_log_convey(TransLogConvey* trans) {
  convey_mgr_->cancel_trans(trans);
}

void LogPipeHandler:: truncate_log(){
  if (m_storage && m_storage->current_file_saving()) {
    m_storage->on_cur_media_disabled();
  }
}
