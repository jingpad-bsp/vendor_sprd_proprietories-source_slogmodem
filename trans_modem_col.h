/*
 *  trans_modem_col.h - The MODEM log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-5-18 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_MODEM_COL_H_
#define TRANS_MODEM_COL_H_

#include "timer_mgr.h"
#include "trans_modem.h"
#include "wan_modem_log.h"

class TransModemLogCollect : public TransModem {
 public:
  TransModemLogCollect(WanModemLogHandler* wan);
  TransModemLogCollect(const TransModemLogCollect&) = delete;
  ~TransModemLogCollect();

  TransModemLogCollect& operator = (const TransModemLogCollect&) = delete;

  int execute() override;
  void cancel() override;

 private:
  int start_log_convey();

  static void log_convey_fin(void* client, Transaction* trans);
  static void dump_end_callback(void* client);
  static void log_flush_callback(void* client);

 private:
  WanModemLogHandler* wan_modem_;
  TransLogConvey* modem_convey_;
  TimerManager::Timer* last_collect_timer_;
};

#endif  // !TRANS_MODEM_COL_H_
