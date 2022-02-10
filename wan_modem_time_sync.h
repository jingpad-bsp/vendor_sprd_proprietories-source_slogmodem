/*
 * wan_modem_time_sync.h - store modem time,
 *                             and handler of updating wan modem log file
 *                             time information.
 *
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * History:
 * 2018-09-28 Gloria He
 * intial version.
 */

#ifndef WAN_MODEM_TIME_SYNC_H_
#define WAN_MODEM_TIME_SYNC_H_

#include "timer_mgr.h"
#include "cp_log_cmn.h"

class LogController;
class Multiplexer;
class WanModemLogHandler;

class WanModemTimeSync {
 public:
  using cp_time_update_callback_t = void (*)(void* client, const time_sync& ts);

  WanModemTimeSync();
  WanModemTimeSync(const WanModemTimeSync&) = delete;
  WanModemTimeSync(WanModemTimeSync&&) = delete;

  virtual ~WanModemTimeSync() = 0;
  WanModemTimeSync& operator = (const WanModemTimeSync&) = delete;
  WanModemTimeSync& operator = (WanModemTimeSync&&) = delete;

  virtual void on_modem_alive() = 0;
  virtual void on_modem_assert();

  /*  start - start AP/MODEM time alignment service.
   *
   *  Derivatives shall try to get MODEM tick count in this function or
   *  later as appropriate from a proper source.
   *
   *  Return Value:
   *    Return 0 on success, and -1 on failure. If this function returns
   *    -1, it won't be able to provide time alignment service.
   */

  virtual int start() = 0;

  bool get_time_sync_info(time_sync& ts);
  bool time_sync_state() const { return ts_state_; }
  void set_time_update_callback(void* client,cp_time_update_callback_t cb);
  void cleanup_time_update_callback();

 protected:
  void on_time_updated(const time_sync& ts);

 private:
  void clear_time_sync_info();

 private:
  bool ts_state_;
  time_sync ts_;
  void* time_sync_client_;
  cp_time_update_callback_t time_update_;
};

#endif // !WAN_MODEM_TIME_SYNC_H_
