/*
 *  last_log_req.cpp - last log saving request class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-11-17 Zhang Ziyi
 *  Initial version.
 */

#include "last_log_req.h"

int LastLogRequest::do_request() {
  uint8_t cmd[32];
  size_t len;

  memcpy(cmd, "SAVE_LAST_LOG ", 14);
  if (CT_5MODE == subsys_) {
    memcpy(cmd + 14, "MODEM\n", 6);
    len = 20;
  } else {
    memcpy(cmd + 14, "WCN\n", 4);
    len = 18;
  }

  int err = send_req(cmd, len);

  if (err) {
    report_error(RE_SEND_CMD_ERROR);
    return -1;
  }

  return wait_simple_response(DEFAULT_RSP_TIME);
}
