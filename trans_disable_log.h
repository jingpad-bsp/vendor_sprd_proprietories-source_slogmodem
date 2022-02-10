/*
 *  trans_disable_log.h - The disable log transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-6-22 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_DISABLE_LOG_H_
#define TRANS_DISABLE_LOG_H_

#include "trans_global.h"

class ExtWcnLogHandler;
class LogController;
class LogPipeHandler;
class TransStopCellularLog;
class TransStopWcnLog;

class TransDisableLog : public TransGlobal {
 public:
  TransDisableLog(LogController* ctrl);
  TransDisableLog(const TransDisableLog&) = delete;
  ~TransDisableLog();

  TransDisableLog& operator = (const TransDisableLog&) = delete;

  void add(LogPipeHandler* cp);

  int execute() override;
  void cancel() override;

 private:
  static void stop_cellular_log_result(void* client, Transaction* trans);
#if defined(SUPPORT_WCN) && defined(EXTERNAL_WCN)
  static void stop_wcn_log_result(void* client, Transaction* trans);
#endif

 private:
  LogController* log_ctrl_;
  LogVector<LogPipeHandler*> subsys_;
  WanModemLogHandler* wan_modem_;
  TransStopCellularLog* stop_cellular_log_trans_;
#if defined(SUPPORT_WCN) && defined(EXTERNAL_WCN)
  ExtWcnLogHandler* ext_wcn_;
  TransStopWcnLog* stop_wcn_log_trans_;
#endif
  unsigned finished_num_;
};

#endif  // !TRANS_DISABLE_LOG_H_
