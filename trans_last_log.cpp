/*
 *  trans_last_col.cpp - The last log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-8-9 Zhang Ziyi
 *  Initial version.
 */

#include "trans_last_log.h"

TransModemLastLog::TransModemLastLog(WanModemLogHandler* wan)
    :TransModem{wan, SAVE_MODEM_LAST_LOG},
     wan_modem_{wan},
     last_log_timer_{} {}

TransModemLastLog::~TransModemLastLog() {
  TransModemLastLog::cancel();
}

int TransModemLastLog::execute() {
  if (LogConfig::LM_NORMAL != wan_modem_->log_mode() ||
      LogPipeHandler::CWS_NOT_WORKING == wan_modem_->cp_state()) {
    on_finished(TRANS_R_SUCCESS);
    return TRANS_E_SUCCESS;
  }

  int ret{TRANS_E_SUCCESS};
  TimerManager& tmgr = wan_modem_->multiplexer()->timer_mgr();
  last_log_timer_ = tmgr.create_timer(MODEM_LAST_LOG_PERIOD,
                                      last_log_timeout, this);
  if (last_log_timer_) {
    on_started();
    ret = TRANS_E_STARTED;
  } else {
    on_finished(TRANS_R_FAILURE);
  }

  return ret;
}

void TransModemLastLog::last_log_timeout(void* client) {
  TransModemLastLog* last_log = static_cast<TransModemLastLog*>(client);

  last_log->last_log_timer_ = nullptr;
  last_log->wan_modem_->flush();

  last_log->finish(TRANS_R_SUCCESS);
}

void TransModemLastLog::cancel() {
  if (TS_EXECUTING == state()) {
    TimerManager& tmgr = wan_modem_->multiplexer()->timer_mgr();
    tmgr.del_timer(last_log_timer_);
    last_log_timer_ = nullptr;
    on_canceled();
  }
}
