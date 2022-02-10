/*
 *  set_cp_max_size.cpp - set storage's max size for log saving.
 *
 *  Copyright (C) 2016 Spreadtrum Communications Inc.
 *
 *  History:
 *  2016-11-2 YAN Zhihang
 *  Initial version.
 */

#include "set_cp_max_size.h"

SetCpMaxSize::SetCpMaxSize(CpType ct, MediaType mt, unsigned size)
    : m_type{ct},
      mt_{mt},
      m_size{size} {}

int SetCpMaxSize::do_request() {
  uint8_t buf[64];

  memcpy(buf, "SET_CP_LOG_SIZE ", 16);

  uint8_t* p = buf + 16;
  size_t rlen = sizeof buf - 16;
  size_t tlen = 0;
  int err = put_cp_type(p, rlen, m_type, tlen);

  if (err || (rlen == tlen)) {
    fprintf(stderr,
            "Can not make the subsystem parameter of SET_CP_LOG_SIZE\n");
    return -1;
  }

  p += tlen;
  size_t len;

  if (MT_INTERNAL == mt_) {
    len = snprintf(reinterpret_cast<char*>(p), rlen - tlen, " internal %u\n", m_size);
  } else {
    len = snprintf(reinterpret_cast<char*>(p), rlen - tlen, " external %u\n", m_size);
  }

  err = send_req(buf, tlen + 16 + len);

  if (err) {
    report_error(RE_SEND_CMD_ERROR);
    return -1;
  }

  return wait_simple_response(DEFAULT_RSP_TIME);
}
