/*
 *  trans_enable_log.cpp - The enable log transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-6-20 Zhang Ziyi
 *  Initial version.
 */

#include "log_config.h"
#include "log_ctrl.h"
#include "trans_enable_log.h"
#include "trans_normal_log.h"
#include "wan_modem_log.h"

TransEnableLog::TransEnableLog(LogController* ctrl)
    :TransGlobal{ctrl, ENABLE_LOG},
     log_ctrl_{ctrl},
     wan_modem_{nullptr},
     normal_log_trans_{nullptr},
     finished_num_{0} {}

TransEnableLog::~TransEnableLog() {
  TransEnableLog::cancel();
}

void TransEnableLog::add(LogPipeHandler* cp) {
  subsys_.push_back(cp);
}

int TransEnableLog::execute() {
  LogConfig* config = log_ctrl_->config();

  for (auto subsys: subsys_) {
    CpType t = subsys->type();
    if (t >= CT_WCDMA && t <= CT_5MODE) {  // Cellular MODEM
      WanModemLogHandler* modem = static_cast<WanModemLogHandler*>(subsys);
      int err = modem->start_normal_log(this, start_normal_log_result,
                                        normal_log_trans_);
      switch (err) {
        case Transaction::TRANS_E_SUCCESS:  // Finished
          delete normal_log_trans_;
          normal_log_trans_ = nullptr;
          config->set_log_mode(t, LogConfig::LM_NORMAL);
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
    } else {  // Non cellular MODEM
      subsys->start();
      ++finished_num_;
      config->set_log_mode(t, LogConfig::LM_NORMAL);
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

void TransEnableLog::cancel() {
  if (TS_EXECUTING == state()) {
    if (wan_modem_) {
      wan_modem_->cancel_trans(normal_log_trans_);
      delete normal_log_trans_;
      normal_log_trans_ = nullptr;
      wan_modem_ = nullptr;
    }
    on_canceled();
  }
}

void TransEnableLog::start_normal_log_result(void* client,
                                             Transaction* trans) {
  TransEnableLog* en_log = static_cast<TransEnableLog*>(client);

  if (TS_EXECUTING == en_log->state() && en_log->wan_modem_) {
    LogConfig* config = en_log->log_ctrl_->config();

    ++en_log->finished_num_;
    int res = trans->result();
    delete trans;
    en_log->normal_log_trans_ = nullptr;
    if (TRANS_R_SUCCESS == res) {
      config->set_log_mode(en_log->wan_modem_->type(),
                           LogConfig::LM_NORMAL);
      config->save();
    } else {
      err_log("start normal log failed");
    }
    en_log->finish(res);
  } else {
    err_log("start normal log result got on wrong state %d",
            en_log->state());
  }
}
