/*
 *  log_ctrl_agdsp.cpp - The AGDSP log control functions.
 *
 *  Copyright (C) 2015-2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-11-13 Zhang Ziyi
 *  Initial version.
 */

#include "agdsp_log.h"
#include "log_ctrl.h"
#include "wan_modem_log.h"

LogConfig::AgDspLogDestination LogController::get_agdsp_log_dest() const {
  return m_config->agdsp_log_dest();
}

int LogController::set_agdsp_log_dest(LogConfig::AgDspLogDestination dest) {
  info_log("set AG-DSP log output %d", static_cast<int>(dest));

  LogList<LogPipeHandler*>::iterator it =
      find_log_handler(m_log_pipes, CT_AGDSP);
  int ret = LCR_NO_AGDSP;

  if (it != m_log_pipes.end()) {
    AgDspLogHandler* p = static_cast<AgDspLogHandler*>(*it);
    if (!str_empty(p->log_dev_path())) {
      p->set_log_dest(dest);
      m_config->set_agdsp_log_dest(dest);
      if (m_config->dirty()) {
        m_config->save();
      }

      ret = LCR_SUCCESS;
    }
  }

  it = find_log_handler(m_log_pipes, CLASS_MODEM);
  if (it != m_log_pipes.end()) {
    WanModemLogHandler* p = static_cast<WanModemLogHandler*>(*it);
    p->set_agdsp_log_dest(dest);
  }


  return ret;
}

bool LogController::get_agdsp_pcm_dump() const {
  return m_config->agdsp_pcm_dump_enabled();
}

int LogController::set_agdsp_pcm_dump(bool pcm) {
  info_log("set AG-DSP PCM dump %s", pcm ? "ON" : "OFF");

  LogList<LogPipeHandler*>::iterator it =
      find_log_handler(m_log_pipes, CT_AGDSP);
  int ret = LCR_NO_AGDSP;

  if (it != m_log_pipes.end()) {
    AgDspLogHandler* p = static_cast<AgDspLogHandler*>(*it);
    if (!str_empty(p->log_dev_path())) {
      p->set_pcm_dump(pcm);
      m_config->enable_agdsp_pcm_dump(pcm);
      if (m_config->dirty()) {
        m_config->save();
      }

      ret = LCR_SUCCESS;
    }
  }

  it = find_log_handler(m_log_pipes, CLASS_MODEM);
  if (it != m_log_pipes.end()) {
    WanModemLogHandler* p = static_cast<WanModemLogHandler*>(*it);
    p->set_agdsp_pcm_dump(pcm);
  }


  return ret;
}
