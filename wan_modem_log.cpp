/*
 *  wan_modem_log.cpp - The WAN MODEM log and dump handler class.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-7-13 Zhang Ziyi
 *  Initial version.
 */

#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <pthread.h>

#include "client_mgr.h"
#include "cp_dir.h"
#include "cp_set_dir.h"
#include "cp_stor.h"
#include "diag_dev_hdl.h"
#include "diag_stream_parser.h"
#include "log_config.h"
#include "log_ctrl.h"
#include "modem_parse_library_save.h"
#include "pm_modem_dump.h"
#include "trans_last_log.h"
#include "trans_log_convey.h"
#include "trans_modem_time.h"
#include "trans_modem_ver.h"
#include "trans_normal_log.h"
#ifdef USE_ORCA_MODEM
#include "trans_orca_dump.h"
#endif
#include "trans_orca_etb.h"

#include "trans_save_evt_log.h"
#include "trans_save_sleep_log.h"
#include "trans_start_evt_log.h"
#include "trans_stop_cellular_log.h"
#include "wan_modem_log.h"
#ifdef MODEM_TALIGN_AT_CMD_
#include "wan_modem_time_sync_atcmd.h"
#else
#include "wan_modem_time_sync_refnotify.h"
#endif


WanModemLogHandler::WanModemLogHandler(LogController* ctrl, Multiplexer* multi,
                                       const LogConfig::ConfigEntry* conf,
                                       StorageManager& stor_mgr,
                                       const char* dump_path, CpClass cp_class)
    : LogPipeHandler{ctrl, multi, conf, stor_mgr, cp_class},
      m_dump_path{dump_path},
      m_wcdma_iq_mgr{this},
      m_save_wcdma_iq{false},
      m_save_md{false},
      m_md_to_int{false},
      init_evt_log_{false},
      start_evt_trans_{nullptr},
      time_sync_mgr_{nullptr},
      m_timestamp_miss{true},
      m_evt{CpStateHandler::CE_NONE},
      m_ver_state{MVS_NOT_BEGIN},
      m_ver_stor_state{MVSS_NOT_SAVED},
      m_reserved_version_len{0},
      m_poweron_period{0},
      at_ctrl_{ctrl, multi},
      cmd_ctrl_{ctrl, multi} {

  get_tty_path();
  init_cmd_handler();
  if (conf->type == CT_5MODE) {
    #ifdef MODEM_TALIGN_AT_CMD_
    time_sync_mgr_ = new WanModemTimeSyncAtCmd{this};
    #else
    time_sync_mgr_ = new WanModemTimeSyncRefnotify(controller(), multiplexer());
    info_log("WanModemTimeSyncRefnotify create");
    #endif
    time_sync_mgr_->set_time_update_callback(this,
      notify_modem_time_update);
    time_sync_mgr_->start();
  }
}
WanModemLogHandler::~WanModemLogHandler() {
  // Close the event list file.
  if (auto evt_file = event_list_file_.lock()) {
    evt_file->close();
    event_list_file_.reset();
  }

  if (time_sync_mgr_) {
    time_sync_mgr_->cleanup_time_update_callback();
    delete time_sync_mgr_;
    time_sync_mgr_ = nullptr;
  }

  if (start_evt_trans_) {
    start_evt_trans_->cancel();
    delete start_evt_trans_;
  }

  auto cp_stor = storage();
  if (cp_stor) {
    cp_stor->unsubscribe_media_change_event(this);
  }
}

void WanModemLogHandler::get_tty_path() {
  const char* tty_prop{nullptr};

  switch (type()) {
    case CT_WCDMA:
    case CT_TD:
    case CT_3MODE:
    case CT_4MODE:
    case CT_5MODE:
      tty_prop = MODEM_TTY_PROPERTY;
      break;
    default:
      break;
  }

  if (tty_prop) {
    char val[PROPERTY_VALUE_MAX];

    property_get(tty_prop, val, "");
    if (val[0]) {
      LogString tty_path = val;
      tty_path += "28";
      at_ctrl_.set_path(ls2cstring(tty_path));
    }
  }
}

void WanModemLogHandler::init_cmd_handler() {
  char val[PROPERTY_VALUE_MAX];
  property_get(MODEM_COMMAND_PROPERTY, val, "");
  if (val[0]) {
    cmd_ctrl_.set_path(val);
    cmd_ctrl_.try_open();
  }
}

