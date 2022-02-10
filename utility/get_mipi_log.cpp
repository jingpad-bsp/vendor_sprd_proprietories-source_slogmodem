/*
 *  get_mipi_log.cpp - get mipi log request class.
 *
 *  Copyright (C) 2020 Unisoc Communications Inc.
 *
 *  History:
 *  2020-3-4 Chunlan.Wang
 *  Initial version.
 */

#include "get_mipi_log.h"

GetMipiLog::GetMipiLog(LogString type)
    :m_type{type}{}

int GetMipiLog::do_request() {
  LogString buf;
  str_assign(buf, "GET_MIPI_LOG_INFO ", 18);
  buf += m_type;
  buf += "\n";
  info_log("get mipi log info : %s, len: %d", ls2cstring(buf), strlen(ls2cstring(buf)));
  if (send_req(ls2cstring(buf), strlen(ls2cstring(buf)))) {
    report_error(RE_SEND_CMD_ERROR);
    return -1;
  }

  return parse_query_result();
}

int GetMipiLog::parse_query_result() {
    char resp[SLOGM_RSP_LENGTH];
    size_t rlen = SLOGM_RSP_LENGTH;

    int ret = wait_response(resp, rlen, DEFAULT_RSP_TIME);

    if (ret) {
      report_error(RE_WAIT_RSP_ERROR);
      return -1;
    }
    fprintf(stdout, "%s\n", resp);
    return 0;
}
