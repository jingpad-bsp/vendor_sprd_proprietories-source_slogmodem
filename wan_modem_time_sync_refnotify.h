/*
 * wan_modem_time_sync_refnotify.h - commute with refnotify by socket,
 *                               and receive wan modem time from it. after getting modem time
 *                               notify father class the updated time.
 *
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * History:
 * 2018-09-28 Gloria He
 * intial version.
 */

#ifndef WAN_MODEM_TIME_SYNC_REFNOTIFY_H_
#define WAN_MODEM_TIME_SYNC_REFNOTIFY_H_

#include "data_proc_hdl.h"
#include "timer_mgr.h"
#include "wan_modem_time_sync.h"

class LogController;
class Multiplexer;
class WanModemLogHandler;

class WanModemTimeSyncRefnotify :
 public WanModemTimeSync,private DataProcessHandler {

 public:
  WanModemTimeSyncRefnotify(LogController* ctrl, Multiplexer* multi);

  WanModemTimeSyncRefnotify(const WanModemTimeSyncRefnotify&) = delete;

  ~WanModemTimeSyncRefnotify();

  WanModemTimeSyncRefnotify& operator = (const WanModemTimeSyncRefnotify&) = delete;

  void on_modem_alive() override;
  void on_modem_assert() override;

  /* socket to connect refnotify server,
   * make ready for receiving the time info from refnotify
   * retrun 0 if success
   */
  int start() override;

 private:
  static const size_t WAN_MODEM_TIME_SYNC_BUF_SIZE = 64;

 private:
  TimerManager::Timer* m_timer;

  int process_data() override;
  void process_conn_closed() override;
  void process_conn_error(int err) override;

  static void connect_server(void* param);
};

#endif // !WAN_MODEM_TIME_SYNC_REFNOTIFY_H_
