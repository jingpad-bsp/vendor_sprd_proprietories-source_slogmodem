/*
 *  trans_ext_gnss_col.cpp - The Gnss log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2018-10-28 Gloria.He
 *  Initial version.
 */

#include "trans_log_convey.h"
#include "trans_ext_gnss_col.h"
#include "ext_gnss_log.h"

TransExtGnssLogCollect::TransExtGnssLogCollect(ExtGnssLogHandler* extgnss)
    :TransModem{extgnss, COLLECT_GNSS_LOG},
     extgnss_{extgnss},
     modem_convey_{nullptr} {}

TransExtGnssLogCollect::~TransExtGnssLogCollect() {
   TransExtGnssLogCollect::cancel();
 }

int TransExtGnssLogCollect::execute() {

  int ret{TRANS_E_SUCCESS};
  if (LogPipeHandler::CWS_NOT_WORKING == extgnss_->cp_state()) {
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
  if (LogPipeHandler::CWS_DUMP == extgnss_->cp_state()) {
    on_started();
    extgnss_->register_dump_end(this, dump_end_callback);
    return TRANS_E_STARTED;
  }

  // if log is enabled, save the last log in the Modem
  if (LogConfig::LM_NORMAL == extgnss_->log_mode()) {
    extgnss_->flush();
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

int TransExtGnssLogCollect::start_log_convey() {
  return extgnss_->start_log_convey(this, log_convey_fin,
                                    modem_convey_);
}

void TransExtGnssLogCollect::cancel() {
  if (TS_EXECUTING == state()) {

    extgnss_->unregister_dump_end(this);

    if (modem_convey_) {
      extgnss_->cancel_log_convey(modem_convey_);
      delete modem_convey_;
      modem_convey_ = nullptr;
    }

    on_canceled();
  }
}

void TransExtGnssLogCollect::dump_end_callback(void* client) {
  TransExtGnssLogCollect* extgnss_col = static_cast<TransExtGnssLogCollect*>(client);

  extgnss_col->extgnss_->unregister_dump_end(extgnss_col);

  int res = TRANS_R_FAILURE;
  switch (extgnss_col->start_log_convey()) {
    case TRANS_E_SUCCESS:
      res = extgnss_col->modem_convey_->result();
      delete extgnss_col->modem_convey_;
      extgnss_col->modem_convey_ = nullptr;
      extgnss_col->finish(res);
      break;
    default: // TRANS_E_STARTED
      break;
  }
}

void TransExtGnssLogCollect::log_convey_fin(void* client, Transaction* trans) {
  TransExtGnssLogCollect* extgnss_col = static_cast<TransExtGnssLogCollect*>(client);
  int res = trans->result();
  delete trans;
  extgnss_col->modem_convey_ = nullptr;

  extgnss_col->extgnss_->flush();

  extgnss_col->finish(res);
}