int WanModemLogHandler::start(LogConfig::LogMode mode) {
  if (LogConfig::LM_NORMAL == mode) {  // Normal log
    if (start_logging()) {  // Start failed
      return -1;
    }

    // Start WCDMA I/Q if enabled
    if (m_save_wcdma_iq) {
      if (m_wcdma_iq_mgr.start(storage())) {
        err_log("start saving WCDMA I/Q error");
      }
    }

    //Debug
    //at_ctrl_.register_events(this, modem_event_notify);
  } else {  // Event log
    // Get ready for the storage
    if (!storage()) {
      if (create_storage()) {
        return -1;
      }
    }

    int err = start_event_log(this, init_evt_log_result, start_evt_trans_);
    switch (err) {
      case Transaction::TRANS_E_SUCCESS:
        if (Transaction::TRANS_R_SUCCESS == start_evt_trans_->result()) {
          set_log_mode(LogConfig::LM_EVENT);
        } else {
          err_log("start event log failed");
          init_evt_log_ = true;
        }
        delete start_evt_trans_;
        start_evt_trans_ = nullptr;
        break;
      case Transaction::TRANS_E_STARTED:
        init_evt_log_ = true;
        break;
      default:
        init_evt_log_ = true;
        break;
    }
  }

  // modem parse library should be saved when media changed
  storage()->subscribe_media_change_event(media_change_notify, this);

  if (0 != m_poweron_period) {
    struct timespec tnow;
    if (-1 == clock_gettime(CLOCK_BOOTTIME, &tnow)) {
      err_log("fail to get boot time");
    } else {
      int power_on_time = m_poweron_period - static_cast<int>(tnow.tv_sec);
      m_poweron_period = 0;

      if (power_on_time > 0) {
        set_cp_class(CLASS_MIX);
        set_overwrite(false);
        multiplexer()->timer_mgr().create_timer(power_on_time * 1000,
                                                power_on_stor_finish,
                                                this);
      }
    }
  }

  // Start version query
  if (MVS_NOT_BEGIN == m_ver_state || MVS_FAILED == m_ver_state) {
    if (!start_ver_query()) {
      m_ver_state = MVS_QUERY;
    } else {
      err_log("start MODEM version query error");
      if (get_diag_handler() == NULL) {
        multiplexer()->timer_mgr().add_timer(1000, requery_modem_ver, this);
      } else {
        info_log("diag_handler is not NULL");
      }
      m_ver_state = MVS_FAILED;
    }
  }
  LogConfig::MipiLogList mipilog = controller()->config()->get_mipi_log_info();
  set_mipi_log_info(mipilog);
  return 0;
}

int WanModemLogHandler::stop() {
  // If WCDMA I/Q is started, stop it.
  info_log("WanModemLogHandler::stop() enter ");
  if (m_wcdma_iq_mgr.is_started()) {
    if (m_wcdma_iq_mgr.stop()) {
      err_log("stop saving WCDMA I/Q error");
    }
  }
  // Stop MODEM version query
  if (MVS_QUERY == m_ver_state) {
    TransWanModemVerQuery* query = static_cast<TransWanModemVerQuery*>(
        cur_diag_trans());

    remove(query);
    query->cancel();
    delete query;

    m_ver_state = MVS_NOT_BEGIN;
  }

  auto cp_stor = storage();
  if (cp_stor) {
    cp_stor->unsubscribe_media_change_event(this);
  }

  return stop_logging();
}

void WanModemLogHandler::init_evt_log_result(void* client,
                                             Transaction* trans) {
  WanModemLogHandler* wan = static_cast<WanModemLogHandler*>(client);
  if (wan->init_evt_log_ && wan->start_evt_trans_) {
    if (Transaction::TRANS_R_SUCCESS == trans->result()) {
      info_log("init event log successful");
      wan->set_log_mode(LogConfig::LM_EVENT);
    } else {
      err_log("init event log failed");
    }

    delete wan->start_evt_trans_;
    wan->start_evt_trans_ = nullptr;
    wan->init_evt_log_ = false;
  } else {
    err_log("start event result when no trans");
  }
}

int WanModemLogHandler::start_dump(const struct tm& lt,
                                   CpStateHandler::CpEvent evt) {
  int err {-1};

  if (!storage()) {
    if (create_storage()) {
      err_log("fail to create storage");
      return err;
    }
  }

#ifdef USE_ORCA_MODEM
  TransOrcaDump* dump;
  dump = new TransOrcaDump{this,
                           this, *storage(), lt,
                           evt, "md",
                           "\nmodem_memdump_finish", 0x33};
#else
  TransPmModemDump* dump;
  dump = new TransPmModemDump{this, this, *storage(), lt,
                              m_dump_path,
                              "md",
                              "\nmodem_memdump_finish", 0x33};
#endif

  dump->set_client(this, dump_result);
  dump->set_periodic_callback(this, ongoing_report);
  if (number()) {
    enqueue(dump);
    err = 0;
  } else {
    err = dump->execute();
    switch (err) {
      case Transaction::TRANS_E_STARTED:
        enqueue(dump);
        break;
      case Transaction::TRANS_E_SUCCESS:
        delete dump;
        break;
      default:
        delete dump;
        err = Transaction::TRANS_E_ERROR;
        break;
    }
  }

  return err;
}

int WanModemLogHandler::start_dump_etb(const struct tm& lt,
                                   CpStateHandler::CpEvent evt){
  int err {-1};

  if (!storage()) {
    if (create_storage()) {
      err_log("fail to create storage");
      return err;
    }
  }

  TransOrcaEtb* dump_etb;
  info_log("new TransOrcaEtb");
  dump_etb = new TransOrcaEtb{this,
                           this, *storage(),
                           lt,evt,
                           "md"};
  dump_etb->set_client(this, dump_etb_result);
  if (number()) {
      enqueue(dump_etb);
      err = 0;
  } else {
    err = dump_etb->execute();
    switch (err) {
    case Transaction::TRANS_E_STARTED:
      enqueue(dump_etb);
      err = 0;
      break;
    case Transaction::TRANS_E_SUCCESS:
      delete dump_etb;
      break;
    default:
      delete dump_etb;
      err = Transaction::TRANS_E_ERROR;
      break;
  }
}
return err;
}
void WanModemLogHandler::dump_etb_result(void* client,
                                     Transaction* trans) {
  WanModemLogHandler* cp = static_cast<WanModemLogHandler*>(client);
  info_log("%s dump %s", ls2cstring(cp->name()),
             Transaction::TRANS_R_SUCCESS == trans->result() ?
                 "succeeded" : "failed");
//  cp->etb_dump_done = true;
  delete trans;
}
void WanModemLogHandler::ongoing_report(void* client) {
  WanModemLogHandler* cp = static_cast<WanModemLogHandler*>(client);

  cp->notify_dump_ongoing();
}

