/*
 *  trans_dump_rcv.h - The cellular MODEM and PM Sensorhub dump reveiver class.
 *
 *  Copyright (C) 2015-2018 Spreadtrum Communications Inc.
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *
 *  2015-6-15 Zhang Ziyi
 *  Initial version.
 *
 *  2016-7-13 Yan Zhihang
 *  Dump consumer to support both modem and pm sensorhub.
 *
 *  2017-6-3 Zhang Ziyi
 *  Join the Transaction class family.
 *
 *  2019-4-10 Elon li
 *  Only do data receiving transactions.
 */

#ifndef TRANS_DUMP_RCV_H_
#define TRANS_DUMP_RCV_H_

#include "cp_dump.h"
#include "diag_stream_parser.h"
#include "timer_mgr.h"

class LogFile;

class TransDumpReceiver : public TransCpDump {
 public:
  using DumpOngoingCallback = void (*)(void*);

  TransDumpReceiver(TransactionManager* tmgr,
                    LogPipeHandler* subsys, CpStorage& cp_stor,
                    const struct tm& lt, const char* prefix,
                    const char* end_of_content, uint8_t cmd);

  TransDumpReceiver(const TransDumpReceiver&) = delete;

  ~TransDumpReceiver();

  TransDumpReceiver& operator = (const TransDumpReceiver&) = delete;

  // Transaction::execute()
  int execute() override;
  // Transaction::cancel()
  void cancel() override;

  // TransDiagDevice::process()
  bool process(DataBuffer& buffer) override;

  // Register ongoing periodic event
  void set_periodic_callback(void* client, DumpOngoingCallback cb) {
    client_ = client;
    on_going_cb_ = cb;
  }

 private:
  int start_dump();

  bool check_ending(DataBuffer& buffer);

  /*  notify_stor_inactive - callback when external storage umounted.
   */
  static void notify_stor_inactive(void* client, unsigned priority);

  static void dump_read_check(void* param);

 private:
  TimerManager::Timer* m_timer;
  DiagStreamParser parser_;
  const char* m_end_of_content;
  size_t m_end_content_size;
  const uint8_t m_cmd;
  // Whether data are received in the most recent time window
  bool dump_ongoing_;
  // The number of adjacent data silent windows
  int silent_wins_;
  void* client_;
  DumpOngoingCallback on_going_cb_;
};

#endif  // !TRANS_DUMP_RCV_H_
