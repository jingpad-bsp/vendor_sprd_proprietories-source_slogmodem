/*
 *  set_log_file_size.cpp - set log file's max size.
 *
 *  Copyright (C) 2016 Spreadtrum Communications Inc.
 *
 *  History:
 *  2016-11-4 YAN Zhihang
 *  Initial version.
 */

#include "set_log_file_size.h"

SetLogFileSize::SetLogFileSize(CpType type, unsigned size)
    : type_{type},
      m_size{size} {}

int SetLogFileSize::do_request() {
  // SET_LOG_FILE_SIZE <cp name> <file size>
  uint8_t buf[64];

  memcpy(buf, "SET_LOG_FILE_SIZE ", 18);

  uint8_t* p = buf + 18;
  size_t rlen = sizeof buf - 18;
  size_t tlen = 0;
  int err = put_cp_type(p, rlen, type_, tlen);

  if (err || (rlen == tlen)) {
    fprintf(stderr,
            "Can not make the subsystem parameter of SET_LOG_FILE_SIZE\n");
    return -1;
  }

  p += tlen;

  size_t len = snprintf(reinterpret_cast<char*>(p),
                        sizeof buf - 18 - tlen, " %u\n", m_size);

  err = send_req(buf, 18 + tlen + len);

  if (err) {
    report_error(RE_SEND_CMD_ERROR);
    return -1;
  }

  return wait_simple_response(DEFAULT_RSP_TIME);
}
