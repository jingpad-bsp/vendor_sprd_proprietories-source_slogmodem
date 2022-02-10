/*
 *  trans_last_col.h - The last log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-8-9 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_LAST_LOG_H_
#define TRANS_LAST_LOG_H_

#include "timer_mgr.h"
#include "trans_modem.h"
#include "wan_modem_log.h"

class TransModemLastLog : public TransModem {
 public:
  TransModemLastLog(WanModemLogHandler* wan);
  TransModemLastLog(const TransModemLastLog&) = delete;
  ~TransModemLastLog();

  TransModemLastLog& operator = (const TransModemLastLog&) = delete;

  int execute() override;
  void cancel() override;

 private:
  static void last_log_timeout(void* client);

 private:
  WanModemLogHandler* wan_modem_;
  TimerManager::Timer* last_log_timer_;
};

#endif  // !TRANS_LAST_LOG_H_