void WanModemLogHandler::dump_result(void* client,
                                     Transaction* trans) {
  WanModemLogHandler* cp = static_cast<WanModemLogHandler*>(client);

  cp->end_dump();

  info_log("%s dump %s", ls2cstring(cp->name()),
           Transaction::TRANS_R_SUCCESS == trans->result() ?
               "succeeded" : "failed");

  delete trans;
}

int WanModemLogHandler::create_storage() {
  int ret = LogPipeHandler::create_storage();

  if (!ret) {
    storage()->set_new_log_callback(new_log_callback);
  }

  return ret;
}

int WanModemLogHandler::start_ver_query() {
  int ret;
  TransWanModemVerQuery* query = new TransWanModemVerQuery{this};
  query->set_client(this, modem_ver_result);

  if (!cur_diag_trans()) {
    ret = query->execute();
    switch (ret) {
      case Transaction::TRANS_E_STARTED:
        enqueue(query);
        ret = 0;
        break;
      case Transaction::TRANS_E_SUCCESS:
        delete query;
        ret = -1;
        break;
      default:  // TRANS_E_ERROR
        delete query;
        ret = -1;
        break;
    }
  } else {  // The diag device is busy now, so enqueue it.
    enqueue(query);
    ret = 0;
  }

  return ret;
}

void WanModemLogHandler::modem_ver_result(void* client,
                                          Transaction* trans) {
  WanModemLogHandler* cp = static_cast<WanModemLogHandler*>(client);

  if (Transaction::TRANS_R_SUCCESS == trans->result()) {
    TransWanModemVerQuery* query = static_cast<TransWanModemVerQuery*>(trans);
    size_t len;
    const char* ver = query->version(len);
    str_assign(cp->m_modem_ver, ver, len);
    cp->m_ver_state = MVS_GOT;

    info_log("WAN MODEM ver: %s", ls2cstring(cp->m_modem_ver));

    if (cp->storage()->current_file_saving() &&
        MVSS_NOT_SAVED == cp->m_ver_stor_state) {
      // Correct the version info
      cp->correct_ver_info();
      cp->m_ver_stor_state = MVSS_SAVED;
    }
  } else {  // Failed to get version
    cp->m_ver_state = MVS_FAILED;
    err_log("query MODEM software version failed");
  }

  delete trans;
}

void WanModemLogHandler::fill_smp_header(uint8_t* buf, size_t len,
                                         unsigned lcn, unsigned type) {
  memset(buf, 0x7e, 4);
  buf[4] = static_cast<uint8_t>(len);
  buf[5] = static_cast<uint8_t>(len >> 8);
  buf[6] = static_cast<uint8_t>(lcn);
  buf[7] = static_cast<uint8_t>(type);
  buf[8] = 0x5a;
  buf[9] = 0x5a;

  uint32_t cs = ((type << 8) | lcn);

  cs += (len + 0x5a5a);
  cs = (cs & 0xffff) + (cs >> 16);
  cs = ~cs;
  buf[10] = static_cast<uint8_t>(cs);
  buf[11] = static_cast<uint8_t>(cs >> 8);
}

void WanModemLogHandler::fill_diag_header(uint8_t* buf, uint32_t sn,
                                          unsigned len, unsigned cmd,
                                          unsigned subcmd) {
  buf[0] = static_cast<uint8_t>(sn);
  buf[1] = static_cast<uint8_t>(sn >> 8);
  buf[2] = static_cast<uint8_t>(sn >> 16);
  buf[3] = static_cast<uint8_t>(sn >> 24);

  buf[4] = static_cast<uint8_t>(len);
  buf[5] = static_cast<uint8_t>(len >> 8);
  buf[6] = static_cast<uint8_t>(cmd);
  buf[7] = static_cast<uint8_t>(subcmd);
}

uint8_t* WanModemLogHandler::frame_noversion_and_reserve(size_t& length) {
  const char* unavailable = "(unavailable)";
  uint8_t* ret = nullptr;

  if (log_diag_dev_same()) {
    // Keep the place holder for diag frame
    // modem version length * 2 (escaped length) +
    // 8 * 2 (length for escaped header) + 2 (two flags)
    length = (PRE_MODEM_VERSION_LEN << 1) + 18;
    ret = new uint8_t[length];

    size_t framed_len;
    uint8_t* diag_frame =
        DiagStreamParser::frame(0xffffffff, 0, 0,
                                reinterpret_cast<uint8_t*>(
                                    const_cast<char*>(unavailable)),
                                strlen(unavailable), framed_len);
    memcpy(ret, diag_frame, framed_len);
    delete [] diag_frame;
    memset(ret + framed_len, 0x7e, length - framed_len);
  } else {
    // Keep the place holder for smp frame
    // modem version length + 12(smp) + 8(diag)
    length = PRE_MODEM_VERSION_LEN + 20;
    ret = new uint8_t[length];
    memset(ret, ' ', length);

    fill_smp_header(ret, length - 4, 1, 0x9d);
    fill_diag_header(ret + 12, 0xffffffff, length - 12, 0, 0);
    memcpy(ret + 20, unavailable, strlen(unavailable));
  }

  return ret;
}

