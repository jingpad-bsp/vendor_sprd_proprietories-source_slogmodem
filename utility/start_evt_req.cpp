/*
 *  start_evt_req.cpp - start event log request class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-6-16 Zhang Ziyi
 *  Initial version.
 */

#include "start_evt_req.h"

int StartEventLogRequest::do_request() {
  uint8_t cmd[] = "ENABLE_EVT_LOG 5MODE\n";

  int err = send_req(cmd, 21);

  if (err) {
    report_error(RE_SEND_CMD_ERROR);
    return -1;
  }

  return wait_simple_response(DEFAULT_RSP_TIME);
}
