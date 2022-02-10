/*
 *  trans_normal_log.cpp - The normal log start transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-6-20 Zhang Ziyi
 *  Initial version.
 */

#include "trans_normal_log.h"
#include "wan_modem_log.h"

TransStartNormalLog::TransStartNormalLog(WanModemLogHandler* modem)
    :TransModem{modem, START_NORMAL_LOG},
     wan_modem_{modem} {}

TransStartNormalLog::~TransStartNormalLog() {
  TransStartNormalLog::cancel();
}

int TransStartNormalLog::execute() {
  LogConfig::LogMode cur_mode = wan_modem_->log_mode();

  if (LogConfig::LM_NORMAL == cur_mode) {
    on_finished(TRANS_R_SUCCESS);
    return TRANS_E_SUCCESS;
  }

  if (LogConfig::LM_OFF == cur_mode) {
    int res {TRANS_R_FAILURE};
    if (!wan_modem_->start()) {
      res = TRANS_R_SUCCESS;
    }
    on_finished(res);
    return TRANS_E_SUCCESS;
  }

  // Stop event log
  ModemAtController& at_ctrl = wan_modem_->at_controller();
  int ret = at_ctrl.start_unsubscribe_event(this, unsubscribe_result);
  int res {TRANS_R_FAILURE};

  switch (ret) {
    case 0:  // Started
      on_started();
      ret = TRANS_E_STARTED;
      break;
    case 1:  // Finished successfully
      if (!wan_modem_->start()) {
        res = TRANS_R_SUCCESS;
      }
      on_finished(res);
      ret = TRANS_E_SUCCESS;
      break;
    default:  // Error
      on_finished(TRANS_R_FAILURE);
      ret = TRANS_E_SUCCESS;
      break;
  }

  return ret;
}

void TransStartNormalLog::cancel() {
  if (TS_EXECUTING == state()) {
    ModemAtController& at_ctrl = wan_modem_->at_controller();
    at_ctrl.cancel_trans();

    on_canceled();
  }
}

void TransStartNormalLog::unsubscribe_result(void* client,
    ModemAtController::ActionType act, int result) {
  TransStartNormalLog* trans = static_cast<TransStartNormalLog*>(client);
  int res {TRANS_R_FAILURE};

  if (ModemAtController::RESULT_OK == result) {
    if (!trans->wan_modem_->start()) {
      res = TRANS_R_SUCCESS;
    }
  }

  trans->finish(res);
}