uint8_t* WanModemLogHandler::frame_version_info(const uint8_t* payload,
                                                size_t pl_len,
                                                size_t len_reserved,
                                                size_t& frame_len) {
  uint8_t* ret = nullptr;

  if (log_diag_dev_same()) {
    ret = DiagStreamParser::frame(0xffffffff, 0, 0, payload,
                                  pl_len, frame_len);
    if (len_reserved && (frame_len > len_reserved)) {
      // if framed info is larger than preserved, the next diag frame will
      // be destroyed.
      delete [] ret;
      ret = nullptr;
      err_log("reserved length %u is less than needed %u",
              static_cast<unsigned>(len_reserved),
              static_cast<unsigned>(frame_len));
    }
  } else {
    if (len_reserved) {
      if (len_reserved < pl_len + 20) {
        err_log("reserved length %u is less than needed %u",
                static_cast<unsigned>(len_reserved),
                static_cast<unsigned>(pl_len + 20));
        return nullptr;
      } else {
        frame_len = len_reserved;
      }
    } else {
      frame_len = 20 + pl_len;
    }

    ret = new uint8_t[frame_len];
    memset(ret, ' ', frame_len);

    fill_smp_header(ret, frame_len - 4, 1, 0x9d);
    fill_diag_header(ret + 12, 0xffffffff, frame_len - 12, 0, 0);
    memcpy(ret + 20, payload, pl_len);
  }

  return ret;
}

void WanModemLogHandler::correct_ver_info() {
  CpStorage* stor = storage();
  DataBuffer* buf = stor->get_buffer();

  if (!buf) {
    err_log("No buffer for correcting MODEM version info");
    return;
  }

  size_t size_to_frame = m_modem_ver.length();
  if (size_to_frame > PRE_MODEM_VERSION_LEN) {
    size_to_frame = PRE_MODEM_VERSION_LEN;
  }

  size_t framed_len;
  uint8_t* ver_framed =
      frame_version_info(reinterpret_cast<uint8_t*>(const_cast<char*>(
                             ls2cstring(m_modem_ver))),
                         size_to_frame, m_reserved_version_len, framed_len);
  if (!ver_framed) {
    err_log("fail to correct modem version information");
    stor->free_buffer(buf);
    return;
  }

  memcpy(buf->buffer, ver_framed, framed_len);
  delete [] ver_framed;

  buf->dst_offset = sizeof(modem_timestamp);
  buf->data_len = framed_len;

  if (!stor->amend_current_file(buf)) {
    stor->free_buffer(buf);
    err_log("amend_current_file MODEM version error");
  }
}

bool WanModemLogHandler::save_timestamp(LogFile* f) {
  time_sync ts;
  bool ret = false;
  modem_timestamp modem_ts = {0, 0, 0, 0};

  if (time_sync_mgr_ && time_sync_mgr_->get_time_sync_info(ts)) {
    m_timestamp_miss = false;
    calculate_modem_timestamp(ts, modem_ts);
  } else {
    m_timestamp_miss = true;
    err_log("Wan modem timestamp is not available.");
  }

  // if timestamp is not available, 0 will be written to the first 16 bytes.
  // otherwise, save the timestamp.
  ssize_t n = f->write_raw(&modem_ts, sizeof(uint32_t) * 4);

  if (static_cast<size_t>(n) == (sizeof(uint32_t) * 4)) {
    ret = true;
  } else {
    err_log("write timestamp fail.");
  }

  if (n > 0) {
    f->add_size(n);
  }

  return ret;
}

void WanModemLogHandler::media_change_notify(void* client) {
  WanModemLogHandler* wan = static_cast<WanModemLogHandler*>(client);
  auto ms = wan->stor_mgr().get_media_stor();
  if (ms) {
    auto cpset = ms->prepare_cp_set(CLASS_MODEM);
    if (cpset) {
      bool new_dir_created = false;
      auto cpdir = cpset->prepare_cp_dir(wan->type(), new_dir_created);

      if (cpdir) {
        auto mpl = new ModemParseLibrarySave(wan->convey_mgr_, wan, cpdir);
        mpl->set_client(wan, parse_lib_result);

        switch (wan->convey_mgr_->start_trans(mpl)) {
          case Transaction::TRANS_E_SUCCESS:
            delete mpl;
            mpl = nullptr;
            break;
          default: //TRANS_E_STARTED:
            break;
        }
      }
    }
  }
}

void WanModemLogHandler::on_new_log_file(LogFile* f) {
  save_timestamp(f);

  // MODEM version info
  uint8_t* buf = nullptr;
  // resulted length to be written
  size_t ver_len = 0;
  info_log("m_ver_state = %d", m_ver_state);
  if (MVS_GOT == m_ver_state) {
    buf = frame_version_info(reinterpret_cast<uint8_t*>(const_cast<char*>(
                                 ls2cstring(m_modem_ver))),
                             m_modem_ver.length(), 0, ver_len);
    m_ver_stor_state = MVSS_SAVED;
  } else {
    info_log("no MODEM version when log file is created");

    buf = frame_noversion_and_reserve(ver_len);
    m_reserved_version_len = ver_len;
    m_ver_stor_state = MVSS_NOT_SAVED;
  }

  ssize_t nwr = f->write_raw(buf, ver_len);
  delete [] buf;

  if (nwr > 0) {
    f->add_size(nwr);
  }
}

void WanModemLogHandler::notify_modem_time_update(void* client,
    const time_sync& ts) {
  WanModemLogHandler* wan = static_cast<WanModemLogHandler*>(client);

  if (!wan->enabled()) {
    return;
  }

  CpStorage* stor = wan->storage();

  if (stor->current_file_saving()) {
    if (wan->m_timestamp_miss) {
      modem_timestamp modem_ts;

      wan->calculate_modem_timestamp(ts, modem_ts);
      // Amend the current log file with the correct time stamp.
      DataBuffer* buf = stor->get_buffer();

      if (buf) {
        memcpy(buf->buffer, &modem_ts, sizeof modem_ts);
        buf->data_len = sizeof modem_ts;
        buf->dst_offset = 0;
        if (stor->amend_current_file(buf)) {
          wan->m_timestamp_miss = false;
          info_log("WAN MODEM time stamp is corrected.");
        } else {
          stor->free_buffer(buf);
          err_log("amend_current_file error");
        }
      } else {
        err_log("no buffer to correct WAN MODEM time alignment info");
      }
    } else {
      err_log("New Time sync notification but current log file have timestamp.");
    }
  } else {
    info_log("No modem log file when modem time sync info is received.");
  }
}

