/*
 *  log_ctrl.h - The log controller class declaration.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-2-16 Zhang Ziyi
 *  Initial version.
 */
#ifndef LOG_CTRL_H_
#define LOG_CTRL_H_

#include "cp_log_cmn.h"
#include "multiplexer.h"
#include "client_mgr.h"
#include "log_pipe_hdl.h"
#include "modem_stat_hdl.h"
#include "wcnd_stat_hdl.h"
#include "log_config.h"
#include "log_file.h"
#include "stor_mgr.h"
#include "trans.h"
#include "trans_mgr.h"
#include "convey_workshop.h"


class TransDisableLog;
class TransEnableLog;
class TransLogCollect;
class TransStartEventLog;
class TransWcnLastLog;

class LogController : public TransactionManager {
 public:
  LogController();
  LogController(const LogController&) = delete;
  ~LogController();

  LogController& operator = (const LogController&) = delete;

  int init(LogConfig* config);
  int run();

  LogConfig* config() const { return m_config; }

  ClientManager* cli_mgr() { return m_cli_mgr; }
  ConveyWorkshop* convey_workshop() { return convey_workshop_; }
  StorageManager* stor_mgr() { return &m_stor_mgr; }
  LogPipeHandler* get_cp(CpType type);
  LogList<LogPipeHandler*> get_cps() { return m_log_pipes; }
  /*  get_generic_cp - get the LogPipeHandler object of the CP type.
   *  @type: the CP type. type can be CT_WANMODEM.
   */
  LogPipeHandler* get_generic_cp(CpType type);

  void process_cp_evt(CpType type, CpStateHandler::CpEvent evt,
                      LogString assert_info, size_t assert_info_len);

  // Client requests
  /*  enable_log - start log enable transaction.
   *  @cps: the CPs whose log are to be collected. If cps is empty, collect
   *        log for all CPs in the system.
   *  @num: number of elements in cps array.
   *  @cb: transaction result callback function pointer
   *  @client: client pointer
   *  @trans: returns the transaction pointer if it is started successfully.
   *
   *  This function creates a TransEnableLog object and try to start the
   *  transaction.
   *
   *  Return Value:
   *    Transaction::TRANS_E_STARTED: transaction started and not finished.
   *    Transaction::TRANS_E_SUCCESS: transaction finished successfully.
   *    other:                        error
   */
  int enable_log(const CpType* cps, size_t num,
                 Transaction::ResultCallback cb,
                 void* client,
                 TransEnableLog*& trans);
  /*  disable_log - start log disable transaction.
   *  @cps: the CPs whose log are to be collected. If cps is empty, collect
   *        log for all CPs in the system.
   *  @num: number of elements in cps array.
   *  @cb: transaction result callback function pointer
   *  @client: client pointer
   *  @trans: returns the transaction pointer if it is started successfully.
   *
   *  This function creates a TransDisableLog object and try to start the
   *  transaction.
   *
   *  Return Value:
   *    Transaction::TRANS_E_STARTED: transaction started and not finished.
   *    Transaction::TRANS_E_SUCCESS: transaction finished successfully.
   *    other:                        error
   */
  int disable_log(const CpType* cps, size_t num,
                  Transaction::ResultCallback cb,
                  void* client,
                  TransDisableLog*& trans);
  LogConfig::LogMode get_log_state(CpType ct) const;
  int enable_md();
  int disable_md();
  int mini_dump();
  size_t get_log_file_size(CpType ct, size_t& sz) const;
  int set_log_file_size(CpType ct, size_t len);
  int get_log_overwrite(CpType ct, bool& en) const;
  int set_log_overwrite(CpType ct, bool en = true);
  bool get_md_int_stor() const;
  int set_md_int_stor(bool md_int_stor);
  int clear_log();
  int get_cp_log_size(CpType ct, StorageManager::MediaType mt, size_t& sz) const;
  int set_cp_log_size(CpType ct, StorageManager::MediaType mt, size_t sz);
  bool use_external_storage();
  int active_external_storage(bool active);
  /*  enable_wcdma_iq - enable WCDMA I/Q saving.
   *  @cpt: CP type
   *
   *  Return LCR_SUCCESS on success, LCR_xxx on error.
   *  Possible errors:
   *    LCR_CP_NONEXISTENT  CP not exist
   */
  int enable_wcdma_iq(CpType cpt);
  /*  disable_wcdma_iq - disable WCDMA I/Q saving.
   *  @cpt: CP type
   *
   *  Return LCR_SUCCESS on success, LCR_xxx on error.
   *  Possible errors:
   *    LCR_CP_NONEXISTENT  CP not exist
   */
  int disable_wcdma_iq(CpType cpt);
  /*  copy_cp_files - copy cp (log/memory dump) files into modem_log.
   *  @t - cp type
   *  @file_type - log/dump type of files to be copied.
   *  @src_files - collection of files to be copied.
   *
   *  Return LCR_SUCCESS on success, LCR_ERROR on error.
   *  Possible errors:
   *    LCR_CP_NONEXISTENT  CP not exist
   */
  int copy_cp_files(CpType t, LogFile::LogType file_type,
                    const LogVector<LogString>& src_files);

#ifdef SUPPORT_AGDSP
  LogConfig::AgDspLogDestination get_agdsp_log_dest() const;
  int set_agdsp_log_dest(LogConfig::AgDspLogDestination dest);
  bool get_agdsp_pcm_dump() const;
  int set_agdsp_pcm_dump(bool pcm);
#endif

