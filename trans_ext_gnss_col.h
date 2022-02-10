/*
 *  trans_ext_gnss_col.h - The Gnss  log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2018-10-28 Gloria He
 *  Initial version.
 */

#ifndef TRANS_EXT_GNSS_COL_H_
#define TRANS_EXT_GNSS_COL_H_

#include "trans_modem.h"

class ExtGnssLogHandler;

class TransExtGnssLogCollect : public TransModem {
 public:
  TransExtGnssLogCollect(ExtGnssLogHandler* extgnss);
  TransExtGnssLogCollect(const TransExtGnssLogCollect&) = delete;

  ~TransExtGnssLogCollect();
  TransExtGnssLogCollect& operator = (const TransExtGnssLogCollect&) = delete;

  int execute() override;
  void cancel() override;

 private:
  int start_log_convey();

  static void log_convey_fin(void* client, Transaction* trans);
  static void dump_end_callback(void* client);

 private:
  ExtGnssLogHandler* extgnss_;
  TransLogConvey* modem_convey_;

};

#endif  // !TRANS_EXT_GNSS_COL_H_

