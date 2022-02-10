/*
 *  int_wcn_log.h - The internal WCN log and dump handler class declaration.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-8-7 Zhang Ziyi
 *  Initial version.
 */
#ifndef INT_WCN_LOG_H_
#define INT_WCN_LOG_H_

#include "log_pipe_hdl.h"

class IntWcnLogHandler : public LogPipeHandler {
 public:
  IntWcnLogHandler(LogController* ctrl, Multiplexer* multi,
                   const LogConfig::ConfigEntry* conf,
                   StorageManager& stor_mgr, const char* dump_path,
                   CpClass cp_class);
  IntWcnLogHandler(const IntWcnLogHandler&) = delete;

  IntWcnLogHandler& operator = (const IntWcnLogHandler&) = delete;

  /*  start - override the virtual function for starting ext WCN log
   *
   *  Return Value:
   *    Return 0 if ext WCN log start successfully.
   */
  int start(LogConfig::LogMode mode = LogConfig::LM_NORMAL) override;

  /*
   *    stop - Stop logging.
   *
   *    This function put the LogPipeHandler in an adminitrative
   *    disable state and close the log device.
   *
   *    Return Value:
   *      0
   */
  int stop() override;

  /*
   *    process_assert - Process the internal wcn assertion.
   *
   *    This function will be called by the LogController when the
   *    CP asserts.
   */
  void process_assert(const struct tm& lt,
                      CpStateHandler::CpEvent evt) override;

  /*  crash - crash the internal wcn administratively.
   *  Return 0 on success, -1 on error.
   */
  int crash();

 private:
  /*  start_dump - internal WCN dump.
   *  Return Value:
   *    return -1 if the dump can not be saved,
   *    return 0 if the dump is saved.
   */
  int start_dump(const struct tm& lt);

 private:
  const char* m_dump_path;
};

#endif  // !INT_WCN_LOG_H_
