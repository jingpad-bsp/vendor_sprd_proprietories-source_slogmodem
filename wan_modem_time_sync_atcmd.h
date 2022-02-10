/*
 * wan_modem_time_sync_atcmd.h - To get  wan modem time by at cmd
 *                             and handler of updating wan modem log file
 *                             time information.
 *
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * History:
 * 2018-09-28 Gloria He
 * intial version.
 */

#ifndef WAN_MODEM_TIME_SYNC_ATCMD_H_
#define WAN_MODEM_TIME_SYNC_ATCMD_H_

#include "timer_mgr.h"
#include "wan_modem_time_sync.h"

class Transaction;
class WanModemLogHandler;
class TransModemTimeSync;

class WanModemTimeSyncAtCmd : public WanModemTimeSync {
 public:
  WanModemTimeSyncAtCmd(WanModemLogHandler* modem);
  WanModemTimeSyncAtCmd(const WanModemTimeSyncAtCmd&) = delete;

  ~WanModemTimeSyncAtCmd();
  WanModemTimeSyncAtCmd& operator = (const WanModemTimeSyncAtCmd&) = delete;

  /* using atcmd for modem time sync query,
   * send at cmd to query time and make ready for receiving the time info from modem
   * retrun 0 if success,other fail
   */
  int start() override;
  void on_modem_alive() override;
  void on_modem_assert() override;

 private:
  enum ClientTransState {
    CTS_NOT_BEGIN,
    CTS_EXECUTING,
    CTS_START_FAIL,
    CTS_EXECUTING_FAIL,
    CTS_SUCCESS
  };
  int start_time_sync();
  static void trans_time_sync_result(void* client, Transaction* trans);
  static void start_query(void* param);

 private:
  bool in_query_;
  ClientTransState trans_state_;
  TransModemTimeSync* trans_ts_;
  TimerManager::Timer* timer_;
  WanModemLogHandler* wan_modem_;
};

#endif // !WAN_MODEM_TIME_SYNC_ATCMD_H_
