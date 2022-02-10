/*
 *  ext_wcn_dump.h - The external WCN dump class.
 *
 *  Copyright (C) 2015-2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-6-13 Zhang Ziyi
 *  Initial version.
 *
 *  2017-6-3 Zhang Ziyi
 *  Join the Transaction class family.
 */

#ifndef _EXT_WCN_DUMP_H_
#define _EXT_WCN_DUMP_H_

#include "cp_dump.h"
#include "timer_mgr.h"

class ExtWcnLogHandler;
class LogFile;

class TransExtWcnDump : public TransCpDump {
 public:
  TransExtWcnDump(TransactionManager* tmgr, ExtWcnLogHandler* wcn,
                  CpStorage& cp_stor, const struct tm& lt,
                  const char* prefix);
  TransExtWcnDump(const TransExtWcnDump&) = delete;

  ~TransExtWcnDump();

  TransExtWcnDump& operator = (const TransExtWcnDump&) = delete;

  // Transaction::execute()
  int execute() override;
  // Transaction::cancel()
  void cancel() override;

  // TransDiagDevice::process()
  bool process(DataBuffer& buffer) override;

 private:
  int start();

  static void dump_read_timeout(void* param);

  /*  notify_stor_inactive - callback when external storage umounted.
  */
  static void notify_stor_inactive(void* client, unsigned priority);

 private:
  TimerManager::Timer* read_timer_;
};

#endif  // !_EXT_WCN_DUMP_H_
