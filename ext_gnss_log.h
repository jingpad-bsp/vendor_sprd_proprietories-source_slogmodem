/*
 *  ext_gnss_log.h - The external GNSS log and dump handler class declaration.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-8-26 George YAN
 *  Initial version.
 */
#ifndef EXT_GNSS_LOG_H_
#define EXT_GNSS_LOG_H_

#include "log_pipe_hdl.h"

class Transaction;

class ExtGnssLogHandler : public LogPipeHandler {
 public:
  ExtGnssLogHandler(LogController* ctrl, Multiplexer* multi,
                    const LogConfig::ConfigEntry* conf,
                    StorageManager& stor_mgr, CpClass cp_class);

  /*  start - override the virtual function for starting gnss log
   *
   *  Return Value:
   *    Return 0 if gnss log start successfully,
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

  void trans_finished(Transaction* trans) override;

 private:
  /*
   *    process_assert - Process the gnss assertion.
   *
   *    This function will be called by the LogController when the
   *    CP asserts.
   */
  void process_assert(const struct tm& lt, CpStateHandler::CpEvent evt) {}

  /*  start_dump - start external GNSS dump.
   *  Return Value:
   *    Return 0 if the dump transaction is started successfully,
   *    return -1 if the dump transaction can not be started,
   *    return 1 if the dump transaction is finished.
   */
  int start_dump(const struct tm& lt);
};

#endif  // !EXT_GNSS_LOG_H_
