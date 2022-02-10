/*
 *  trans_pmsh_col.cpp - The PM/Sensor Hub log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-10-13 Zhang Ziyi
 *  Initial version.
 */

#include "pm_sensorhub_log.h"
#include "trans_log_convey.h"
#include "trans_pmsh_col.h"

TransPmShLogCollect::TransPmShLogCollect(PmSensorhubLogHandler* pmsh)
    :TransModem{pmsh, COLLECT_PMSH_LOG},
     pmsh_{pmsh},
     modem_convey_{nullptr} {}

TransPmShLogCollect::~TransPmShLogCollect() {
  TransPmShLogCollect::cancel();
}

int TransPmShLogCollect::execute() {

  int ret{TRANS_E_SUCCESS};
  if (LogPipeHandler::CWS_NOT_WORKING == pmsh_->cp_state()) {
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
  if (LogPipeHandler::CWS_DUMP == pmsh_->cp_state()) {
    on_started();
    pmsh_->register_dump_end(this, dump_end_callback);
    return TRANS_E_STARTED;
  }

  // if log is enabled, save the last log in the Modem
  if (LogConfig::LM_NORMAL == pmsh_->log_mode()) {
    pmsh_->flush();
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

int TransPmShLogCollect::start_log_convey() {
  return pmsh_->start_log_convey(this, log_convey_fin,
                                 modem_convey_);
}

void TransPmShLogCollect::cancel() {
  if (TS_EXECUTING == state()) {

    pmsh_->unregister_dump_end(this);

    if (modem_convey_) {
      pmsh_->cancel_log_convey(modem_convey_);
      delete modem_convey_;
      modem_convey_ = nullptr;
    }

    on_canceled();
  }
}

void TransPmShLogCollect::dump_end_callback(void* client) {
  TransPmShLogCollect* pm_col = static_cast<TransPmShLogCollect*>(client);

  pm_col->pmsh_->unregister_dump_end(pm_col);
  int res = TRANS_R_FAILURE;

  switch (pm_col->start_log_convey()) {
    case TRANS_E_SUCCESS:
      res = pm_col->modem_convey_->result();
      delete pm_col->modem_convey_;
      pm_col->modem_convey_ = nullptr;
      pm_col->finish(res);
      break;
    default: // TRANS_E_STARTED
      break;
  }
}

void TransPmShLogCollect::log_convey_fin(void* client, Transaction* trans) {
  TransPmShLogCollect* pmsh_col = static_cast<TransPmShLogCollect*>(client);
  int res = trans->result();
  delete trans;
  pmsh_col->modem_convey_ = nullptr;

  pmsh_col->pmsh_->flush();

  pmsh_col->finish(res);
}

