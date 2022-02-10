/*
 *  trans_pmsh_col.h - The PM/Sensor Hub log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-10-13 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_PMSH_COL_H_
#define TRANS_PMSH_COL_H_

#include "pm_sensorhub_log.h"
#include "trans_modem.h"

class TransPmShLogCollect : public TransModem {
 public:
  TransPmShLogCollect(PmSensorhubLogHandler* pmsh);
  TransPmShLogCollect(const TransPmShLogCollect&) = delete;

  ~TransPmShLogCollect();
  TransPmShLogCollect& operator = (const TransPmShLogCollect&) = delete;

  int execute() override;
  void cancel() override;

 private:
  int start_log_convey();

  static void log_convey_fin(void* client, Transaction* trans);
  static void dump_end_callback(void* client);

 private:
  PmSensorhubLogHandler* pmsh_;
  TransLogConvey* modem_convey_;

};

#endif  // !TRANS_PMSH_COL_H_
