/*
 *  trans_stop_wcn_log.h - The WCN log stop transaction class.
 *
 *  Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2018-5-14 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_STOP_WCN_LOG_H_
#define TRANS_STOP_WCN_LOG_H_

#include "trans_modem.h"

class ExtWcnLogHandler;

class TransStopWcnLog : public TransModem {
 public:
  TransStopWcnLog(ExtWcnLogHandler* wcn);
  TransStopWcnLog(const TransStopWcnLog&) = delete;
  ~TransStopWcnLog();

  TransStopWcnLog& operator = (const TransStopWcnLog&) = delete;

  int execute() override;
  void cancel() override;

 private:
  ExtWcnLogHandler* wcn_;
};

#endif  // !TRANS_STOP_WCN_LOG_H_
