/*
 *  orca_miniap_log.cpp - The ORCA miniap log handler class.
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
#include "orca_miniap_log.h"
#include "cp_stor.h"
#ifdef MODEM_TALIGN_AT_CMD_
#include "wan_modem_time_sync_atcmd.h"
#else
#include "wan_modem_time_sync_refnotify.h"
#endif

OrcaMiniapLogHandler::OrcaMiniapLogHandler(LogController* ctrl, Multiplexer* multi,
                                       const LogConfig::ConfigEntry* conf,
                                       StorageManager& stor_mgr, CpClass cp_class, WanModemLogHandler* wm)
  : WanModemLogHandler(ctrl, multi, conf, stor_mgr, nullptr, cp_class),
    wan_handler_{wm}{
}

OrcaMiniapLogHandler::~OrcaMiniapLogHandler() { stop(); }

void OrcaMiniapLogHandler::on_new_log_file(LogFile* f) {
  //save_timestamp(f);

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
void OrcaMiniapLogHandler::check_version_update(void* param) {
  OrcaMiniapLogHandler* ml = static_cast<OrcaMiniapLogHandler*>(param);
  if (!ml->wan_handler_) {
    err_log("WanModemLogHandler is NULL");
    return;
  }
  WanModemLogHandler::ModemVerState vs = ml->wan_handler_->get_md_ver_state();
  info_log("ModemVerState is %d in miniap", vs);
  if (vs == WanModemLogHandler::MVS_GOT) {
    ml->m_ver_state = WanModemLogHandler::MVS_GOT;
    ml->m_modem_ver = ml->wan_handler_->get_md_ver();
    info_log("m_modem_ver is %s in miniap", ls2cstring(ml->m_modem_ver));
    ml->correct_ver_info();
  } else {
    ml->multiplexer()->timer_mgr().create_timer(10 * 1000,
                                                check_version_update,
                                                param);
  }
}

int OrcaMiniapLogHandler::start(LogConfig::LogMode mode) {
  if (start_logging()) {  // Start failed
    return -1;
  }
  multiplexer()->timer_mgr().create_timer(10 * 1000,
                                          check_version_update,
                                          this);
  return 0;
}

int OrcaMiniapLogHandler::stop() {
  info_log("OrcaMiniapLogHandler::stop() enter ");
  return LogPipeHandler::stop_logging();
}