void WanModemLogHandler::calculate_modem_timestamp(const time_sync& ts,
    modem_timestamp& mp) {
  struct timeval time_now;
  struct sysinfo sinfo;

  gettimeofday(&time_now, 0);
  sysinfo(&sinfo);

  mp.magic_number = 0x12345678;
  mp.sys_cnt = ts.sys_cnt + (sinfo.uptime - ts.uptime) * 1000;
  mp.tv_sec = time_now.tv_sec + get_timezone_diff(time_now.tv_sec);
  mp.tv_usec = time_now.tv_usec;
}

void WanModemLogHandler::save_minidump(const struct tm& lt) {
  char md_name[80];
  std::shared_ptr<LogFile> f;

  snprintf(md_name, sizeof md_name,
           "minidump_%04d%02d%02d-%02d%02d%02d.bin", lt.tm_year + 1900,
           lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);

  if (m_md_to_int) {
    // First check storage quota
    if (storage()->check_quota(StorageManager::MT_INT_STOR) < 0) {
      err_log("no space for mini dump");
      return;
    }
    f = storage()->create_file(LogString(md_name), LogFile::LT_MINI_DUMP,
                               StorageManager::MT_INT_STOR).lock();
  } else {
    // First check storage quota
    if (storage()->check_quota() < 0) {
      err_log("no space for mini dump");
      return;
    }
    f = storage()->create_file(LogString(md_name), LogFile::LT_MINI_DUMP).lock();
  }

  if (f) {
    time_sync ts;
    modem_timestamp modem_ts = {0, 0, 0, 0};

    if (time_sync_mgr_ && time_sync_mgr_->get_time_sync_info(ts)) {
      calculate_modem_timestamp(ts, modem_ts);
      ssize_t n = f->write_raw(&modem_ts, sizeof(modem_timestamp));

      if (static_cast<size_t>(n) != sizeof(modem_timestamp)) {
        err_log("write timestamp fail.");
      }

      if (n > 0) {
        f->add_size(n);
      }
    } else {
      err_log("Wan modem timestamp is not available.");
    }

    int ret = -1;
    if (CT_WCDMA == type()) {
      ret = f->copy(MINI_DUMP_WCDMA_SRC_FILE);
    } else if (CT_TD == type()) {
      ret = f->copy(MINI_DUMP_TD_SRC_FILE);
    } else {
      ret = f->copy(MINI_DUMP_LTE_SRC_FILE);
    }
    if (ret) {
      // if mini dump fail, save assert infomation to minidump file
      size_t n = f->write_raw(ls2cstring(m_assert_info), m_assert_info_len);
      if (n != m_assert_info_len) {
        err_log("save assert info failed");
      } else {
        ret = 0;
      }
    }
    f->close();
    if (ret) {
      err_log("save mini dump error");
      f->dir()->remove(f);
    }
  } else {
    err_log("create mini dump file %s failed", md_name);
  }
}

void WanModemLogHandler::save_sipc_and_minidump(const struct tm& lt) {
  if (!storage()) {
    if (create_storage()) {
      err_log("fail to create storage");
      return;
    }
  }

  LogController::save_sipc_dump(storage(), lt);
#ifndef USE_ORCA_MODEM
  LogController::save_etb(storage(), lt);
#endif

  if (m_save_md) {
    save_minidump(lt);
  }
}

void WanModemLogHandler::process_assert(const struct tm& lt,
                                        CpStateHandler::CpEvent evt) {
  if (CWS_DUMP == cp_state()) {  // Dumping now, ignore.
    warn_log("assertion got when dumping");
    return;
  }

  info_log("wan modem process_assert evt=%d, m_evt=%d.", evt, m_evt);

  if (CWS_NOT_WORKING == cp_state()
      && (evt == m_evt)) {
    info_log("assertion got when MODEM nonfunctional");
    notify_dump_end();
    return;
  }
  m_evt = evt;

  set_cp_state(CWS_DUMP);

  save_sipc_and_minidump(lt);

  //dump
  bool check_modem_state = (CpStateHandler::CE_PANIC == evt) ||
                           (CpStateHandler::CE_ASSERT_DP == evt) ||
                           (CpStateHandler::CE_BLOCKED == evt);
  bool shall_dump = shall_save_dump();
  info_log("wan modem state is %d, cp log is %s, dump mode is %s.",
           cp_state(), enabled() ? "enabled" : "disabled",
           shall_dump ? "on" : "off");

  info_log("evt is %d, miniap_log is %s", evt,
          m_mini_ap_log ? "on" : "off");
  bool dump = (((LogConfig::LM_OFF != log_mode()) && shall_save_dump()) ||
               (check_modem_state && shall_save_dump()));
  if (dump) {
    info_log("modem memory will be saved");
    notify_dump_start();

    // if modem is blocked, dump etb
    if(evt == CpStateHandler::CE_BLOCKED){
      int err = start_dump_etb(lt, evt);
      if (err != 0) {
        err_log("fail to save etb dump");
      }
    }

    int err = start_dump(lt, evt);
    if (err < 0) {
      err_log("fail to save memory dump");
      end_dump();
    } else if (!err){
      end_dump();
    }
  } else {
    end_dump();
  }

  if (time_sync_mgr_) {
    time_sync_mgr_->on_modem_assert();
  }
}

