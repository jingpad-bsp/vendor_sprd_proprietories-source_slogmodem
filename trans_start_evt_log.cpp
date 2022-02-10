/*
 *  trans_start_evt_log.cpp - The event triggered log start transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-5-26 Zhang Ziyi
 *  Initial version.
 */

#include "log_ctrl.h"
#include "trans_start_evt_log.h"
#include "wan_modem_log.h"

TransStartEventLog::TransStartEventLog(WanModemLogHandler* modem)
    :TransModem{modem, START_EVENT_LOG},
     wan_modem_{modem} {}

TransStartEventLog::~TransStartEventLog() {
  TransStartEventLog::cancel();
}

int TransStartEventLog::execute() {
  if (LogConfig::LM_EVENT == wan_modem_->log_mode()) {
    on_finished(TRANS_R_SUCCESS);
    return TRANS_E_SUCCESS;
  }

  ModemAtController& at_ctrl {wan_modem_->at_controller()};
  int ret = at_ctrl.start_subscribe_event(this, subscribe_result);

  switch (ret) {
    case 1:
      on_finished(TRANS_R_SUCCESS);
      ret = TRANS_E_SUCCESS;
      break;
    case 0:
      on_started();
      ret = TRANS_E_STARTED;
      break;
    default:
      on_finished(TRANS_R_FAILURE);
      ret = TRANS_E_SUCCESS;
      break;
  }

  return ret;
}

void TransStartEventLog::cancel() {
  if (TS_EXECUTING == state()) {
    wan_modem_->at_controller().cancel_trans();
  }
}

void TransStartEventLog::subscribe_result(void* client,
    ModemAtController::ActionType act, int result) {
  TransStartEventLog* start_evt = static_cast<TransStartEventLog*>(client);

  if (TS_EXECUTING == start_evt->state()) {
    int res {TRANS_R_FAILURE};

    if (!result) {
      start_evt->wan_modem_->change_log_mode(LogConfig::LM_EVENT);
      LogConfig* lc = start_evt->wan_modem_->controller()->config();

      lc->set_log_mode(start_evt->wan_modem_->type(),
                       LogConfig::LM_EVENT);
      lc->save();

      res = TRANS_R_SUCCESS;
    } else {
      err_log("subscribe event error %d", result);
    }

    start_evt->finish(res);
  }
}
