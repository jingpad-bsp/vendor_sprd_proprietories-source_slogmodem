/*
 *  log_ctrl_mipilog.cpp - The mipilog log control functions.
 *
 *  Copyright (C) 2020-2022 Unisoc Communications Inc.
 *
 *  History:
 *
 *  2020-03-04 Chunlan.Wang
 *  Initial version.
 */

#include "log_ctrl.h"
#include "wan_modem_log.h"

int LogController::set_mipi_log_info(LogString type, LogString channel, LogString freq) {
  int ret = LCR_NO_MIPI_LOG;
  info_log("set mipi log info %s %s %s", ls2cstring(type), ls2cstring(channel), ls2cstring(freq));
  LogList<LogPipeHandler*>::iterator it;
  it = find_log_handler(m_log_pipes, CLASS_MIX);
  if (it == m_log_pipes.end()) {
    it = find_log_handler(m_log_pipes, CLASS_MODEM);
  }
  if (it != m_log_pipes.end()) {
    info_log("get modem handler");
    WanModemLogHandler* p = static_cast<WanModemLogHandler*>(*it);
    LogConfig::MipiLogEntry* mipilog = new LogConfig::MipiLogEntry{
                    type,
                    channel,
                    freq};
    LogConfig::MipiLogList mipilist;
    mipilist.push_back(mipilog);
    ret = p->set_mipi_log_info(mipilist);
    if (!ret) {
      LogConfig::MipiLogList mipi_log_info = config()->get_mipi_log_info();

      for (auto c : mipi_log_info) {
        if (!memcmp(ls2cstring(type), ls2cstring(c->type), 7)) {
          ll_remove(mipi_log_info, c);
          delete c;
          break;
        }
      }

      mipi_log_info.push_back(mipilog);
      config()->set_mipi_log_info(mipi_log_info);
      m_config->save();
    }

  } else {
    err_log("cannot get modem handler");
  }


  return ret;
}
int LogController::get_mipi_log_info(LogString type, LogString& channel, LogString& freq) {
  int ret = LCR_SUCCESS;
  info_log("get mipi log info %s", ls2cstring(type));
  LogConfig::MipiLogList mipi_log_info = config()->get_mipi_log_info();
  for (auto c : mipi_log_info) {
    if (!memcmp(ls2cstring(type), ls2cstring(c->type), 7)) {
      channel = c->channel;
      freq = c->freq;
      break;
    }
  }
  return ret;
}