void WanModemLogHandler::power_on_stor_finish(void* param) {
  WanModemLogHandler* wm = static_cast<WanModemLogHandler*>(param);
  info_log("power on period is arrived");

  wm->flush();
  info_log("flush over");
  wm->storage()->stop();
  info_log("storage stop over");
  wm->set_cp_class(CLASS_MODEM);
  bool en = false;
  wm->controller()->get_log_overwrite(wm->type(), en);
  wm->set_overwrite(en);
  info_log("power_on_stor_finish over");
}

int WanModemLogHandler::enable_wcdma_iq() {
  // We need to reboot to turn on the /dev/iq_mem device, so we
  // only set the m_save_wcdma_iq member here.
  m_save_wcdma_iq = true;
  return LCR_SUCCESS;
}

int WanModemLogHandler::disable_wcdma_iq() {
  if (!m_save_wcdma_iq) {
    return LCR_SUCCESS;
  }

  int ret = LCR_SUCCESS;

  if (enabled()) {
    if (!m_wcdma_iq_mgr.stop()) {
      m_save_wcdma_iq = false;
    } else {
      ret = LCR_ERROR;
    }
  } else {
    m_save_wcdma_iq = false;
  }
  return ret;
}

int WanModemLogHandler::pause() {
  flush();

  if (m_wcdma_iq_mgr.is_started()) {
    m_wcdma_iq_mgr.pause();
    m_wcdma_iq_mgr.pre_clear();
  }

  return 0;
}

int WanModemLogHandler::resume() {
  if (m_wcdma_iq_mgr.is_started()) {
    m_wcdma_iq_mgr.resume();
  }

  // Open the event list file.
  if (LogConfig::LM_EVENT == log_mode()) {
    CpStorage* cp_stor = storage();
    if (!cp_stor) {
      if (!WanModemLogHandler::create_storage()) {
        cp_stor = storage();
      }
    }
  }

  return 0;
}

void WanModemLogHandler::process_alive() {
  if (CWS_WORKING != cp_state()) {
    info_log("cp state is not working");
    LogConfig::MipiLogList mipilog = controller()->config()->get_mipi_log_info();
    set_mipi_log_info(mipilog);
    LogString miniap_cmd;
    LogString orcadp_cmd;
    str_assign(miniap_cmd, "SET_MINIAP_LOG", 14);
    str_assign(orcadp_cmd, "SET_ORCADP_LOG", 14);
    set_orca_log(miniap_cmd, m_mini_ap_log);
    set_orca_log(orcadp_cmd, m_orcadp_log);
  }
  // Call base class first
  LogPipeHandler::process_alive();
  if (time_sync_mgr_) {
    time_sync_mgr_->on_modem_alive();
  }

  if (init_evt_log_ && !start_evt_trans_) {
    int err = start_event_log(this, init_evt_log_result, start_evt_trans_);
    switch (err) {
      case Transaction::TRANS_E_SUCCESS:
        delete start_evt_trans_;
        start_evt_trans_ = nullptr;
        init_evt_log_ = false;
        break;
      case Transaction::TRANS_E_STARTED:
        break;
      default:
        init_evt_log_ = false;
        break;
    }
  }

  if ((LogConfig::LM_OFF != log_mode() || init_evt_log_) &&
      (MVS_NOT_BEGIN == m_ver_state || MVS_FAILED == m_ver_state)) {
    if (!start_ver_query()) {
      m_ver_state = MVS_QUERY;
    } else {
      err_log("start MODEM version query error");
      if (get_diag_handler() == NULL) {
        multiplexer()->timer_mgr().add_timer(1000, requery_modem_ver, this);
      } else {
        info_log("diag_handler is not NULL");
      }
      m_ver_state = MVS_FAILED;
    }
  }
}

void WanModemLogHandler::requery_modem_ver(void* param) {
  WanModemLogHandler* cp_handler = static_cast<WanModemLogHandler*>(param);
  if (0 != cp_handler->start_ver_query()) {
    TimerManager& tmgr = cp_handler->multiplexer()->timer_mgr();
    tmgr.add_timer(1000, requery_modem_ver, param);
  }

}

int WanModemLogHandler::save_sleep_log(void* client,
                                       Transaction::ResultCallback cb,
                                       TransSaveSleepLog*& trans) {
  // When ClientHandler request to save sleep log, m_storage
  // may not be created.
  if (!storage()) {
    if (create_storage()) {
      return Transaction::TRANS_E_ERROR;
    }
  }

  TransSaveSleepLog* sleep_log = new TransSaveSleepLog{this, *storage()};

  sleep_log->set_client(client, cb);

  int ret;

  if (!cur_diag_trans()) {
    ret = sleep_log->execute();
    switch (ret) {
      case Transaction::TRANS_E_STARTED:
        enqueue(sleep_log);
        trans = sleep_log;
        break;
      case Transaction::TRANS_E_SUCCESS:
        trans = sleep_log;
        break;
      default:  // TRANS_E_ERROR
        delete sleep_log;
        break;
    }
  } else {  // The diag device is busy now, so enqueue it.
    enqueue(sleep_log);
    trans = sleep_log;
    ret = Transaction::TRANS_E_STARTED;
  }

  return ret;
}

