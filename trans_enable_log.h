/*
 *  trans_enable_log.h - The enable log transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-6-20 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_ENABLE_LOG_H_
#define TRANS_ENABLE_LOG_H_

#include "trans_global.h"

class LogController;
class LogPipeHandler;
class TransStartNormalLog;

class TransEnableLog : public TransGlobal {
 public:
  TransEnableLog(LogController* ctrl);
  TransEnableLog(const TransEnableLog&) = delete;
  ~TransEnableLog();

  TransEnableLog& operator = (const TransEnableLog&) = delete;

  void add(LogPipeHandler* cp);

  int execute() override;
  void cancel() override;

 private:
  static void start_normal_log_result(void* client, Transaction* trans);

 private:
  LogController* log_ctrl_;
  LogVector<LogPipeHandler*> subsys_;
  WanModemLogHandler* wan_modem_;
  TransStartNormalLog* normal_log_trans_;
  unsigned finished_num_;
};

#endif  // !TRANS_ENABLE_LOG_H_