  int set_miniap_log_status(bool status);
  int set_orca_log(LogString cmd, bool status);
  int set_mipi_log_info(LogString type, LogString channel, LogString freq);
  int get_mipi_log_info(LogString type, LogString& channel, LogString& freq);

  /*  flush - flush buffered log into file.
   *
   *  Return LCR_SUCCESS.
   */
  int flush();

  /*  start_log_collect - collect log for specified CPs.
   *  @cps: the CPs whose log are to be collected. If cps is empty, collect
   *        log for all CPs in the system.
   *  @cb: transaction result callback function pointer
   *  @client: client pointer
   *  @trans: returns the transaction pointer if it is started successfully.
   *
   *  This function creates a TransLogCollect object and try to start the
   *  transaction.
   *
   *  Return Value:
   *    Transaction::TRANS_E_STARTED: transaction started and not finished.
   *    Transaction::TRANS_E_SUCCESS: transaction finished successfully.
   *    other:                        error
   */
  int start_log_collect(const ModemSet& cps,
                        Transaction::ResultCallback cb,
                        void* client,
                        TransLogCollect*& trans);

  /*  start_event_log - start event triggered log.
   *  @ct: the CP type
   *  @cb: transaction result callback function pointer
   *  @client: client pointer
   *  @trans: returns the transaction pointer if it is started successfully.
   *
   *  This function creates a TransStartEventLog object and try to start the
   *  transaction.
   *
   *  Return Value:
   *    Transaction::TRANS_E_STARTED: transaction started and not finished.
   *    Transaction::TRANS_E_SUCCESS: transaction finished successfully.
   *    other:                        error
   */
  int start_event_log(CpType ct, Transaction::ResultCallback cb,
                      void* client, TransStartEventLog*& trans);

  /*  save_wcn_last_log - start WCN last log save transaction.
   *  @cb: transaction result callback function pointer
   *  @client: client pointer
   *  @trans: returns the transaction pointer if it is started successfully.
   *
   *  This function creates a TransWcnLastLog object and try to start the
   *  transaction.
   *
   *  Return Value:
   *    Transaction::TRANS_E_STARTED: transaction started and not finished.
   *    Transaction::TRANS_E_SUCCESS: transaction finished successfully.
   *    other:                        error
   */
  int save_wcn_last_log(Transaction::ResultCallback cb, void* client,
                        TransWcnLastLog*& trans);

  /*
   *    save_sipc_dump - Save the SIPC dump.
   *    @stor: the CP storage handle
   *    @t: the time to be used as the file name
   *
   *    Return Value:
   *      Return 0 on success, -1 otherwise.
   */
  static int save_sipc_dump(CpStorage* stor, const struct tm& t);

  /*
   *    save_etb - save ETB.
   *    @stor: the CP storage handle
   *    @lt: the time to be used as the file name
   *
   *    Return Value:
   *      Return 0 on success, -1 otherwise.
   */
  static int save_etb(CpStorage* stor, const struct tm& lt);

  int get_ratefile() const { return f_rate_statistic_; }

private:
  /*
   * create_handler - Create the LogPipeHandler object and try to
   *                  open the device if it's enabled in the config.
   */
  LogPipeHandler* create_handler(const LogConfig::ConfigEntry* e);

  /*
   * init_state_handler - Create the ModemStateHandler object for
   *                      connections to modemd and wcnd.
   */
  ModemStateHandler* init_state_handler(const char* serv_name);
  /*
   * init_wcn_state_handler - Create the ExtWcnStateHandler
   *                          or the IntWcnStateHandler.
   * @serv_name - wcnd socket
   * @assert_msg - assert notification
   *
   * Return Value:
   *   initialized wcn state handler on success or
   *   nullptr if fail to create wcn state handler
   */
  WcndStateHandler* init_wcn_state_handler(const char* serv_name,
                                           const char* assert_msg);

  static LogList<LogPipeHandler*>::iterator find_log_handler(
      LogList<LogPipeHandler*>& handlers, CpType t);
  static LogList<LogPipeHandler*>::const_iterator find_log_handler(
      const LogList<LogPipeHandler*>& handlers, CpType t);
  static LogList<LogPipeHandler*>::iterator find_log_handler(
      LogList<LogPipeHandler*>& handlers, CpClass cls);
  static LogList<LogPipeHandler*>::const_iterator find_log_handler(
      const LogList<LogPipeHandler*>& handlers, CpClass cls);
  
  int open_ratefile();
  int close();

 private:
  typedef LogList<LogPipeHandler*>::iterator LogPipeIter;

  LogConfig* m_config;
  Multiplexer m_multiplexer;
  ClientManager* m_cli_mgr;
  LogList<LogPipeHandler*> m_log_pipes;
  ModemStateHandler* m_modem_state;
  WcndStateHandler* m_wcn_state;
  int f_rate_statistic_;
  ConveyWorkshop* convey_workshop_;
  // Storage manager
  StorageManager m_stor_mgr;
};

#endif  // !LOG_CTRL_H_
