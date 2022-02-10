/*
 *  en_evt_req.cpp - enable event log request class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-6-9 Zhang Ziyi
 *  Initial version.
 */

#include "en_evt_req.h"

EnableEventLogRequest::EnableEventLogRequest(CpType type)
    :subsys_{type} {}

int EnableEventLogRequest::do_request() {
  uint8_t cmd[32];

  memcpy(cmd, "ENABLE_EVT_LOG ", 15);
  if (CT_5MODE == subsys_) {
    memcpy(cmd + 15, "5MODE\n", 6);
  } else {
    memcpy(cmd + 15, "WCDMA\n", 6);
  }

  int err = send_req(cmd, 21);
  if (err) {
    report_error(RE_SEND_CMD_ERROR);
    return -1;
  }

  return wait_simple_response(DEFAULT_RSP_TIME);
}
