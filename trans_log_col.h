/*
 *  trans_log_col.h - The log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-5-17 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_LOG_COL_H_
#define TRANS_LOG_COL_H_

#include "client_req.h"
#include "trans_global.h"

class LogController;
class LogPipeHandler;

class TransLogCollect : public TransGlobal {
 public:
  TransLogCollect(LogController* ctrl, const ModemSet& cps);
  TransLogCollect(const TransLogCollect&) = delete;
  ~TransLogCollect();

  TransLogCollect& operator = (const TransLogCollect&) = delete;

  int execute() override;
  void cancel() override;
  void start_subsys_col(LogPipeHandler* log_pipe);

 private:
  Transaction* create_transaction(LogPipeHandler* log_pipe);

  static void log_col_result(void* client, Transaction* trans);

 private:
  LogController* log_ctrl_;
  ModemSet cps_;
  LogList<Transaction*> trans_;
  unsigned num_done_;
  unsigned num_failure_;
};

#endif  // !TRANS_LOG_COL_H_
