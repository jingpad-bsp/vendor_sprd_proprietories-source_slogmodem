/*
 *  trans_orca_dump.cpp - The ORCA MODEM/PM-SensorHub dump class.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *
 *  2019-4-10 Elon li
 *  Initial version.
 */

#ifndef TRANS_ORCA_DUMP_H_
#define TRANS_ORCA_DUMP_H_

#include "cp_dump.h"
#include "cp_log_cmn.h"
#include "cp_stat_hdl.h"
#include "def_config.h"
#include "diag_stream_parser.h"
#include "modem_utils.h"
#include "timer_mgr.h"
#include "trans_dump_rcv.h"

class TransOrcaDump : public TransCpDump {
 public:
  using DumpOngoingCallback = void (*)(void*);

  TransOrcaDump(TransactionManager* tmgr,
                LogPipeHandler* subsys, CpStorage& cp_stor,
                const struct tm& lt, CpStateHandler::CpEvent evt,
                const char* prefix, const char* end_of_content,
                uint8_t cmd);
  TransOrcaDump(const TransOrcaDump&) = delete;

  ~TransOrcaDump();

  TransOrcaDump& operator = (const TransOrcaDump&) = delete;

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
  enum FaultSubsystem {
    FS_ORCA_SYS,
    FS_MODEM,
    FS_MODEM_DP,
    FS_PM_SH
  };

 private:
  bool save_mem_file();
  static void recv_result(void* client, Transaction* trans);
  static void ongoing_report(void* client);

 private:
  struct tm time_;
  CpStorage& cp_stor_;
  const char* end_of_content_;
  const uint8_t cmd_;
  const char* prefix_;
  CpStateHandler::CpEvent evt_;
  FaultSubsystem fault_subsys_;
  LogString dump_path_;
  LogPipeHandler* subsys_;
  void* client_;
  DumpOngoingCallback ongoing_cb_;
  TransDumpReceiver* dump_rcv_;
};

#endif  // !TRANS_ORCA_DUMP_H_
