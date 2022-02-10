/*
 *  get_cp_max_size.cpp - get subsystem's max size for log saving.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-4-12 YAN Zhihang
 *  Initial version.
 */
#include "get_cp_max_size.h"

GetCpMaxSize::GetCpMaxSize(CpType ct, MediaType mt)
    : m_type{ct}, mt_{mt} {}

int GetCpMaxSize::do_request() {
  uint8_t buf[32];

  memcpy(buf, "GET_CP_LOG_SIZE ", 16);

  uint8_t* p = buf + 16;
  size_t rlen = sizeof buf - 16;
  size_t tlen = 0;
  int err = put_cp_type(p, rlen, m_type, tlen);

  if (err || (rlen == tlen)) {
    fprintf(stderr,
            "Can not make the subsystem parameter of GET_CP_LOG_SIZE\n");
    return -1;
  }

  p += tlen;

  size_t len;
  if (MT_INTERNAL == mt_) {
    len = snprintf(reinterpret_cast<char*>(p), rlen - tlen, " internal\n");
  } else {
    len = snprintf(reinterpret_cast<char*>(p), rlen - tlen, " external\n");
  }

  err = send_req(buf, 16 + tlen + len);

  if (err) {
    report_error(RE_SEND_CMD_ERROR);
    return -1;
  }


  return parse_query_result();
}

int GetCpMaxSize::parse_query_result() {
  char resp[SLOGM_RSP_LENGTH];
  size_t rlen = SLOGM_RSP_LENGTH;

  int ret = wait_response(resp, rlen, DEFAULT_RSP_TIME);

  if (ret) {
    report_error(RE_WAIT_RSP_ERROR);
    return -1;
  }

  ResponseErrorCode err_code;
  const void* stop_ptr;

  ret = parse_result(resp, rlen, err_code, stop_ptr);
  if (ret) {
    LogString sd;

    str_assign(sd, resp, rlen);
    fprintf(stderr, "Invalid response: %s\n", ls2cstring(sd));
    err_log("invalid response: %s", ls2cstring(sd));
  } else {
    if (REC_SUCCESS != err_code) {
      const char* err_str = resp_code_to_string(err_code);
      fprintf(stderr, "Error: %d(%s)\n", static_cast<int>(err_code),
              err_str);
      err_log("error: %d(%s)", static_cast<int>(err_code),
              err_str);
      ret = -1;
    } else {
      char* res = static_cast<char*>(const_cast<void*>(stop_ptr));
      size_t res_len = static_cast<size_t>(rlen - (res - &resp[0]));

      if ((' ' == *res) && (res_len > 1)) {
        ++res;
        --res_len;
        LogString str_size;
        str_assign(str_size, res, res_len);

        unsigned long number;
        const char* endp;
        if (non_negative_number(ls2cstring(str_size), number, endp) &&
            spaces_only(endp)) {
          fprintf(stdout, "%s MB\n", ls2cstring(str_size));
        } else {
          ret = -1;
          fprintf(stderr, "%s is not a valid storage size\n",
                  ls2cstring(str_size));
          err_log("%s is not a valid storage size", ls2cstring(str_size));
        }
      } else {
        ret = -1;
        LogString invalid_res_err;

        str_assign(invalid_res_err, resp, rlen);
        fprintf(stderr, "Invalid response: %s\n", ls2cstring(invalid_res_err));
        err_log("invalid response: %s", ls2cstring(invalid_res_err));
      }
    }
  }

  return ret;

}
