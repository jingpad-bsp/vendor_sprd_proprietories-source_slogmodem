/*
 *  collect_req.cpp - log collection request class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-6-9 Zhang Ziyi
 *  Initial version.
 */

#include "collect_req.h"

CollectRequest::CollectRequest(const LogVector<CpType>& types)
    :sys_list_{types} {}

int CollectRequest::do_request() {
  size_t len = 0;
  uint8_t* cmd = prepare_cmd(len);

  if (!cmd) {
    return -1;
  }

  int err = send_req(cmd, len);
  delete [] cmd;
  if (err) {
    report_error(RE_SEND_CMD_ERROR);
    return -1;
  }

  return wait_simple_response(420000);
}

uint8_t* CollectRequest::prepare_cmd(size_t& len) {
  uint8_t* buf = new uint8_t[128];
  uint8_t* p = buf + 11;
  size_t rlen = 116;
  bool ok {true};

  memcpy(buf, "COLLECT_LOG", 11);
  for (auto subsys: sys_list_) {
    if (!rlen) {
      ok = false;
      break;
    }
    *p = ' ';
    ++p;
    --rlen;

    size_t wlen;

    if (put_cp_type(p, len, subsys, wlen)) {
      ok = false;
      break;
    }

    p += wlen;
    rlen -= wlen;
  }

  if (ok) {
    *p = '\n';
    len = p - buf + 1;
  } else {
    delete [] buf;
    buf = nullptr;
  }

  return buf;
}
