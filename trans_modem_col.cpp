/*
 *  trans_modem_col.cpp - The MODEM log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-5-18 Zhang Ziyi
 *  Initial version.
 */

#include "trans_log_convey.h"
#include "trans_modem_col.h"
#include "wan_modem_log.h"

TransModemLogCollect::TransModemLogCollect(WanModemLogHandler* wan)
    :TransModem{wan, COLLECT_MODEM_LOG},
     wan_modem_{wan},
     modem_convey_{nullptr},
     last_collect_timer_{nullptr} {}

TransModemLogCollect::~TransModemLogCollect() {
  TransModemLogCollect::cancel();
}

int TransModemLogCollect::execute() {
  int ret{TRANS_E_SUCCESS};
  if (LogPipeHandler::CWS_NOT_WORKING == wan_modem_->cp_state()) {
    info_log("wanmodem is not working");
    ret = start_log_convey();
    switch (ret) {
      case TRANS_E_SUCCESS:
        on_finished(modem_convey_->result());
        delete modem_convey_;
        modem_convey_ = nullptr;
        break;
      default: // TRANS_E_STARTED
        on_started();
        break;
    }

    return ret;
  }

  // The MODEM is dumping
  if (LogPipeHandler::CWS_DUMP == wan_modem_->cp_state()) {
    info_log("wanmodem is dumping");
    on_started();
    wan_modem_->register_dump_end(this, dump_end_callback);
    return TRANS_E_STARTED;
  }

  TimerManager& tmgr = wan_modem_->multiplexer()->timer_mgr();

  info_log("log mode is %d",wan_modem_->log_mode());
  // if log is enabled, save the last log in the Modem
  if (LogConfig::LM_NORMAL == wan_modem_->log_mode()) {
    info_log("create timer");
    last_collect_timer_ =
        tmgr.create_timer(MODEM_LAST_LOG_PERIOD,
                          log_flush_callback, this);
    on_started();
    ret = TRANS_E_STARTED;
  } else {
    ret = start_log_convey();
    info_log("not LM_NORMAL, ret = %d", ret);
    switch (ret) {
      case TRANS_E_SUCCESS:
        on_finished(modem_convey_->result());
        delete modem_convey_;
        modem_convey_ = nullptr;
        break;
      default: // TRANS_E_STARTED
        on_started();
        break;
    }
  }

  return ret;
}

int TransModemLogCollect::start_log_convey() {
  return wan_modem_->start_log_convey(this, log_convey_fin,
                                      modem_convey_);
}

void TransModemLogCollect::cancel() {
  if (TS_EXECUTING == state()) {
    if (last_collect_timer_) {
      TimerManager& tmgr = wan_modem_->multiplexer()->timer_mgr();
      tmgr.del_timer(last_collect_timer_);
      last_collect_timer_ = nullptr;
    }

    wan_modem_->unregister_dump_end(this);

    if (modem_convey_) {
      wan_modem_->cancel_log_convey(modem_convey_);
      delete modem_convey_;
      modem_convey_ = nullptr;
    }

    on_canceled();
  }
}

void TransModemLogCollect::dump_end_callback(void* client) {
  TransModemLogCollect* modem_col = static_cast<TransModemLogCollect*>(client);

  modem_col->wan_modem_->unregister_dump_end(modem_col);

  int res = TRANS_R_FAILURE;
  switch (modem_col->start_log_convey()) {
    case TRANS_E_SUCCESS:
      res = modem_col->modem_convey_->result();
      delete modem_col->modem_convey_;
      modem_col->modem_convey_ = nullptr;
      modem_col->finish(res);
      break;
    default: // TRANS_E_STARTED
      break;
  }
}

void TransModemLogCollect::log_flush_callback(void* client) {
  TransModemLogCollect* modem_col = static_cast<TransModemLogCollect*>(client);

  info_log("log flush callback");
  modem_col->last_collect_timer_ = nullptr;
  modem_col->wan_modem_->flush();

  int res = TRANS_R_FAILURE;
  switch (modem_col->start_log_convey()) {
    case TRANS_E_SUCCESS:
      res = modem_col->modem_convey_->result();
      delete modem_col->modem_convey_;
      modem_col->modem_convey_ = nullptr;
      modem_col->finish(res);
      break;
    default: // TRANS_E_STARTED
      break;
  }
}

void TransModemLogCollect::log_convey_fin(void* client, Transaction* trans) {
  TransModemLogCollect* modem_col = static_cast<TransModemLogCollect*>(client);
  int res = trans->result();
  delete trans;
  modem_col->modem_convey_ = nullptr;

  modem_col->wan_modem_->flush();

  modem_col->finish(res);
}

