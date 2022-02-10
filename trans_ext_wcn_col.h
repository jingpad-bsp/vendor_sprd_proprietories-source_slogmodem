/*
 *  trans_ext_wcn_col.h - The external wcn log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-5-16 YAN Zhihang
 *  Initial version.
 */

#ifndef TRANS_EXT_WCN_COL_H_
#define TRANS_EXT_WCN_COL_H_

#include "trans_modem.h"

class ExtWcnLogHandler;

class TransExtWcnLogCollect : public TransModem {
 public:
  TransExtWcnLogCollect(ExtWcnLogHandler* wan);
  TransExtWcnLogCollect(const TransExtWcnLogCollect&) = delete;
  ~TransExtWcnLogCollect();

  TransExtWcnLogCollect& operator = (const TransExtWcnLogCollect&) = delete;

  int execute() override;
  void cancel() override;

 private:
  int start_log_convey();

  static void dump_end_callback(void* client);
  static void log_collect_fin_callback(void* client, Transaction* trans);

 private:
  ExtWcnLogHandler* ext_wcn_;
  TransLogConvey* modem_convey_;
};

#endif  // !TRANS_EXT_WCN_COL_H_
