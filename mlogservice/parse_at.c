/*
 *  parse_at.c - At commands return value analysis.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-2-13 Elon Li
 *  Initial version.
*/
#include <stdio.h>
#include <string.h>
#include "misc.h"
#include "parse_at.h"
#include "config.h"

int parse_at_res(const uint8_t* cmd, uint8_t* buf, int len) {
  size_t tok_len;
  size_t sign_len = 0;

  const uint8_t* token = get_token(buf, len, &tok_len);
  const uint8_t* sign = get_sign(cmd, &sign_len);
;
  if (!token || !sign) {
    info_log("token is error");
    return -1;
  }
  info_log("cmd is %s, sign is %s, signlen is %d, buf is %s, token is %s,token len is %d, len is %d",
           cmd, sign, sign_len, buf, token, tok_len, len);

  switch (tok_len) {
    case 2:
      if (!memcmp(token, "OK", 2)) {
        info_log("get response success");
        return 1;
      }
    case 5:
      if (!memcmp(token, "ERROR", 5)) {
        info_log("get response error");
        return 0;
      }
    default:
      if ( tok_len == sign_len && !memcmp(token, sign, sign_len - 1)) {
        return parse_first_number(token + tok_len, len - tok_len);
      } else {
        err_log("parse response error");
        break;
      }
  }
  return -1;
}
