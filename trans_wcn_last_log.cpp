/*
 *  trans_wcn_last_col.cpp - The WCN last log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-11-16 Zhang Ziyi
 *  Initial version.
 */

#include "log_ctrl.h"
#include "trans_wcn_last_log.h"

TransWcnLastLog::TransWcnLastLog(LogController* ctrl, LogPipeHandler* wcn)
    :TransGlobal{ctrl, WCN_LAST_LOG},
     log_ctrl_{ctrl},
     wcn_{wcn},
     last_log_timer_{} {}

TransWcnLastLog::~TransWcnLastLog() {
  TransWcnLastLog::cancel();
}

int TransWcnLastLog::execute() {
  if (LogConfig::LM_NORMAL != wcn_->log_mode()) {
    wcn_->flush();

    on_finished(TRANS_R_SUCCESS);

    return TRANS_E_SUCCESS;
  }

  TimerManager& tmgr = wcn_->multiplexer()->timer_mgr();
  last_log_timer_ = tmgr.create_timer(1000, last_log_timeout, this);

  on_started();

  return TRANS_E_STARTED;
}

void TransWcnLastLog::cancel() {
  if (TS_EXECUTING == state()) {
    TimerManager& tmgr = wcn_->multiplexer()->timer_mgr();
    tmgr.del_timer(last_log_timer_);
    last_log_timer_ = nullptr;

    on_canceled();
  }
}

void TransWcnLastLog::last_log_timeout(void* client) {
  TransWcnLastLog* last_log = static_cast<TransWcnLastLog*>(client);

  last_log->last_log_timer_ = nullptr;
  last_log->wcn_->flush();

  last_log->finish(TRANS_R_SUCCESS);
}
