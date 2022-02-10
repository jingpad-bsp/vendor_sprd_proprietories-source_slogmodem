/*
 *  set_mipi_log.cpp - set mipi log request class.
 *
 *  Copyright (C) 2020 Unisoc Communications Inc.
 *
 *  History:
 *  2020-3-4 Chunlan.Wang
 *  Initial version.
 */

#include "set_mipi_log.h"

SetMipiLog::SetMipiLog(LogString type, LogString channel, LogString freq)
    :m_type{type},
     m_channel{channel},
     m_freq{freq}{}

int SetMipiLog::do_request() {
  LogString buf;
  str_assign(buf, "SET_MIPI_LOG_INFO ", 18);
  buf += m_type;
  buf += " ";
  buf += m_channel;
  buf += " ";
  buf += m_freq;
  buf += "\n";
  info_log("set mipi log info : %s, len: %d", ls2cstring(buf), strlen(ls2cstring(buf)));
  if (send_req(ls2cstring(buf), strlen(ls2cstring(buf)))) {
    report_error(RE_SEND_CMD_ERROR);
    return -1;
  }

  return wait_simple_response(DEFAULT_RSP_TIME);
}
