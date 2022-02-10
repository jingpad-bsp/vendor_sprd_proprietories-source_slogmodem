/*
 *  trans_disable_log.cpp - The disable log transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-6-22 Zhang Ziyi
 *  Initial version.
 */

#include "ext_wcn_log.h"
#include "log_config.h"
#include "log_ctrl.h"
#include "trans_disable_log.h"
#include "trans_stop_cellular_log.h"
#if defined(SUPPORT_WCN) && defined(EXTERNAL_WCN)
#include "trans_stop_wcn_log.h"
#endif
#include "wan_modem_log.h"

TransDisableLog::TransDisableLog(LogController* ctrl)
    :TransGlobal{ctrl, DISABLE_LOG},
     log_ctrl_{ctrl},
     wan_modem_{},
     stop_cellular_log_trans_{},
#if defined(SUPPORT_WCN) && defined(EXTERNAL_WCN)
     ext_wcn_{},
     stop_wcn_log_trans_{},
#endif
     finished_num_{} {}

TransDisableLog::~TransDisableLog() {
  TransDisableLog::cancel();
}

void TransDisableLog::add(LogPipeHandler* cp) {
  subsys_.push_back(cp);
}

int TransDisableLog::execute() {
  LogConfig* config = log_ctrl_->config();

  for (auto subsys: subsys_) {
    CpType t = subsys->type();
    if (t >= CT_WCDMA && t <= CT_5MODE) {  // Cellular MODEM
      WanModemLogHandler* modem = static_cast<WanModemLogHandler*>(subsys);
      int err = modem->stop_log(this, stop_cellular_log_result,
                                stop_cellular_log_trans_);
      switch (err) {
        case Transaction::TRANS_E_SUCCESS:  // Finished
          delete stop_cellular_log_trans_;
          stop_cellular_log_trans_ = nullptr;
          config->set_log_mode(t, LogConfig::LM_OFF);
          ++finished_num_;
          break;
        case Transaction::TRANS_E_STARTED:  // Started
          wan_modem_ = modem;
          on_started();
          break;
        default:  // Error (also finished)
          ++finished_num_;
          break;
      }
    }
#if defined(SUPPORT_WCN) && defined(EXTERNAL_WCN)
    else if (CT_WCN == t) {
      ExtWcnLogHandler* wcn = static_cast<ExtWcnLogHandler*>(subsys);
      int err = wcn->stop_log(this, stop_wcn_log_result,
                              stop_wcn_log_trans_);
      switch (err) {
        case Transaction::TRANS_E_SUCCESS:  // Finished
          delete stop_wcn_log_trans_;
          stop_wcn_log_trans_ = nullptr;
          config->set_log_mode(t, LogConfig::LM_OFF);
          ++finished_num_;
          break;
        case Transaction::TRANS_E_STARTED:  // Started
          ext_wcn_ = wcn;
          on_started();
          break;
        default:  // Error (also finished)
          ++finished_num_;
          break;
      }
    }
#endif
    else {  // Non cellular MODEM
      subsys->stop();
      ++finished_num_;
      config->set_log_mode(t, LogConfig::LM_OFF);
    }
  }

  if (config->dirty()) {
    config->save();
  }

  int ret {TRANS_E_STARTED};
  if (finished_num_ == subsys_.size()) {
    on_finished(TRANS_R_SUCCESS);
    ret = TRANS_E_SUCCESS;
  }

  return ret;
}

void TransDisableLog::cancel() {
  if (TS_EXECUTING == state()) {
    if (wan_modem_) {
      if (stop_cellular_log_trans_) {
        wan_modem_->cancel_trans(stop_cellular_log_trans_);
        delete stop_cellular_log_trans_;
        stop_cellular_log_trans_ = nullptr;
      }
      wan_modem_ = nullptr;
    }
#if defined(SUPPORT_WCN) && defined(EXTERNAL_WCN)
    if (ext_wcn_) {
      if (stop_wcn_log_trans_) {
        ext_wcn_->cancel_trans(stop_wcn_log_trans_);
        delete stop_wcn_log_trans_;
        stop_wcn_log_trans_ = nullptr;
      }
      ext_wcn_ = nullptr;
    }
#endif
    on_canceled();
  }
}

void TransDisableLog::stop_cellular_log_result(void* client,
                                               Transaction* trans) {
  TransDisableLog* stop_log = static_cast<TransDisableLog*>(client);

  if (TS_EXECUTING == stop_log->state() && stop_log->wan_modem_) {
    LogConfig* config = stop_log->log_ctrl_->config();

    ++stop_log->finished_num_;
    int res = trans->result();
    delete trans;
    stop_log->stop_cellular_log_trans_ = nullptr;
    if (TRANS_R_SUCCESS == res) {
      config->set_log_mode(stop_log->wan_modem_->type(),
                           LogConfig::LM_OFF);
      config->save();
    } else {
      err_log("stop log failed");
    }
    stop_log->finish(res);
  } else {
    err_log("stop log result got on wrong state %d",
            stop_log->state());
  }
}

#if defined(SUPPORT_WCN) && defined(EXTERNAL_WCN)
void TransDisableLog::stop_wcn_log_result(void* client,
                                          Transaction* trans) {
  TransDisableLog* stop_log = static_cast<TransDisableLog*>(client);

  if (TS_EXECUTING == stop_log->state() && stop_log->ext_wcn_) {
    LogConfig* config = stop_log->log_ctrl_->config();

    ++stop_log->finished_num_;
    int res = trans->result();
    delete trans;
    stop_log->stop_wcn_log_trans_ = nullptr;
    if (TRANS_R_SUCCESS == res) {
      config->set_log_mode(stop_log->ext_wcn_->type(),
                           LogConfig::LM_OFF);
      config->save();
    } else {
      err_log("stop log failed");
    }
    stop_log->finish(res);
  } else {
    err_log("stop log result got on wrong state %d",
            stop_log->state());
  }
}
#endif
