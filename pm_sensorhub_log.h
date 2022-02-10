/*
 *  pm_sensorhub_log.h - The power management, sensorhub log
 *                       and dump handler class declaration.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2016-3-2 YAN Zhihang
 *  Initial version.
 */

#ifndef PM_SENSORHUB_LOG_H_
#define PM_SENSORHUB_LOG_H_

#include "log_pipe_hdl.h"
#include "timer_mgr.h"

class Transaction;

class PmSensorhubLogHandler : public LogPipeHandler {
 public:
  PmSensorhubLogHandler(LogController* ctrl, Multiplexer* multi,
                        const LogConfig::ConfigEntry* conf,
                        StorageManager& stor_mgr, CpClass cp_class);
  ~PmSensorhubLogHandler();

  PmSensorhubLogHandler(const PmSensorhubLogHandler&) = delete;

  PmSensorhubLogHandler& operator = (const PmSensorhubLogHandler&) = delete;

  /*  start - override the virtual function for starting power
   *          management and sensorhub log.
   *
   *  Return Value:
   *    Return 0 if pm and sensorhub log start successfully,
   */
  int start(LogConfig::LogMode mode = LogConfig::LM_NORMAL) override;
  /*
   *  stop - Stop logging.
   *
   *  This function put the LogPipeHandler in an adminitrative
   *  disable state and close the log device.
   *
   *  Return Value:
   *    0
   */
  int stop() override;

  /*  crash - crash the PM/Sensor Hub administratively.
   *  Return 0 on success, -1 on error.
   */
  int crash();

  void trans_finished(Transaction* trans) override;

  /*
   *    process_assert - Process the PM/Sensor Hub assertion.
   *
   *    This function will be called by the LogController when the
   *    CP asserts.
   */
  void process_assert(const struct tm& lt,
                      CpStateHandler::CpEvent evt) override;

 private:
  /*
   *  check_ctrl_file - check whether the control file is opened.
   *
   *  Return Value:
   *    0 if the control file is opened, -1 otherwise.
   */
  int check_ctrl_file();

  /*
   *  log_switch - open or close the log
   *  @on: true - turn on log output, false - turn off log output.
   *
   *  @on: open log if true, otherwise close log
   *
   *  Return Value:
   *    0 if success else -1.
   */
  int log_switch(bool on = true);

  /*
   *  start_dump - start power management and sensorhub subsystem dump.
   *  @lt: assertion time.
   *  @result: dump saving result.
   *
   *  Return Value:
   *    Transaction::TRANS_E_SUCCESS: dump finished.
   *    Transaction::TRANS_E_STARTED: dump started and not finished.
   *    Transaction::TRANS_E_ERROR: failed to start dump.
   */
  int start_dump(const struct tm& lt, CpStateHandler::CpEvent evt);

  static void enable_output_timer(void* client);

  static void dump_result(void* client, Transaction* trans);

 private:
  // Control file (PM_LOG_CTRL) descriptor
  int ctrl_file_;
  // Timer for reenabling log output
  TimerManager::Timer* reopen_timer_;
};

#endif  // !PM_SENSORHUB_LOG_H_
