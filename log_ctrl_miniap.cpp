/*
 *  log_ctrl_miniap.cpp - The miniap log control functions.
 *
 *  Copyright (C) 2020-2022 Unisoc Communications Inc.
 *
 *  History:
 *
 *  2020-01-20 Chunlan.Wang
 *  Initial version.
 */

#include "log_ctrl.h"
#include "wan_modem_log.h"

int LogController::set_miniap_log_status(bool status) {
  int ret = LCR_NO_MINIAP;
  info_log("set miniAp log status %d", static_cast<int>(status));
  LogList<LogPipeHandler*>::iterator it;
  it = find_log_handler(m_log_pipes, CLASS_MIX);
  if (it == m_log_pipes.end()) {
    it = find_log_handler(m_log_pipes, CLASS_MODEM);
  }
  if (it != m_log_pipes.end()) {
    info_log("get modem handler");
    WanModemLogHandler* p = static_cast<WanModemLogHandler*>(*it);
    p->set_miniap_log_status(status);

    ret = LCR_SUCCESS;
  } else {
    err_log("cannot get modem handler");
  }

  return ret;
}

int LogController::set_orca_log(LogString cmd, bool status) {
  int ret = LCR_NO_ORCA;
  info_log("set_orca_log %d", static_cast<int>(status));
  LogList<LogPipeHandler*>::iterator it;
  it = find_log_handler(m_log_pipes, CLASS_MIX);
  if (it == m_log_pipes.end()) {
    it = find_log_handler(m_log_pipes, CLASS_MODEM);
  }
  if (it != m_log_pipes.end()) {
    info_log("get modem handler");
    WanModemLogHandler* p = static_cast<WanModemLogHandler*>(*it);
    p->set_orca_log(cmd, status);

    ret = LCR_SUCCESS;
  } else {
    err_log("cannot get modem handler");
  }

  return ret;
}
