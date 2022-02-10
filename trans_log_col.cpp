/*
 *  trans_log_col.cpp - The log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-5-18 Zhang Ziyi
 *  Initial version.
 *
 *  2017-6-6 Yan Zhihang
 *  WCN log collection added.
 */

#include "agdsp_log.h"
#include "log_config.h"
#include "log_ctrl.h"
#ifdef SUPPORT_WCN
  #ifdef EXTERNAL_WCN
    #include "ext_wcn_log.h"
    #include "trans_ext_wcn_col.h"
  #else
    #include "int_wcn_log.h"
    #include "trans_int_wcn_col.h"
  #endif
#endif
#ifdef SUPPORT_AGDSP
  #include "trans_agdsp_col.h"
#endif
#include "trans_log_col.h"
#include "trans_modem_col.h"
#include "trans_pmsh_col.h"
#include "trans_ext_gnss_col.h"

#include "wan_modem_log.h"
#include "ext_gnss_log.h"
#include "pm_sensorhub_log.h"
TransLogCollect::TransLogCollect(LogController* ctrl, const ModemSet& cps)
    : TransGlobal{ctrl, LOG_COLLECT},
      log_ctrl_{ctrl},
      cps_{cps},
      num_done_{0},
      num_failure_{0} {}

TransLogCollect::~TransLogCollect() {
  TransLogCollect::cancel();
}

int TransLogCollect::execute() {
  if (cps_.num) {
    for (int i = 0; i < cps_.num; ++i) {
      LogPipeHandler* cp = log_ctrl_->get_generic_cp(cps_.modems[i]);
      info_log("%s is execute",
                LogConfig::cp_type_to_name(cps_.modems[i]));
      if (nullptr != cp) {
        start_subsys_col(cp);
      } else {
        err_log("%s is not supported",
                LogConfig::cp_type_to_name(cps_.modems[i]));
      }
    }
  } else {
    auto all_cps = log_ctrl_->get_cps();
    for (auto cp: all_cps) {
      start_subsys_col(cp);
    }
  }

  if (num_done_ == trans_.size()) {  // All finished.
    int result{TRANS_R_SUCCESS};

    if (num_failure_ == num_done_) {  // All failed.
      result = TRANS_R_FAILURE;
    }
    on_finished(result);
    return TRANS_E_SUCCESS;
  }

  // Some transactions are executing.
  on_started();
  return TRANS_E_STARTED;
}

void TransLogCollect::cancel() {
  if (TS_EXECUTING == state()) {
    for (auto ts : trans_) {
      ts->cancel();
    }

    clear_ptr_container(trans_);
    on_canceled();
  }
}

void TransLogCollect::log_col_result(void* client,
                                     Transaction* trans) {
  TransLogCollect* log_col = static_cast<TransLogCollect*>(client);
  ++log_col->num_done_;

  if (TRANS_R_SUCCESS != trans->result()) {
    ++log_col->num_failure_;
  }

  info_log("num_done is %d, num_failure is %d, trans_size is %d",
           log_col->num_done_, log_col->num_failure_, log_col->trans_.size());

  if (log_col->num_done_ < log_col->trans_.size()) {
    return;
  }

  int res = TRANS_R_SUCCESS;
  if (log_col->num_done_ == log_col->num_failure_) {
    res = TRANS_R_FAILURE;
  }

  clear_ptr_container(log_col->trans_);
  log_col->log_ctrl_->flush();
  log_col->finish(res);
}

Transaction* TransLogCollect::create_transaction(LogPipeHandler* log_pipe) {
  Transaction* trans{};

  switch (log_pipe->type()) {
    case CT_WANMODEM:
    case CT_TD:
    case CT_WCDMA:
    case CT_3MODE:
    case CT_4MODE:
    case CT_5MODE:
      trans = new TransModemLogCollect{
          static_cast<WanModemLogHandler*>(log_pipe)};
      break;
#ifdef SUPPORT_WCN
    case CT_WCN:
  #ifdef EXTERNAL_WCN
      trans = new TransExtWcnLogCollect{
          static_cast<ExtWcnLogHandler*>(log_pipe)};
  #else
      trans = new TransIntWcnLogCollect{
          static_cast<IntWcnLogHandler*>(log_pipe)};
  #endif
      break;
#endif
#ifdef SUPPORT_AGDSP
    case CT_AGDSP:
      trans = new TransAgDspLogCollect{
          static_cast<AgDspLogHandler*>(log_pipe)};
      break;
#endif
    case CT_PM_SH:
      trans = new TransPmShLogCollect{
          static_cast<PmSensorhubLogHandler*>(log_pipe)};
      break;
    case CT_GNSS:
      trans = new TransExtGnssLogCollect{
          static_cast<ExtGnssLogHandler*>(log_pipe)};
      break;

    default:
      break;
  }

  return trans;
}

void TransLogCollect::start_subsys_col(LogPipeHandler* log_pipe) {
  Transaction* ts = create_transaction(log_pipe);

  int ret{TRANS_E_SUCCESS};
  if (nullptr != ts) {
    ts->set_client(this, log_col_result);
    trans_.push_back(ts);
    ret = ts->execute();
    info_log("ret = %d", ret);
    switch (ret) {
      case TRANS_E_STARTED:
        break;
      case TRANS_E_SUCCESS:
        ++num_done_;
        break;
      default:  // TRANS_E_ERROR
        ++num_done_;
        ++num_failure_;
        break;
    }
  }
}
