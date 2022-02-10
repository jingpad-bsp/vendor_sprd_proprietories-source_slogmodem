/*
 *  orca_dp_log.cpp - The ORCA miniap log handler class.
 *
 *  Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 *  History:
 *  2020-3-18 Chunlan Wang
 *  Initial version.
 */

#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include "client_mgr.h"
#include "diag_dev_hdl.h"
#include "log_config.h"
#include "log_ctrl.h"
#include "orca_dp_log.h"
#include "cp_stor.h"
#ifdef MODEM_TALIGN_AT_CMD_
#include "wan_modem_time_sync_atcmd.h"
#else
#include "wan_modem_time_sync_refnotify.h"
#endif

OrcaDpLogHandler::OrcaDpLogHandler(LogController* ctrl, Multiplexer* multi,
                                       const LogConfig::ConfigEntry* conf,
                                       StorageManager& stor_mgr, CpClass cp_class, WanModemLogHandler* wm)
  : WanModemLogHandler(ctrl, multi, conf, stor_mgr, nullptr, cp_class),
    wan_handler_{wm},
    m_timestamp_miss{true}{
      if (wm) {
        time_sync_mgr_ = wm->get_time_sync_mgr();
      }
    }

OrcaDpLogHandler::~OrcaDpLogHandler() {
}

void OrcaDpLogHandler::on_new_log_file(LogFile* f) {
  save_timestamp(f);

  // MODEM version info
  uint8_t* buf = nullptr;
  // resulted length to be written
  size_t ver_len = 0;
  if (MVS_GOT == m_ver_state) {
    buf = frame_version_info(reinterpret_cast<uint8_t*>(const_cast<char*>(
                                 ls2cstring(m_modem_ver))),
                             m_modem_ver.length(), 0, ver_len);
    m_ver_stor_state = WanModemLogHandler::MVSS_SAVED;
  } else {
    info_log("no MODEM version when log file is created");
    buf = frame_noversion_and_reserve(ver_len);
    m_reserved_version_len = ver_len;
    m_ver_stor_state = WanModemLogHandler::MVSS_NOT_SAVED;
  }

  ssize_t nwr = f->write_raw(buf, ver_len);
  delete [] buf;

  if (nwr > 0) {
    f->add_size(nwr);
  }
}

void OrcaDpLogHandler::check_version_update(void* param) {
  OrcaDpLogHandler* ml = static_cast<OrcaDpLogHandler*>(param);
  if (!ml->wan_handler_) {
    err_log("WanModemLogHandler is NULL");
    return;
  }
  WanModemLogHandler::ModemVerState vs = ml->wan_handler_->get_md_ver_state();
  if (vs == WanModemLogHandler::MVS_GOT) {
    ml->m_ver_state = WanModemLogHandler::MVS_GOT;
    ml->m_modem_ver = ml->wan_handler_->get_md_ver();
    ml->flush();
    info_log("m_modem_ver is %s in cm4", ls2cstring(ml->m_modem_ver));
    ml->correct_ver_info();
  } else {
    ml->multiplexer()->timer_mgr().create_timer(10 * 1000,
                                                check_version_update,
                                                param);
  }
}

bool OrcaDpLogHandler::save_timestamp(LogFile* f) {
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

void OrcaDpLogHandler::check_time_update(void* param) {
  OrcaDpLogHandler* dl = static_cast<OrcaDpLogHandler*>(param);
  if (dl->m_timestamp_miss) {
    time_sync ts;
    modem_timestamp modem_ts = {0, 0, 0, 0};
    if (!dl->time_sync_mgr_) {
      err_log("time_sync_mgr_ is NULL");
      return;
    }
    if (dl->time_sync_mgr_->get_time_sync_info(ts)) {
      info_log("get_time_sync_info");
      calculate_modem_timestamp(ts, modem_ts);
    } else {
      dl->multiplexer()->timer_mgr().create_timer(10 * 1000,
                                                  check_time_update,
                                                  param);
      err_log("Wan modem timestamp is not available.");
      return;
    }
    CpStorage* stor = dl->storage();
    if (stor->current_file_saving()) {
      DataBuffer* buf = stor->get_buffer();
      if (buf) {
        memcpy(buf->buffer, &modem_ts, sizeof modem_ts);
        buf->data_len = sizeof modem_ts;
        buf->dst_offset = 0;
        if (stor->amend_current_file(buf)) {
          dl->m_timestamp_miss = false;
          info_log("ORCA AP time stamp is corrected.");
        } else {
          stor->free_buffer(buf);
          err_log("amend_current_file error");
        }
      }
    }
  }
}

int OrcaDpLogHandler::start(LogConfig::LogMode mode) {
  if (LogPipeHandler::start_logging()) {  // Start failed
    return -1;
  }
  multiplexer()->timer_mgr().create_timer(10 * 1000,
                                          check_version_update,
                                          this);
  multiplexer()->timer_mgr().create_timer(10 * 1000,
                                          check_time_update,
                                          this);
  return 0;
}

int OrcaDpLogHandler::stop() {
  info_log("OrcaCm4LogHandler::stop() enter ");
  return LogPipeHandler::stop_logging();
}
