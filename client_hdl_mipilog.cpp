/*
 *  client_hdl_mipilog.cpp - mipilog commands handler.
 *
 *  Copyright (C) 2020-2022 Unisoc Communications Inc.
 *
 *  History:
 *  2020-1-20 Zhang Ziyi
 *  Initial version.
 */

#include "client_hdl.h"
#include "log_ctrl.h"
#include "parse_utils.h"
#include "req_err.h"

void ClientHandler::proc_set_mipi_log_info(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  size_t tlen;
  const uint8_t* endp = req + len;
  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("SET_MIPI_LOG_INFO with no type");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }
  LogString type;
  str_assign(type, reinterpret_cast<char*>(const_cast<uint8_t*>(tok)), tlen);

  req = tok + tlen;
  len = endp - req;

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("SET_MIPI_LOG_INFO with no channel");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  LogString channel;
  str_assign(channel, reinterpret_cast<char*>(const_cast<uint8_t*>(tok)), tlen);
  req = tok + tlen;
  len = endp - req;

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("SET_MIPI_LOG_INFO with no freq");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  LogString freq;
  str_assign(freq, reinterpret_cast<char*>(const_cast<uint8_t*>(tok)), tlen);
  req = tok + tlen;
  len = endp - req;
  tok = get_token(req, len, tlen);
  if (tok) {  // Unexpected parameter
    send_response(fd(), REC_INVAL_PARAM);

    LogString sd;

    str_assign(sd, reinterpret_cast<const char*>(req), len);
    err_log("SET_MIPI_LOG_INFO extra parameter %s",
            ls2cstring(sd));

    return;
  }

  int err = controller()->set_mipi_log_info(type, channel, freq);

  if (LCR_SUCCESS != err) {
    err_log("set_mipi_log_info error %d", err);
  }
  send_response(fd(), trans_result_to_req_result(err));

}

void ClientHandler::proc_get_mipi_log_info(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  size_t tlen;
  const uint8_t* endp = req + len;
  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("GET_MIPI_LOG_INFO with no type");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }
  LogString type;
  str_assign(type, reinterpret_cast<char*>(const_cast<uint8_t*>(tok)), tlen);

  req = tok + tlen;
  len = endp - req;
  tok = get_token(req, len, tlen);
  if (tok) {  // Unexpected parameter
    send_response(fd(), REC_INVAL_PARAM);

    LogString sd;

    str_assign(sd, reinterpret_cast<const char*>(req), len);
    err_log("GET_MINIAP_LOG_STATUS extra parameter %s",
            ls2cstring(sd));

    return;
  }
  LogString channel, freq;
  int err = controller()->get_mipi_log_info(type, channel, freq);
  char rsp[64];
  memset(rsp, 0, sizeof(rsp));
  snprintf(rsp, sizeof(rsp), "%s", ls2cstring(type));
  if (strlen(ls2cstring(channel)) > 0 && strlen(ls2cstring(freq)) > 0) {
    snprintf(rsp + strlen(rsp), sizeof(rsp) - strlen(rsp), " %s %s\n",
                         ls2cstring(channel), ls2cstring(freq));
  } else {
    snprintf(rsp + strlen(rsp), sizeof(rsp) - strlen(rsp), "\n");
    err_log("channel or freq is NULL");
  }
  write(fd(), rsp, strlen(rsp));
  if (LCR_SUCCESS != err) {
    err_log("get_mipi_log_info error %d", err);
  }
}
