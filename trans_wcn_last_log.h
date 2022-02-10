/*
 *  trans_wcn_last_col.h - The WCN last log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-11-16 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_WCN_LAST_LOG_H_
#define TRANS_WCN_LAST_LOG_H_

#include "timer_mgr.h"
#include "trans_global.h"

class LogController;

class TransWcnLastLog : public TransGlobal {
 public:
  TransWcnLastLog(LogController* ctrl, LogPipeHandler* wcn);
  TransWcnLastLog(const TransWcnLastLog&) = delete;
  ~TransWcnLastLog();

  TransWcnLastLog& operator = (const TransWcnLastLog&) = delete;
  TransWcnLastLog& operator = (TransWcnLastLog&&) = delete;

  int execute() override;
  void cancel() override;

 private:
  static void last_log_timeout(void* client);

 private:
  LogController* log_ctrl_;
  LogPipeHandler* wcn_;
  TimerManager::Timer* last_log_timer_;
};

#endif  // !TRANS_WCN_LAST_LOG_H_
