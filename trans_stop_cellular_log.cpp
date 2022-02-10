/*
 *  trans_stop_cellular_log.cpp - The cellular log stop transaction class.
 *
 *  Copyright (C) 2017,2018 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-6-22 Zhang Ziyi
 *  Initial version.
 *
 *  2018-5-11 Zhang Ziyi
 *  Renamed to TransStopCellularLog.
 */

#include "trans_stop_cellular_log.h"
#include "wan_modem_log.h"

TransStopCellularLog::TransStopCellularLog(WanModemLogHandler* modem)
    :TransModem{modem, STOP_LOG},
     wan_modem_{modem} {}

TransStopCellularLog::~TransStopCellularLog() {
  TransStopCellularLog::cancel();
}

int TransStopCellularLog::execute() {
  LogConfig::LogMode cur_mode = wan_modem_->log_mode();

  if (LogConfig::LM_OFF == cur_mode) {
    on_finished(TRANS_R_SUCCESS);
    return TRANS_E_SUCCESS;
  }

  int res {TRANS_R_FAILURE};

  if (LogConfig::LM_NORMAL == cur_mode) {
    if (!wan_modem_->stop()) {
      res = TRANS_R_SUCCESS;
    }
    on_finished(res);
    return TRANS_E_SUCCESS;
  }

  // Stop event log
  ModemAtController& at_ctrl = wan_modem_->at_controller();
  int ret = at_ctrl.start_unsubscribe_event(this, unsubscribe_result);
  switch (ret) {
    case 0:  // Started
      on_started();
      ret = TRANS_E_STARTED;
      break;
    case 1:  // Finished successfully
      if (!wan_modem_->stop()) {
        res = TRANS_R_SUCCESS;
      }
      on_finished(TRANS_R_SUCCESS);
      ret = TRANS_E_SUCCESS;
      break;
    default:  // Error
      ret = TRANS_E_ERROR;
      break;
  }

  return ret;
}

void TransStopCellularLog::cancel() {
  if (TS_EXECUTING == state()) {
    ModemAtController& at_ctrl = wan_modem_->at_controller();
    at_ctrl.cancel_trans();

    on_canceled();
  }
}

void TransStopCellularLog::unsubscribe_result(void* client,
                                              ModemAtController::ActionType act,
                                              int result) {
  TransStopCellularLog* stop_log = static_cast<TransStopCellularLog*>(client);

  if (TS_EXECUTING == stop_log->state()) {
    int res {TRANS_R_FAILURE};

    if (ModemAtController::RESULT_OK == result) {
      if (!stop_log->wan_modem_->stop()) {
        res = TRANS_R_SUCCESS;
      }
    } else {
      err_log("unsubscribe event report failed");
    }
    stop_log->finish(res);
  } else {
    err_log("unsubscribe event result got at wrong state %d",
            stop_log->state());
  }
}
