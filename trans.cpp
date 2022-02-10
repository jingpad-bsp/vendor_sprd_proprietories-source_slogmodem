/*
 *  trans.cpp - The transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-5-17 Zhang Ziyi
 *  Initial version.
 */

#include "trans.h"
#include "trans_mgr.h"

Transaction::Transaction(TransactionManager* mgr, Category cat)
    :trans_mgr_{mgr},
     category_{cat},
     state_{TS_NOT_BEGIN},
     result_{TRANS_R_SUCCESS},
     client_{},
     result_cb_{} {}

void Transaction::finish(int res) {
  state_ = TS_FINISHED;
  result_ = res;
  if (trans_mgr_) {
    trans_mgr_->trans_finished(this);
  } else if (has_client()) {
    notify_result();
  }
}
