/*
 *  trans_mgr.h - The transaction manager class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-5-17 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_MGR_H_
#define TRANS_MGR_H_

#include "cp_log_cmn.h"

class Transaction;

class TransactionManager {
 public:
  TransactionManager() {}
  TransactionManager(const TransactionManager&) = delete;
  virtual ~TransactionManager();

  TransactionManager& operator = (const TransactionManager&) = delete;

  size_t number() const { return transactions_.size(); }

  void enqueue(Transaction* trans) {
    transactions_.push_back(trans);
  }

  void cancel_trans(Transaction* trans);

  /*  trans_finished - transaction finished.
   *
   *  This function will be called when a transaction is finished.
   *  trans_finished will remove the transaction from transactions_.
   *
   *  Derived classes shall override the function and usually call
   *  TransactionManager::trans_finished() at first.
   */
  virtual void trans_finished(Transaction* trans);

 protected:
  /*  remove - remove the transaction from the queue.
   *
   */
  void remove(Transaction* trans);
  virtual void on_trans_canceled();

 protected:
  LogList<Transaction*> transactions_;
};

#endif  // !TRANS_MGR_H_
