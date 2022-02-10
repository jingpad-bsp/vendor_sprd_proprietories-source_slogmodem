/*
 * wan_modem_time_sync_refnotify.cpp - commute with refnotify by socket,
 *                               and receive wan modem time from it. after getting modem time
 *                               notify father class the updated time.
 *
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * History:
 * 2018-09-28 Gloria He
 * Initial version.
 */

#ifdef HOST_TEST_
  #include "sock_test.h"
#else
  #include <cutils/sockets.h>
#endif
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "cp_log_cmn.h"
#include "def_config.h"
#include "multiplexer.h"
#include "wan_modem_log.h"
#include "wan_modem_time_sync_refnotify.h"

WanModemTimeSyncRefnotify::WanModemTimeSyncRefnotify(LogController* ctrl,
    Multiplexer* multi)
    : DataProcessHandler(-1, ctrl, multi, WAN_MODEM_TIME_SYNC_BUF_SIZE),
      m_timer{0} {}

WanModemTimeSyncRefnotify::~WanModemTimeSyncRefnotify() {
  if (m_timer) {
    multiplexer()->timer_mgr().del_timer(m_timer);
    m_timer = 0;
  }
}

int WanModemTimeSyncRefnotify::start() {
  m_fd = socket_local_client(CP_TIME_SYNC_SERVER_SOCKET,
                             ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);

  if (-1 == m_fd) {
    m_timer = multiplexer()->timer_mgr().create_timer(500,
                                                      connect_server, this);
  } else {
    long flags = fcntl(m_fd, F_GETFL);
    flags |= O_NONBLOCK;
    int err = fcntl(m_fd, F_SETFL, flags);
    if (-1 == err) {
      close();
      err_log("set time sync socket O_NONBLOCK fail.");
      return -1;
    } else {
      multiplexer()->register_fd(this, POLLIN);
    }
  }

  return 0;
}

void WanModemTimeSyncRefnotify::connect_server(void* param) {
  WanModemTimeSyncRefnotify* p = static_cast<WanModemTimeSyncRefnotify*>(param);

  p->m_timer = 0;
  p->start();
}

void WanModemTimeSyncRefnotify::on_modem_alive() {
  return ;
}

void WanModemTimeSyncRefnotify::on_modem_assert() {
  WanModemTimeSync::on_modem_assert();
}

int WanModemTimeSyncRefnotify::process_data() {
  if (2 * sizeof(uint32_t) <= m_buffer.data_len) {
    time_sync m_ts;
    memcpy(&m_ts, m_buffer.buffer, 2 * sizeof(uint32_t));
    info_log("MODEM time info: sys_cnt = %u, up time = %u",
             static_cast<unsigned>(m_ts.sys_cnt),
             static_cast<unsigned>(m_ts.uptime));
    m_buffer.data_len = 0;
    on_time_updated(m_ts);
  } else {
    err_log("modem time synchronisation information length %d is not correct.",
            static_cast<int>(m_buffer.data_len));
  }

  return 0;
}

void WanModemTimeSyncRefnotify::process_conn_closed() {
  multiplexer()->unregister_fd(this);
  close();
  start();
}

void WanModemTimeSyncRefnotify::process_conn_error(int /*err*/) {
  process_conn_closed();
}
