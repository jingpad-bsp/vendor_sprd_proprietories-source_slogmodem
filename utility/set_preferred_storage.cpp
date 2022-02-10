/*
 *  set_preferred_storage.cpp - set the preferred storage for log saving
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-11-27 YAN Zhihang
 *  Initial version.
 */
#include "cplogctl_cmn.h"
#include "set_preferred_storage.h"

SetPreferredStorage::SetPreferredStorage(MediaType mt)
    : mt_{mt} {}

int SetPreferredStorage::do_request() {
  int err = -1;

  if (MT_INTERNAL == mt_) {
    err = send_req("SET_STORAGE_CHOICE INTERNAL\n", 28);
  } else {
    err = send_req("SET_STORAGE_CHOICE EXTERNAL\n", 28);
  }

  if (err) {
    report_error(RE_SEND_CMD_ERROR);
    return -1;
  }

  return wait_simple_response(DEFAULT_RSP_TIME);
}
