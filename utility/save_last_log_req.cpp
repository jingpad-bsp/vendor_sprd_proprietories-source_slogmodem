/*
 *  save_last_log.cpp - save last log request class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-9-4 YAN Zhihang
 *  Initial version.
 */

#include "save_last_log_req.h"

int SaveLastLogRequest::do_request() {
  uint8_t cmd[] = "SAVE_LAST_LOG MODEM\n";

  int err = send_req(cmd, 20);

  if (err) {
    report_error(RE_SEND_CMD_ERROR);
    return -1;
  }

  return wait_simple_response(DEFAULT_RSP_TIME);
}
