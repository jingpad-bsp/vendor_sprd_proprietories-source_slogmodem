/*
 *  trans_save_evt_log.h - The MODEM event log transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-6-13 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_SAVE_EVT_LOG_H_
#define TRANS_SAVE_EVT_LOG_H_

#include "dev_file_hdl.h"
#include "modem_at_ctrl.h"
#include "timer_mgr.h"
#include "trans_modem.h"

class CpStorage;
class LogFile;
class WanModemLogHandler;

class TransSaveEventLog : public TransModem,
                          public DeviceFileHandler {
 public:
  TransSaveEventLog(WanModemLogHandler* wan, CpStorage& stor,
                    int sim, int evt, const CalendarTime& t);
  TransSaveEventLog(const TransSaveEventLog&) = delete;
  ~TransSaveEventLog();

  TransSaveEventLog& operator = (const TransSaveEventLog&) = delete;

  int sim() const { return sim_; }
  int event() const { return event_; }

  void set_time_alignment(const timeval& tnow, uint32_t cnt);

  // Transaction::execute()
  int execute() override;
  // Transaction::cancel()
  void cancel() override;

 private:
  struct EventFileInfo {
    int event;
    const char* suffix;

    bool operator == (int evt) const { return event == evt; }
  };

  std::weak_ptr<LogFile> create_file();
  void close_rm_file();
  int send_command();

  // DeviceFileHandler::process_data()
  bool process_data(DataBuffer& buffer) override;

  void on_timeout();

  static void evt_log_read_timeout(void* param);
  static void data_idle_timeout(void* param);
  static void event_log_cmd_result(void* client,
                                   ModemAtController::ActionType t,
                                   int result);

 private:
  WanModemLogHandler* wan_modem_;
  CpStorage& storage_;
  int sim_;
  int event_;
  CalendarTime evt_time_;
  // AP/MODEM time alignment
  bool has_ta_;
  modem_timestamp time_alignment_;
  //DiagStreamParser parser_;
  // The timer for max read time
  TimerManager::Timer* read_timer_;
  // Data idle timer
  TimerManager::Timer* idle_timer_;
  std::weak_ptr<LogFile> evt_file_;
  bool at_finished_;
  int at_result_;
  // Data export finished
  bool data_finished_;
  // Data export successful
  bool data_ok_;
  size_t log_size_;

  static const int kDataBufSize = 1024 * 256;
};

#endif  // !TRANS_SAVE_EVT_LOG_H_
