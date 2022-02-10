/*
 *  trans_int_wcn_col.h - The internal wcn log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-5-27 YAN Zhihang
 *  Initial version.
 */

#ifndef TRANS_INT_WCN_COL_H_
#define TRANS_INT_WCN_COL_H_

#include "timer_mgr.h"
#include "trans.h"

class IntWcnLogHandler;

class TransIntWcnLogCollect : public TransGlobal {
 public:
  TransIntWcnLogCollect(IntWcnLogHandler* wan);
  TransIntWcnLogCollect(const TransIntWcnLogCollect&) = delete;
  ~TransIntWcnLogCollect();

  TransIntWcnLogCollect& operator = (const TransIntWcnLogCollect&) = delete;

  int execute() override;
  void cancel() override;

 private:
  static void dump_start_callback(void* client);
  static void dump_end_callback(void* client);
  static void dump_timer(void* client);

 private:
  IntWcnLogHandler* int_wcn_;
  TimerManager::Timer* dump_timer_;
};

#endif  // !TRANS_INT_WCN_COL_H_
