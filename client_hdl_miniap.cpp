/*
 *  client_hdl_miniap.cpp - miniap commands handler.
 *
 *  Copyright (C) 2020-2022 Unisoc Communications Inc.
 *
 *  History:
 *  2020-01-20 Chunlan.Wang
 *  Initial version.
 */

#include "client_hdl.h"
#include "log_ctrl.h"
#include "parse_utils.h"
#include "req_err.h"

void ClientHandler::proc_set_miniap_status(const uint8_t* req, size_t len) {
  const uint8_t* endp = req + len;
  const uint8_t* tok;
  size_t tlen;

  tok = get_token(req, len, tlen);
  if (!tok) {  // Parameter missing
    send_response(fd(), REC_INVAL_PARAM);

    err_log("SET_MINIAP_LOG parameter missing");

    return;
  }

  bool status;
  info_log("The operation of the cmd is %s", tok);
  if (2 == tlen && !memcmp(tok, "ON", 2)) {
    status = true;
  } else if (3 == tlen && !memcmp(tok, "OFF", 3)) {
    status = false;
  } else {
    send_response(fd(), REC_INVAL_PARAM);

    LogString sd;

    str_assign(sd, reinterpret_cast<const char*>(tok), tlen);
    err_log("SET_MINIAP_LOG_STATUS invalid status %s",
            ls2cstring(sd));
    return;
  }

  req = tok + tlen;
  len = endp - req;

  tok = get_token(req, len, tlen);
  if (tok) {  // Unexpected parameter
    send_response(fd(), REC_INVAL_PARAM);

    LogString sd;

    str_assign(sd, reinterpret_cast<const char*>(req), len);
    err_log("SET_MINIAP_LOG_STATUS extra parameter %s",
            ls2cstring(sd));

    return;
  }

  int err = controller()->set_miniap_log_status(status);

  if (LCR_SUCCESS != err) {
    err_log("set_miniap_log_state error %d", err);
  }
  send_response(fd(), trans_result_to_req_result(err));
}

void ClientHandler::proc_set_orca_log(const uint8_t* cmdp, size_t cmd_len, const uint8_t* req, size_t len) {
  const uint8_t* endp = req + len;
  const uint8_t* tok;
  size_t tlen;
  LogString cmd;
  tok = get_token(req, len, tlen);
  if (!tok) {  // Parameter missing
    send_response(fd(), REC_INVAL_PARAM);
    err_log("SET_ORCA_LOG parameter missing");

    return;
  }

  bool status;
  info_log("The operation of the cmd is %s", tok);
  if (2 == tlen && !memcmp(tok, "ON", 2)) {
    status = true;
  } else if (3 == tlen && !memcmp(tok, "OFF", 3)) {
    status = false;
  } else {
    send_response(fd(), REC_INVAL_PARAM);

    LogString sd;

    str_assign(sd, reinterpret_cast<const char*>(tok), tlen);
    err_log("SET_ORCA_LOG invalid status %s",
            ls2cstring(sd));
    return;
  }

  req = tok + tlen;
  len = endp - req;

  tok = get_token(req, len, tlen);
  if (tok) {  // Unexpected parameter
    send_response(fd(), REC_INVAL_PARAM);

    LogString sd;

    str_assign(sd, reinterpret_cast<const char*>(req), len);
    err_log("SET_ORCA_LOG extra parameter %s",
            ls2cstring(sd));

    return;
  }
  str_assign(cmd, reinterpret_cast<const char*>(cmdp), cmd_len);
  int err = controller()->set_orca_log(cmd, status);

  if (LCR_SUCCESS != err) {
    err_log("set_miniap_log_state error %d", err);
  }
  send_response(fd(), trans_result_to_req_result(err));
}
