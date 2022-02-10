/*
 *  ext_wcn_log.h - The external WCN log and dump handler class declaration.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-7-13 Zhang Ziyi
 *  Initial version.
 */
#ifndef EXT_WCN_LOG_H_
#define EXT_WCN_LOG_H_

#include "log_pipe_hdl.h"

class Transaction;
class TransStopWcnLog;

class ExtWcnLogHandler : public LogPipeHandler {
 public:
  ExtWcnLogHandler(LogController* ctrl, Multiplexer* multi,
                   const LogConfig::ConfigEntry* conf,
                   StorageManager& stor_mgr, CpClass cp_class);
  ExtWcnLogHandler(const ExtWcnLogHandler&) = delete;

  ExtWcnLogHandler& operator = (const ExtWcnLogHandler&) = delete;

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
  /*  stop_log - Stop log.
   *  @client: the client pointer.
   *  @cb: the result callback function.
   *  @trans: the transaction object pointer.
   *
   *  Return Values:
   *    Transaction::TRANS_E_STARTED: transaction started and not finished.
   *    Transaction::TRANS_E_SUCCESS: transaction finished successfully.
   *                                  trans contains the transaction pointer.
   *    other:                        error
   */
   int stop_log(void* client,
                Transaction::ResultCallback cb,
                TransStopWcnLog*& trans);
  /*
   *    process_assert - Process the external wcn assertion.
   *
   *    This function will be called by the LogController when the
   *    CP asserts.
   */
  void process_assert(const struct tm& lt,
                      CpStateHandler::CpEvent evt) override;

  /*  crash - crash the external wcn administratively.
   *  Return 0 on success, -1 on error.
   */
  int crash();
  void trans_finished(Transaction* trans) override;

 private:
  /*  start_dump - external WCN dump.
   *  Return Value:
   *    Return 0 if the dump transaction is started successfully,
   *    return -1 if the dump transaction can not be started,
   */
  int start_dump(const struct tm& lt);

  static void dump_result(void* client, Transaction* trans);
};

#endif  // !EXT_WCN_LOG_H_
