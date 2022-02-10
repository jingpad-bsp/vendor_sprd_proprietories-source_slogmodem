/*
 *  trans_stop_wcn_log.cpp - The WCN log stop transaction class.
 *
 *  Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2018-5-14 Zhang Ziyi
 *  Initial version.
 */

#include "ext_wcn_log.h"
#include "trans_stop_wcn_log.h"

TransStopWcnLog::TransStopWcnLog(ExtWcnLogHandler* wcn)
    :TransModem{wcn, STOP_WCN_LOG},
     wcn_{wcn} {}

TransStopWcnLog::~TransStopWcnLog() {
  TransStopWcnLog::cancel();
}

int TransStopWcnLog::execute() {
  int ret;
  int res;

  if (wcn_->stop()) {
    ret = TRANS_E_ERROR;
    res = TRANS_R_FAILURE;
  } else {
    ret = TRANS_E_SUCCESS;
    res = TRANS_R_SUCCESS;
  }

  on_finished(res);

  return ret;
}

void TransStopWcnLog::cancel() {
  on_canceled();
}