void WanModemLogHandler::trans_finished(Transaction* trans) {
  TransModem* mt = static_cast<TransModem*>(trans);
  if (TransModem::START_EVENT_LOG == mt->type()) {
    if (Transaction::TRANS_R_SUCCESS == mt->result()) {
      at_ctrl_.register_events(this, modem_event_notify);
      if (event_list_file_.expired()) {
        event_list_file_ = storage()->create_file(LogString{"event_list.txt"},
                                                  LogFile::LT_UNKNOWN);
      }
    } else {
      at_ctrl_.unregister_events();
    }
  }

  TransactionManager::trans_finished(trans);

  // Start the next diag transaction
  auto it = transactions_.begin();
  while (it != transactions_.end()) {
    TransModem* mt = static_cast<TransModem*>(*it);
    if (Transaction::TS_NOT_BEGIN == mt->state()) {
      int err = mt->execute();
      if (Transaction::TRANS_E_STARTED == err) {  // Started
        break;
      }

      it = transactions_.erase(it);
      if (mt->has_client()) {
        mt->notify_result();
      } else {
        delete mt;
      }
    } else {  // Transaction started or finished.
      ++it;
    }
  }
}

int WanModemLogHandler::set_agdsp_log_dest(LogConfig::AgDspLogDestination dest) {
  return cmd_ctrl_.set_agdsp_log_dest(dest);
}

int WanModemLogHandler::set_agdsp_pcm_dump(bool pcm) {
  return cmd_ctrl_.set_agdsp_pcm_dump(pcm);
}

int WanModemLogHandler::set_miniap_log_status(bool status) {
  m_mini_ap_log = status;
  return cmd_ctrl_.set_miniap_log_status(status);
}

int WanModemLogHandler::set_orca_log(LogString cmd, bool status) {
  if (!strcmp(ls2cstring(cmd), "SET_MINIAP_LOG")) {
    m_mini_ap_log = status;
    return cmd_ctrl_.set_miniap_log_status(status);
  } else if (!strcmp(ls2cstring(cmd), "SET_ORCADP_LOG")) {
    m_orcadp_log = status;
    return cmd_ctrl_.set_orcadp_log_status(status);
  }
  return 0;
}

int WanModemLogHandler::set_mipi_log_info(LogConfig::MipiLogList mipilog_list) {
  return cmd_ctrl_.set_mipi_log_info(mipilog_list);
}

int WanModemLogHandler::crash() {
  return at_ctrl_.crash();
}

int WanModemLogHandler::start_normal_log(void* client,
                                         Transaction::ResultCallback cb,
                                         TransStartNormalLog*& trans) {
  int ret;
  TransStartNormalLog* normal_log = new TransStartNormalLog{this};

  normal_log->set_client(client, cb);
  if (number()) {  // Transaction executing
    enqueue(normal_log);
    trans = normal_log;
    ret = Transaction::TRANS_E_STARTED;
  } else {
    ret = normal_log->execute();
    switch (ret) {
      case Transaction::TRANS_E_SUCCESS:
        trans = normal_log;
        break;
      case Transaction::TRANS_E_STARTED:
        enqueue(normal_log);
        trans = normal_log;
        break;
      default:
        delete normal_log;
        break;
    }
  }

  return ret;
}

int WanModemLogHandler::start_event_log(void* client,
                                        Transaction::ResultCallback cb,
                                        TransStartEventLog*& trans) {
  int ret;
  TransStartEventLog* start_evt = new TransStartEventLog{this};

  start_evt->set_client(client, cb);
  if (number()) {  // Transaction executing
    enqueue(start_evt);
    trans = start_evt;
    ret = Transaction::TRANS_E_STARTED;
  } else {
    ret = start_evt->execute();
    switch (ret) {
      case Transaction::TRANS_E_STARTED:
        enqueue(start_evt);
        trans = start_evt;
        break;
      case Transaction::TRANS_E_SUCCESS:
        trans = start_evt;
        if (event_list_file_.expired()) {
          event_list_file_ = storage()->create_file(LogString{"event_list.txt"},
                                                    LogFile::LT_UNKNOWN);
        }
        break;
      default:
        delete start_evt;
        break;
    }
  }

  return ret;
}

int WanModemLogHandler::stop_log(void* client,
                                 Transaction::ResultCallback cb,
                                 TransStopCellularLog*& trans) {
  int ret;
  TransStopCellularLog* stop_log = new TransStopCellularLog{this};

  stop_log->set_client(client, cb);

  size_t tnum = number();
  bool run_now{};

  if (1 == tnum) {
    TransModem* t = static_cast<TransModem*>(transactions_.front());
    if (TransModem::VERSION_QUERY == t->type()) {
      remove(t);

      TransWanModemVerQuery* tv = static_cast<TransWanModemVerQuery*>(t);

      tv->cancel();
      delete tv;
      m_ver_state = MVS_NOT_BEGIN;

      info_log("MODEM version query cancelled");

      run_now = true;
    }
  } else if (!tnum) {
    run_now = true;
  }

  if (run_now) {  // Can run now.
    ret = stop_log->execute();
    switch (ret) {
      case Transaction::TRANS_E_SUCCESS:
        trans = stop_log;
        break;
      case Transaction::TRANS_E_STARTED:
        enqueue(stop_log);
        trans = stop_log;
        break;
      default:
        delete stop_log;
        break;
    }
  } else {
    enqueue(stop_log);
    trans = stop_log;
    ret = Transaction::TRANS_E_STARTED;
  }

  return ret;
}

