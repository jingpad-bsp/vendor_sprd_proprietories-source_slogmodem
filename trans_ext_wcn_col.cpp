/*
 *  trans_ext_wcn_col.cpp - The external wcn log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-5-18 Zhang Ziyi
 *  Initial version.
 */
#include "trans_log_convey.h"
#include "trans_ext_wcn_col.h"
#include "ext_wcn_log.h"

TransExtWcnLogCollect::TransExtWcnLogCollect(ExtWcnLogHandler* ext_wcn)
    : TransModem{ext_wcn, COLLECT_WCN_LOG},
      ext_wcn_{ext_wcn},
      modem_convey_{nullptr} {}

TransExtWcnLogCollect::~TransExtWcnLogCollect() {
  TransExtWcnLogCollect::cancel();
}

int TransExtWcnLogCollect::execute() {
  int ret{TRANS_E_SUCCESS};
  if (LogPipeHandler::CWS_NOT_WORKING == ext_wcn_->cp_state()) {
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

  // The External WCN (marlin/marlin2) is dumping
  if (LogPipeHandler::CWS_DUMP == ext_wcn_->cp_state()) {
    on_started();
    ext_wcn_->register_dump_end(this, dump_end_callback);
    return TRANS_E_STARTED;
  }

  if (LogConfig::LM_NORMAL == ext_wcn_->log_mode()) {
    ext_wcn_->flush();
  }
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

int TransExtWcnLogCollect::start_log_convey() {
  return ext_wcn_->start_log_convey(this, log_collect_fin_callback,
                                    modem_convey_);
}

void TransExtWcnLogCollect::cancel() {
  if (TS_EXECUTING == state()) {

    ext_wcn_->unregister_dump_end(this);

    if (modem_convey_) {
      ext_wcn_->cancel_log_convey(modem_convey_);
      delete modem_convey_;
      modem_convey_ = nullptr;
    }

    on_canceled();
  }
}

void TransExtWcnLogCollect::dump_end_callback(void* client) {
  TransExtWcnLogCollect* ext_wcn_col = static_cast<TransExtWcnLogCollect*>(client);

  ext_wcn_col->ext_wcn_->unregister_dump_end(ext_wcn_col);

  int res = TRANS_R_FAILURE;
  switch (ext_wcn_col->start_log_convey()) {
    case TRANS_E_SUCCESS:
      res = ext_wcn_col->modem_convey_->result();
      delete ext_wcn_col->modem_convey_;
      ext_wcn_col->modem_convey_ = nullptr;
      ext_wcn_col->finish(res);
      break;
    default: // TRANS_E_STARTED
      break;
  }

}

void TransExtWcnLogCollect::log_collect_fin_callback(void* client, Transaction* trans) {
  TransExtWcnLogCollect* ext_wcn_col = static_cast<TransExtWcnLogCollect*>(client);
  int res = trans->result();

  ext_wcn_col->modem_convey_ = nullptr;
  ext_wcn_col->ext_wcn_->flush();

  ext_wcn_col->finish(res);
}
