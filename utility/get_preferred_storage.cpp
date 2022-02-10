/*
 *  get_preferred_storage.cpp - get the preferred storage for log saving
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-11-27 YAN Zhihang
 *  Initial version.
 */
#include "cplogctl_cmn.h"
#include "get_preferred_storage.h"

int GetPreferredStorage::do_request() {
  if (send_req("GET_STORAGE_CHOICE\n", 19)) {
    report_error(RE_SEND_CMD_ERROR);
    return -1;
  }

  return parse_query_result();
}

int GetPreferredStorage::parse_query_result() {
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

      if ((' ' == *res) && (res_len == 9)) {
        ++res;
        --res_len;

        if (!memcmp(res, "EXTERNAL", 8)) {
          printf("%s storage\n", "external");
        } else if (!memcmp(res, "INTERNAL", 8)) {
          printf("%s storage\n", "internal");
        } else {
          ret = -1;
          LogString res_err;

          str_assign(res_err, res, res_len);
          fprintf(stderr, "get preferred storage return: %s error\n",
                  ls2cstring(res_err));
          err_log("get preferred storage return: %s error", ls2cstring(res_err));
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
