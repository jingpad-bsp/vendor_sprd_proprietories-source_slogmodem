/*
 *  cp_stat_hdl.cpp - The general CP status handler implementation.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-4-8 Zhang Ziyi
 *  Initial version.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef HOST_TEST_
#include "sock_test.h"
#else
#include "cutils/sockets.h"
#endif

#include "cp_stat_hdl.h"
#include "multiplexer.h"
#include "log_ctrl.h"
#include "parse_utils.h"

CpStateHandler::CpStateHandler(LogController* ctrl, Multiplexer* multiplexer,
                               const char* serv_name)
    : DataProcessHandler(-1, ctrl, multiplexer, MODEM_STATE_BUF_SIZE),
      m_serv_name(serv_name) {}

int CpStateHandler::init() {
  m_fd = socket_local_client(ls2cstring(m_serv_name),
                             ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
  if (-1 == m_fd) {
    multiplexer()->timer_mgr().add_timer(3000, connect_server, this);
  } else {
    multiplexer()->register_fd(this, POLLIN);
  }

  return 0;
}

void CpStateHandler::connect_server(void* param) {
  CpStateHandler* p = static_cast<CpStateHandler*>(param);

  p->init();
}

int CpStateHandler::process_data() {
  LogString assert_info;

  str_assign(assert_info, reinterpret_cast<const char*>(m_buffer.buffer),
             m_buffer.data_len);
  info_log("%u bytes: %s", static_cast<unsigned>(m_buffer.data_len),
           ls2cstring(assert_info));

  CpType type;
  CpEvent evt = parse_notify(m_buffer.buffer, m_buffer.data_len, type);

  if (evt != CE_NONE) {
    controller()->process_cp_evt(type, evt, assert_info, m_buffer.data_len);
  }

  m_buffer.data_len = 0;

  return 0;
}

void CpStateHandler::process_conn_closed() {
  err_log("CpStateHandler::process_conn_closed()");
  multiplexer()->unregister_fd(this);
  close();

  init();
}

void CpStateHandler::process_conn_error(int /*err*/) {
  err_log("CpStateHandler::process_conn_error()");
  CpStateHandler::process_conn_closed();
}
