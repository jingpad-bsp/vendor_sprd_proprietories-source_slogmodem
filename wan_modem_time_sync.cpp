/*
 * wan_modem_time_sync.cpp - store modem time,
 * and handler of updating wan modem log file
 *
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * History:
 * 2018-09-28 Gloria He
 * intial version.
 */

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include "cp_log_cmn.h"
#include "multiplexer.h"
#include "wan_modem_log.h"
#include "wan_modem_time_sync.h"

WanModemTimeSync::WanModemTimeSync()
     :ts_state_{false},
      ts_{0,0},
      time_sync_client_{nullptr},
      time_update_{nullptr} {}

WanModemTimeSync::~WanModemTimeSync() {
  time_update_ = nullptr;
  time_sync_client_ = nullptr;
}

void WanModemTimeSync::set_time_update_callback(void* client,
    cp_time_update_callback_t cb) {
  time_sync_client_ = client;
  time_update_ = cb;
}

void WanModemTimeSync::cleanup_time_update_callback() {
  time_sync_client_ = nullptr;
  time_update_ = nullptr;
}

bool WanModemTimeSync::get_time_sync_info(time_sync& ts) {
  bool ret = false;

  if (ts_state_) {
    ts = ts_;
    ret = true;
  }

  return ret;
}

void WanModemTimeSync::on_time_updated(const time_sync& ts) {
  ts_ = ts;
  ts_state_ = true;
  if (time_update_) {
    time_update_(time_sync_client_,ts_);
  }
}

void WanModemTimeSync::clear_time_sync_info() {
  if (ts_state_) {
    memset(&ts_, 0, sizeof ts_);
    ts_state_ = false;
  }
}

void WanModemTimeSync::on_modem_assert() {
  clear_time_sync_info();
}

