/*
 *  set_orca_dp.cpp - set orca dp request class.
 *
 *  Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 *  History:
 *  2020-4-4 Chunlan.Wang
 *  Initial version.
 */

#include "set_orca_dp.h"

SetOrcaDp::SetOrcaDp(bool orcadp_on)
    :m_orcadp_on{orcadp_on} {}

int SetOrcaDp::do_request() {
  uint8_t buf[32] = {0};
  size_t len;

  memcpy(buf, "SET_ORCADP_LOG ", 15);
  if (m_orcadp_on) {
    memcpy(buf + 15, "ON\n", 3);
    len = 25;
  } else {
    memcpy(buf + 15, "OFF\n", 4);
    len = 26;
  }
  err_log("SetOrcaDp::do_request : %s", buf);
  if (send_req(buf, len)) {
    report_error(RE_SEND_CMD_ERROR);
    return -1;
  }

  return wait_simple_response(DEFAULT_RSP_TIME);
}
