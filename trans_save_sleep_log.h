/*
 *  trans_save_sleep_log.h - The MODEM sleep log transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-5-31 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_SAVE_SLEEP_LOG_H_
#define TRANS_SAVE_SLEEP_LOG_H_

#include <memory>

#include "diag_stream_parser.h"
#include "timer_mgr.h"
#include "trans_diag.h"

class CpStorage;
class LogFile;
class WanModemLogHandler;

class TransSaveSleepLog : public TransDiagDevice {
 public:
  TransSaveSleepLog(WanModemLogHandler* wan, CpStorage& stor);
  TransSaveSleepLog(const TransSaveSleepLog&) = delete;
  ~TransSaveSleepLog();

  TransSaveSleepLog& operator = (const TransSaveSleepLog&) = delete;

  // Transaction::execute()
  int execute() override;
  // Transaction::cancel()
  void cancel() override;

  // TransDiagDevice::process()
  bool process(DataBuffer& buffer) override;

 private:
  int start();
  std::weak_ptr<LogFile> open_file();
  void close_file();
  void close_rm_file();

  static void sleep_log_read_timeout(void* param);

 private:
  WanModemLogHandler* wan_modem_;
  CpStorage& storage_;
  DiagStreamParser parser_;
  TimerManager::Timer* m_timer;
  std::weak_ptr<LogFile> m_file;
};

#endif  // !TRANS_SAVE_SLEEP_LOG_H_
