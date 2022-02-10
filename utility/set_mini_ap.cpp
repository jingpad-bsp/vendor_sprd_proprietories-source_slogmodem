/*
 *  set_mini_ap.cpp - set mini ap request class.
 *
 *  Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 *  History:
 *  2020-1-20 Chunlan.Wang
 *  Initial version.
 */

#include "set_mini_ap.h"

SetMiniAp::SetMiniAp(bool miniap_on)
    :m_miniap_on{miniap_on} {}

int SetMiniAp::do_request() {
  uint8_t buf[32] = {0};
  size_t len;

  memcpy(buf, "SET_MINIAP_LOG ", 15);
  if (m_miniap_on) {
    memcpy(buf + 15, "ON\n", 3);
    len = 25;
  } else {
    memcpy(buf + 15, "OFF\n", 4);
    len = 26;
  }
  info_log("SetMiniAp::do_request : %s", buf);
  if (send_req(buf, len)) {
    report_error(RE_SEND_CMD_ERROR);
    return -1;
  }

  return wait_simple_response(DEFAULT_RSP_TIME);
}
