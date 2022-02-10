/*
 * wan_modem_time_sync_atcmd.cpp - To get  wan modem time by at cmd
 *                               and handler of updating
 *                               wan modem log file time information.
 *
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * History:
 * 2018-09-28 Gloria He
 * Initial version.
 */
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/sysinfo.h>

#include "trans_modem_time.h"
#include "wan_modem_log.h"
#include "wan_modem_time_sync_atcmd.h"

WanModemTimeSyncAtCmd::WanModemTimeSyncAtCmd(WanModemLogHandler* modem)
     : in_query_{false},
       trans_state_{CTS_NOT_BEGIN},
       trans_ts_{nullptr},
       timer_{nullptr},
       wan_modem_{modem} {}

WanModemTimeSyncAtCmd::~WanModemTimeSyncAtCmd() {
  if (timer_) {
    TimerManager& tmgr = wan_modem_->multiplexer()->timer_mgr();
    tmgr.del_timer(timer_);
    timer_ = nullptr;
  }
  if (trans_ts_) {
    wan_modem_->cancel_trans(trans_ts_);
    delete trans_ts_;
    trans_ts_ = nullptr;
  }
}

void WanModemTimeSyncAtCmd::trans_time_sync_result(void* client,
                                                   Transaction* trans) {

  TransModemTimeSync* trans_time_sync = static_cast<TransModemTimeSync*>(trans);
  WanModemTimeSyncAtCmd* modem_time_sync = static_cast<WanModemTimeSyncAtCmd*>(client);

  modem_time_sync->trans_ts_ = nullptr;
  if (Transaction::TRANS_R_SUCCESS == trans->result()) {
    time_sync m_ts;
    struct sysinfo sinfo;
    sysinfo(&sinfo);
    m_ts.uptime = sinfo.uptime;
    m_ts.sys_cnt = trans_time_sync->get_modem_tick();
    info_log("MODEM time info: sys_cnt = %u, up time = %u",
             static_cast<unsigned>(m_ts.sys_cnt),
             static_cast<unsigned>(m_ts.uptime));
    modem_time_sync->trans_state_ = CTS_SUCCESS;
    modem_time_sync->on_time_updated(m_ts);
  } else {
    modem_time_sync->trans_state_ = CTS_EXECUTING_FAIL;
    info_log("transaction result %d", trans->result());
    err_log("get modem time failed by at cmd");
    modem_time_sync->start_time_sync();
  }
  delete trans;
}

int WanModemTimeSyncAtCmd::start_time_sync() {
  TransModemTimeSync* time_sync{};
  int ret = wan_modem_->sync_modem_time(this,
                                        trans_time_sync_result,
                                        time_sync);
  switch (ret) {
    case Transaction::TRANS_E_STARTED:
      trans_state_ = CTS_EXECUTING;
      trans_ts_ = time_sync;
      break;
    default:  // TRANS_E_ERROR
      trans_state_ = CTS_START_FAIL;
      break;
  }
  if (CTS_START_FAIL == trans_state_) {
    TimerManager& tmgr = wan_modem_->multiplexer()->timer_mgr();
    timer_ = tmgr.create_timer(1000, start_query, this);
  }
  return ret;
}

int WanModemTimeSyncAtCmd::start() {

  if (in_query_) {
    return -1;
  }

  start_time_sync();
  in_query_ = true;

  return 0;
}

void WanModemTimeSyncAtCmd::start_query(void* param) {
  info_log("restart time query");
  WanModemTimeSyncAtCmd* p = static_cast<WanModemTimeSyncAtCmd*>(param);
  p->timer_ = nullptr;
  p->start_time_sync();
}

void WanModemTimeSyncAtCmd::on_modem_alive() {
  if (in_query_) {
    if (CTS_EXECUTING != trans_state_
        && CTS_SUCCESS != trans_state_) {
      if (timer_) {
        TimerManager& tmgr = wan_modem_->multiplexer()->timer_mgr();
        tmgr.del_timer(timer_);
        timer_ = nullptr;
      }
      int ret = start_time_sync();
      if (Transaction::TRANS_E_STARTED != ret) {
        err_log("on alive restart time query error");
      }
    }
  }
}

void WanModemTimeSyncAtCmd::on_modem_assert() {
  trans_state_ = CTS_NOT_BEGIN;

  if (timer_) {
    TimerManager& tmgr = wan_modem_->multiplexer()->timer_mgr();
    tmgr.del_timer(timer_);
    timer_ = nullptr;
  }

  if (trans_ts_) {
    wan_modem_->cancel_trans(trans_ts_);
    delete trans_ts_;
    trans_ts_ = nullptr;
  }
  WanModemTimeSync::on_modem_assert();
}

