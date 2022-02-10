/*
 *  trans_modem_ver.h - The MODEM software version query transaction.
 *
 *  Copyright (C) 2016 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2016-6-8 Zhang Ziyi
 *  Initial version.
 *
 *  2017-6-2 Zhang Ziyi
 *  Join Transaction class family.
 */

#ifndef _TRANS_MODEM_VER_H_
#define _TRANS_MODEM_VER_H_

#include "cp_log_cmn.h"
#include "diag_stream_parser.h"
#include "timer_mgr.h"
#include "trans_diag.h"

class DiagDeviceHandler;
class WanModemLogHandler;

class TransWanModemVerQuery : public TransDiagDevice {
 public:
  TransWanModemVerQuery(WanModemLogHandler* modem);
  TransWanModemVerQuery(const TransWanModemVerQuery&) = delete;
  ~TransWanModemVerQuery();

  TransWanModemVerQuery& operator = (const TransWanModemVerQuery&) = delete;

  // Transaction::execute()
  int execute() override;
  // Transaction::cancel()
  void cancel() override;

  // TransDiagDevice::process()
  bool process(DataBuffer& buffer) override;

  const char* version(size_t& len) const {
    len = m_ver_len;
    return m_modem_ver;
  }

 private:
  WanModemLogHandler* wan_modem_;
  DiagStreamParser parser_;
  // Max version query times
  int m_max_times;
  // Failed times
  int m_failed_times;
  // Version query timeout
  TimerManager::Timer* m_timer;
  // MODEM version
  size_t m_ver_len;
  char* m_modem_ver;

  int start_query();
  int send_query();

  static void ver_query_timeout(void* param);
};

#endif  // !_TRANS_MODEM_VER_H_
