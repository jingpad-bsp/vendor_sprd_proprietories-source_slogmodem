/*
 *  trans_mgr.cpp - The transaction manager class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-5-17 Zhang Ziyi
 *  Initial version.
 */

#include "trans.h"
#include "trans_mgr.h"

TransactionManager::~TransactionManager() {
  clear_ptr_container(transactions_);
}

void TransactionManager::cancel_trans(Transaction* trans) {
  ll_remove(transactions_, trans);

  trans->cancel();

  on_trans_canceled();
}

void TransactionManager::on_trans_canceled() {
  while (!transactions_.empty()) {
    Transaction* trans = transactions_.front();
    if (Transaction::TS_NOT_BEGIN != trans->state()) {
      break;
    }
    int err = trans->execute();
    if (Transaction::TRANS_E_STARTED == err) {
      break;
    }
    transactions_.pop_front();
    if (trans->has_client()) {
      trans->notify_result();
    } else {
      delete trans;
    }
  }
}

void TransactionManager::trans_finished(Transaction* trans) {
  ll_remove(transactions_, trans);
  if (trans->has_client()) {
    trans->notify_result();
  }
}

void TransactionManager::remove(Transaction* trans) {
  ll_remove(transactions_, trans);
}
