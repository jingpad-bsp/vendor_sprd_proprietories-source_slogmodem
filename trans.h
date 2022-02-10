/*
 *  trans.h - The transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-5-17 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_H_
#define TRANS_H_

class TransactionManager;

class Transaction {
 public:
  enum Category {
    GLOBAL,
    MODEM,
    WCN,
    AGDSP,
    PMSH
  };

  using ResultCallback = void (*)(void*, Transaction*);

  static const int TRANS_E_STARTED = 1;
  static const int TRANS_E_SUCCESS = 0;
  static const int TRANS_E_ERROR = -1;

  // Transaction result code
  //   -100 ~ 0: result code common to all transactions
  //   other:    transaction specific result code
  static const int TRANS_R_SUCCESS = 0;
  static const int TRANS_R_FAILURE = -1;
  static const int TRANS_R_NONEXISTENT_CP = -2;
  static const int TRANS_R_LOG_NOT_ENABLED = -3;
  static const int TRANS_R_CP_ASSERTED = -4;
  static const int TRANS_R_SLEEP_LOG_NOT_SUPPORTED = -5;

  enum State {
    TS_NOT_BEGIN,
    TS_EXECUTING,
    TS_FINISHED,
    TS_CANCELED
  };

  Transaction(TransactionManager* mgr, Category cat);
  Transaction(const Transaction&) = delete;
  virtual ~Transaction() {}

  Transaction& operator = (const Transaction&) = delete;

  Category category() const { return category_; }

  State state() const { return state_; }
  int result() const { return result_; }

  void set_client(void* client, ResultCallback cb) {
    client_ = client;
    result_cb_ = cb;
  }

  bool has_client() const {
    return result_cb_;
  }

  /*  execute - start to execute the transaction.
   *
   *  Return Value:
   *    TRANS_E_STARTED: the transaction is started and not finished.
   *    TRANS_E_SUCCESS: the transaction is finished.
   *    other:           the transaction failed to start.
   */
  virtual int execute() = 0;

  /*  cancel - cancel the transaction.
   */
  virtual void cancel() = 0;

  void notify_result() { result_cb_(client_, this); }

 protected:
  void on_started() { state_ = TS_EXECUTING; }
  void on_finished(int res) {
    state_ = TS_FINISHED;
    result_ = res;
  }
  void on_canceled() { state_ = TS_CANCELED; }

  /*  finish - transit to TS_FINISHED state and inform the TransactionManager
   */
  void finish(int res);

 private:
  TransactionManager* trans_mgr_;
  Category category_;
  State state_;
  int result_;
  void* client_;
  ResultCallback result_cb_;
};

#endif  // !TRANS_H_