void WanModemLogHandler::change_log_mode(LogConfig::LogMode mode) {
  if (log_mode() != mode) {
    switch (mode) {
      case LogConfig::LM_OFF:
        if (at_ctrl_.fd() >= 0) {
          at_ctrl_.close_unreg();
        }
        if (LogConfig::LM_NORMAL == log_mode()) {
          WanModemLogHandler::stop();
        }
        break;
      case LogConfig::LM_NORMAL:
        if (at_ctrl_.fd() >= 0) {
          at_ctrl_.close_unreg();
        }
        WanModemLogHandler::start();
        break;
      default:  // LogConfig::LM_EVENT
        if (LogConfig::LM_NORMAL == log_mode()) {
          WanModemLogHandler::stop();
        }
        break;
    }
    set_log_mode(mode);
  }
}

void WanModemLogHandler::modem_event_notify(void* client,
                                            int sim,
                                            int event) {
  info_log("MODEM event %d SIM %d", event, sim);

  WanModemLogHandler* modem = static_cast<WanModemLogHandler*>(client);

  if (LogConfig::LM_EVENT != modem->log_mode()) {
    err_log("MODEM not in event log state");
    return;
  }

  if (!modem->storage()) {
    if (modem->WanModemLogHandler::create_storage()) {
      return;
    }
  }

  CalendarTime ct;
  struct timeval tnow;

  if (get_calendar_time(tnow, ct)) {
    err_log("get calendar time error");
    return;
  }

  size_t tnum = modem->number();

  if (auto evt_file = modem->event_list_file_.lock()) {
    char buf[128];

    int n = snprintf(buf, sizeof buf,
                     "%04d-%02d-%02d %02d:%02d:%02d.%03d %d SIM%d %d%s\n",
                     ct.year, ct.mon, ct.day,
                     ct.hour, ct.min, ct.sec, ct.msec, ct.lt_diff,
                     sim, event,
                     tnum >= 20 ? " (event queue full)" : "");
    if (n > 0) {
      evt_file->write(buf, n);
    }
  }
  if (tnum >= 20) {
    err_log("too many pending transactions, drop this event");
    return;
  }

  bool has_ta{};
  uint32_t modem_cnt;

  if (!modem->get_cur_modem_cnt(modem_cnt)) {
    has_ta = true;
  }

  TransSaveEventLog* evt_log = new TransSaveEventLog{modem,
                                                     *modem->storage(),
                                                     sim, event, ct};
  evt_log->set_client(modem, event_log_result);
  if (has_ta) {
    tnow.tv_sec += ct.lt_diff;
    evt_log->set_time_alignment(tnow, modem_cnt);
  }
  if (!tnum) {
    int err = evt_log->execute();
    switch (err) {
      case Transaction::TRANS_E_STARTED:
        modem->enqueue(evt_log);
        break;
      default:  // TRANS_E_ERROR
        delete evt_log;
        err_log("start event log trans error");
        break;
    }
  } else {  // Transaction queue not empty
    modem->enqueue(evt_log);
  }
}

void WanModemLogHandler::parse_lib_result(void* client, Transaction* trans) {
  if (Transaction::TRANS_R_SUCCESS != trans->result()) {
    err_log("Modem parse lib save failed");
  }

  delete trans;
  trans = nullptr;
}

void WanModemLogHandler::event_log_result(void* client, Transaction* trans) {
  if (Transaction::TRANS_R_SUCCESS != trans->result()) {
    TransSaveEventLog* evt_log = static_cast<TransSaveEventLog*>(trans);
    err_log("event %d (SIM %d) log save failed",
            evt_log->event(), evt_log->sim());
  }
  delete trans;
}

int WanModemLogHandler::save_last_log(void* client,
                                      Transaction::ResultCallback cb,
                                      TransModemLastLog*& trans) {
  int ret;
  TransModemLastLog* last_log = new TransModemLastLog{this};

  last_log->set_client(client, cb);
  if (number()) {  // Transaction executing
    enqueue(last_log);
    trans = last_log;
    ret = Transaction::TRANS_E_STARTED;
  } else {
    ret = last_log->execute();
    switch (ret) {
      case Transaction::TRANS_E_STARTED:
        enqueue(last_log);
        trans = last_log;
        break;
      case Transaction::TRANS_E_SUCCESS:
        trans = last_log;
        break;
      default:
        delete last_log;
        break;
    }
  }

  return ret;
}

int WanModemLogHandler::sync_modem_time(void* client,
                                        Transaction::ResultCallback cb,
                                        TransModemTimeSync*& trans) {
  int ret;
  TransModemTimeSync* time_sync = new TransModemTimeSync{this};

  time_sync->set_client(client, cb);
  if (number()) {  // Transaction executing
    enqueue(time_sync);
    trans = time_sync;
    ret = Transaction::TRANS_E_STARTED;
  } else {
    ret = time_sync->execute();
    switch (ret) {
      case Transaction::TRANS_E_STARTED:
        enqueue(time_sync);
        trans = time_sync;
        break;
      default:  // TRANS_E_ERROR
        delete time_sync;
        err_log("start time sync trans error");
        break;
    }
  }

  return ret;
}

void WanModemLogHandler::flush() {
  if (auto evt_file = event_list_file_.lock()) {
    evt_file->flush();
  }
  LogPipeHandler::flush();
}

int WanModemLogHandler::get_cur_modem_cnt(uint32_t& cnt) const {
  int ret{-1};
  struct sysinfo sinfo;
  time_sync ts;

  if (time_sync_mgr_ && time_sync_mgr_->get_time_sync_info(ts)) {
    sysinfo(&sinfo);
    cnt = ts.sys_cnt + (sinfo.uptime - ts.uptime) * 1000;
    ret = 0;
  }

  return ret;
}
