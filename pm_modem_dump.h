/*
 *  pm_modem_dump.cpp - The cellular MODEM and PM Sensorhub dump class..
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *
 *  2019-4-10 Elon li
 *  Initial version.
 */

#ifndef _PM_MODEM_DUMP_H_
#define _PM_MODEM_DUMP_H_

#include "cp_dump.h"
#include "def_config.h"
#include "diag_stream_parser.h"
#include "timer_mgr.h"
#include "trans_dump_rcv.h"

class TransPmModemDump : public TransCpDump {
 public:
  using DumpOngoingCallback = void (*)(void*);

  TransPmModemDump(TransactionManager* tmgr,
                   LogPipeHandler* subsys, CpStorage& cp_stor,
                   const struct tm& lt, const char* dump_path,
                   const char* prefix, const char* end_of_content,
                   uint8_t cmd);
  TransPmModemDump(const TransPmModemDump&) = delete;

  ~TransPmModemDump();

  TransPmModemDump& operator = (const TransPmModemDump&) = delete;

  // Transaction::execute()
  int execute() override;
  // Transaction::cancel()
  void cancel() override;

  // TransDiagDevice::process()
  bool process(DataBuffer& buffer) override { return false; }

  void set_periodic_callback(void* client, DumpOngoingCallback cb) {
    client_ = client;
    ongoing_cb_ = cb;
  }

private:
  bool save_mem_file();
  static void recv_result(void* client, Transaction* trans);
  static void ongoing_report(void* client);

 private:
  const char* dump_path_;
  struct tm time_;
  CpStorage& cp_stor_;
  const char* end_of_content_;
  const uint8_t cmd_;
  const char* prefix_;
  LogPipeHandler* subsys_;
  void* client_;
  DumpOngoingCallback ongoing_cb_;
  TransDumpReceiver* dump_rcv_;
};

#endif  // !_PM_MODEM_DUMP_H_
