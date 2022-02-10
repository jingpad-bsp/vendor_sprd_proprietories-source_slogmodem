/*
 *  trans_int_wcn_col.cpp - The external wcn log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-5-27 YAN Zhihang
 *  Initial version.
 */

#include "multiplexer.h"
#include "int_wcn_log.h"
#include "trans_int_wcn_col.h"

TransIntWcnLogCollect::TransIntWcnLogCollect(IntWcnLogHandler* int_wcn)
    : TransGlobal{nullptr, WCN_LOG_COLLECT},
      int_wcn_{int_wcn} {}

TransIntWcnLogCollect::~TransIntWcnLogCollect() {
  TransIntWcnLogCollect::cancel();
}

int TransIntWcnLogCollect::execute() {
  if (LogPipeHandler::CWS_NOT_WORKING == int_wcn_->cp_state()) {
    on_finished(TRANS_R_SUCCESS);
    return TRANS_E_SUCCESS;
  }

  // The internal wcn(TSHARK) is dumping
  if (LogPipeHandler::CWS_DUMP == int_wcn_->cp_state()) {
    on_started();
    int_wcn_->register_dump_end(this, dump_end_callback);
    return TRANS_E_STARTED;
  }

  int ret{TRANS_E_SUCCESS};

  if (int_wcn_->crash()) {
    on_finished(TRANS_R_FAILURE);
  } else {
    on_started();

    TimerManager& tmgr = int_wcn_->multiplexer()->timer_mgr();
    dump_timer_ = tmgr.create_timer(2000, dump_timer, this);

    int_wcn_->register_dump_start(this, dump_start_callback);
    int_wcn_->register_dump_end(this, dump_end_callback);
    ret = TRANS_E_STARTED;
  }

  return ret;
}

void TransIntWcnLogCollect::cancel() {
  if (TS_EXECUTING == state()) {
    if (dump_timer_) {
      TimerManager& tmgr = int_wcn_->multiplexer()->timer_mgr();
      tmgr.del_timer(dump_timer_);
      dump_timer_ = nullptr;
    }
    int_wcn_->unregister_dump_start(this);
    int_wcn_->unregister_dump_end(this);
    on_canceled();
  }
}

void TransIntWcnLogCollect::dump_start_callback(void* client) {
  TransIntWcnLogCollect* int_wcn_col = static_cast<TransIntWcnLogCollect*>(client);

  // Stop the timer
  if (int_wcn_col->dump_timer_) {
    TimerManager& tmgr = int_wcn_col->int_wcn_->multiplexer()->timer_mgr();
    tmgr.del_timer(int_wcn_col->dump_timer_);
    int_wcn_col->dump_timer_ = nullptr;
  }
  int_wcn_col->int_wcn_->unregister_dump_start(int_wcn_col);
}

void TransIntWcnLogCollect::dump_end_callback(void* client) {
  TransIntWcnLogCollect* int_wcn_col = static_cast<TransIntWcnLogCollect*>(client);

  // Even if the IntWcnLogHandler didn't notify us of the dump start,
  // we can stop the timer.
  if (int_wcn_col->dump_timer_) {
    TimerManager& tmgr = int_wcn_col->int_wcn_->multiplexer()->timer_mgr();
    tmgr.del_timer(int_wcn_col->dump_timer_);
    int_wcn_col->dump_timer_ = nullptr;
    int_wcn_col->int_wcn_->unregister_dump_start(int_wcn_col);
  }
  int_wcn_col->int_wcn_->unregister_dump_end(int_wcn_col);
  int_wcn_col->finish(TRANS_R_SUCCESS);
}

void TransIntWcnLogCollect::dump_timer(void* client) {
  TransIntWcnLogCollect* int_wcn_col = static_cast<TransIntWcnLogCollect*>(client);

  int_wcn_col->dump_timer_ = nullptr;
  int_wcn_col->int_wcn_->unregister_dump_start(int_wcn_col);
  int_wcn_col->int_wcn_->unregister_dump_end(int_wcn_col);
  int_wcn_col->finish(TRANS_R_FAILURE);
}
