/*
 *  log_pipe_hdl.h - The CP log and dump handler class declaration.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-3-2 Zhang Ziyi
 *  Initial version.
 */
#ifndef LOG_PIPE_HDL_H_
#define LOG_PIPE_HDL_H_

#include <time.h>
#ifdef HOST_TEST_
#include "prop_test.h"
#else
#include <cutils/properties.h>
#endif

#include "cp_convey_mgr.h"
#include "cp_log_cmn.h"
#include "cp_stat_hdl.h"
#include "evt_notifier.h"
#include "fd_hdl.h"
#include "log_config.h"
#include "log_file.h"
#include "req_err.h"
#include "timer_mgr.h"
#include "trans.h"
#include "trans_mgr.h"

class ClientHandler;
class CpStorage;
class DiagDeviceHandler;
class StorageManager;
class TransDiagDevice;

class LogPipeHandler : public FdHandler, public TransactionManager {
 public:
  enum CpWorkState {
    CWS_NOT_WORKING,
    CWS_WORKING,
    CWS_DUMP,
    CWS_SAVE_SLEEP_LOG,
    CWS_SAVE_RINGBUF,
    CWS_QUERY_VER,
    CWS_SAVE_EVENT_LOG
  };

  LogPipeHandler(LogController* ctrl, Multiplexer* multi,
                 const LogConfig::ConfigEntry* conf, StorageManager& stor_mgr,
                 CpClass cp_class);
  LogPipeHandler(const LogPipeHandler&) = delete;
  ~LogPipeHandler();

  LogPipeHandler& operator = (const LogPipeHandler&) = delete;

  CpType type() const { return m_type; }
  const LogString& name() const { return m_modem_name; }

  CpWorkState cp_state() const { return m_cp_state; }

  CpClass cp_class() const { return m_cp_class; }
  void set_cp_class(CpClass cp_class) {
    m_cp_class = cp_class;
  }

  bool enabled() const { return LogConfig::LM_NORMAL == log_mode_; }

  LogConfig::LogMode log_mode() const { return log_mode_; }

  void set_log_capacity(StorageManager::MediaType mt, uint64_t sz) {
    if (StorageManager::MT_INT_STOR == mt) {
      m_internal_max_size = sz;
    } else if (StorageManager::MT_EXT_STOR == mt) {
      m_external_max_size = sz;
    }
  }

  uint64_t log_capacity(StorageManager::MediaType mt) const {
    if (StorageManager::MT_INT_STOR == mt) {
      return m_internal_max_size;
    } else /*if (StorageManager::MT_EXT_STOR == mt)*/ {
      return m_external_max_size;
    }
  }

  void set_max_log_file_size(uint64_t sz) { m_max_log_file_size = sz; }
  uint64_t get_max_log_file_size() const { return m_max_log_file_size; }

  void set_overwrite(bool ow) { overwrite_ = ow; }
  bool get_overwrite() const { return overwrite_; }
  bool log_diag_dev_same() const { return m_log_diag_same; }

  const LogString& log_dev_path() const { return m_log_dev_path; }
  const LogString& diag_dev_path() const { return m_diag_dev_path; }

  /*
   *    copy_files - copy log/dump... file to modem log.
   *    @file_type : file type (log files or dump files)
   *    @src_files: collection of files to be copied.
   *
   *    Return Value:
   *       LCR_SUCCESS if success
   *       LCR_ERROR if fail
   */
  int copy_files(LogFile::LogType file_type,
                 const LogVector<LogString>& src_files);

  void set_log_diag_dev_path(const LogString& log, const LogString& diag) {
    m_log_dev_path = log;
    m_diag_dev_path = diag;
    m_log_diag_same = false;
  }

  void set_log_diag_dev_path(const LogString& path) {
    m_log_dev_path = path;
    m_log_diag_same = true;
    m_diag_dev_path.clear();
  }

  void set_assert_info(const LogString& assert_info, size_t assert_info_len) {
    m_assert_info = assert_info;
    m_assert_info_len = assert_info_len;
  }

  void reset_external_max_size() {
    tmp_external_max_size_ = m_external_max_size;
    m_external_max_size = 50 << 20;
  }

  bool in_transaction() const { return m_cp_state > CWS_WORKING; }

  /*  start - Initialize the log device and start logging.
   *  @mode: the log mode to start.
   *
   *  This function put the LogPipeHandler in an adminitrative
   *  enable state and try to open the log device.
   *
   *  Return Value:
   *    0 on success, -1 on error.
   */
  virtual int start(LogConfig::LogMode mode = LogConfig::LM_NORMAL) = 0;

  /*  stop - Stop logging.
   *
   *  This function put the LogPipeHandler in an adminitrative
   *  disable state and close the log device.
   *
   *  Return Value:
   *    0 on success, -1 on error.
   */
  virtual int stop() = 0;

  /*  pause - pause logging and I/Q before log clearing.
   *
   *  This function assume log_mode_ is LogConfig::LM_NORMAL.
   *
   *  Return Value:
   *    0
   */
  virtual int pause() { return 0; }
  /*  resume - resume logging and I/Q after log clearing.
   *
   *  This function assume log_mode_ is LogConfig::LM_NORMAL.
   *
   *  Return Value:
   *    0
   */
  virtual int resume() { return 0; }

  void process(int events);

  /*
   *    process_assert - Process the CP assertion.
   *
   *    This function will be called by the LogController when the
   *    CP asserts.
   */
  virtual void process_assert(const struct tm& lt,
                              CpStateHandler::CpEvent evt) = 0;

  /*
   *    will_be_reset - check whether the CP will be reset.
   */
  bool will_be_reset() const;
  bool shall_save_dump() const;

  /*  write_reset_prop - write the auto reset property if available.
   *  @enable: true - enable auto reset, false - disable auto reset.
   *
   *  Return 0 on success, -1 on error.
   */
  int write_reset_prop(bool enable = true);

  void process_reset();

  /*  process_alive - process the alive event.
   *
   *  This function reopen the log device file if necessary.
   *
   *  Derived class shall override the function if necessary.
   */
  virtual void process_alive();
  /*  flush - flush buffered log into file.
   */
  virtual void flush();
  /*  open_dump_mem_file - Open .mem file to store CP memory from
   *                       /proc/cpxxx/mem.
   */
  LogFile* open_dump_mem_file(const struct tm& lt);

  CpStateHandler* cp_state_hdl() const { return m_cp_state_hdl; }

  void set_cp_state_hdl(CpStateHandler* hdl) { m_cp_state_hdl = hdl; }

  int register_dump_start(void* client, EventNotifier::CallbackFun cb);
  void unregister_dump_start(void* client);
  /*  register_dump_end - register for the dump end event.
   */
  int register_dump_end(void* client, EventNotifier::CallbackFun cb);
  /*  unregister_dump_end - unregister for the dump end event.
   */
  void unregister_dump_end(void* client);

  /*  start_diag_trans - start a diag transaction.
   *  @trans: the reference to the TransDiagDevice object.
   *  @state: the state that the subsystem enters after the transaction
   *          is started.
   *
   *  Return 0 on success, -1 on error.
   */
  int start_diag_trans(TransDiagDevice& trans, CpWorkState state);

  /*  stop_diag_trans - stop a diag transaction.
   *
   */
  void stop_diag_trans();

  int register_dump_ongoing(void* client, EventNotifier::CallbackFun cb);
  void unregister_dump_ongoing(void* client);

  StorageManager& stor_mgr() { return m_stor_mgr; }
  /*
   * start_log_convey - start convey all modem log to current media storage
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
  int start_log_convey(void* client,
                       Transaction::ResultCallback cb,
                       TransLogConvey*& trans);
  /*
   * cancel_log_convey - cancel the log convey of transaction trans
   *
   * @trans: transaction to be cancelled
   */
  void cancel_log_convey(TransLogConvey* trans);
  /*
   * cancel_convey - cancel all the convey
   */
  void cancel_convey();

  void truncate_log();

protected:
  void set_cp_state(CpWorkState cs) { m_cp_state = cs; }

  int open();
  /*
   *    close_devices - Close the log device, stop sleep log/RingBuf.
   *
   *    This function assumes m_fd is opened.
   */
  void close_devices();

  void set_log_mode(LogConfig::LogMode mode) { log_mode_ = mode; }

  CpStorage* storage() { return m_storage; }
  virtual int create_storage();
  /*  end_dump - clean up dump state.
   */
  void end_dump();

  /*
   *    start_logging - start cp log when enabled.
   *    Return value:
   *      Return 0 if start log with success.
   */
  int start_logging();

  /*
   *    stop_logging - Stop logging.
   *
   *    This function put the LogPipeHandler in an adminitrative
   *    disable state and close the log device.
   *
   *    Return Value:
   *      0
   */
  int stop_logging();

  void close_on_assert();

  void notify_dump_start();
  void notify_dump_ongoing();
  void notify_dump_end();

  virtual void on_new_log_file(LogFile* /*f*/) {}

  TransDiagDevice* cur_diag_trans() const { return cur_diag_trans_; }
  DiagDeviceHandler* get_diag_handler() const { return m_diag_handler; }

  static void new_log_callback(LogPipeHandler* cp, LogFile* f) {
    cp->on_new_log_file(f);
  }
  /*
   *    reopen_log_dev - Try to open the log device and the mini
   *                     dump device.
   *
   *    This function is called by the multiplexer on check event.
   */
  static void reopen_log_dev(void* param);

  static void set_wcn_dump_prop();

 protected:
  size_t m_max_buf;
  size_t m_max_buf_num;
  size_t m_log_commit_threshold;
  // Commit threshold for a single buffer
  size_t m_buf_commit_threshold;
  bool m_rate_statistic_;
  // Log data buffer
  DataBuffer* m_buffer;
  // Convey Manager
  CpConveyManager* convey_mgr_;
  // assert info,save assert info if minidump fail
  LogString m_assert_info;
  size_t m_assert_info_len;

 private:
  static void buf_avail_callback(void* client);

  static void data_rate_stat(void* param);
  static void mean_rate_stat(void* param);

 private:
  // Log mode
  LogConfig::LogMode log_mode_;
  LogString m_modem_name;
  CpType m_type;
  uint64_t m_internal_max_size;
  uint64_t m_external_max_size;
  uint64_t m_max_log_file_size;
  uint64_t tmp_external_max_size_;
  bool overwrite_;
  // Log device is the same as the diag device ?
  bool m_log_diag_same;
  // Log device file path
  LogString m_log_dev_path;
  // Diag device file path
  LogString m_diag_dev_path;
  // Reset property
  const char* m_reset_prop;
  // Dump property
  const char* m_save_dump_prop;
  CpWorkState m_cp_state;
  // CP state connection
  CpStateHandler* m_cp_state_hdl;
  // Current dump/sleep log/RingBuf handler
  DiagDeviceHandler* m_diag_handler;
  // Current diag transaction
  TransDiagDevice* cur_diag_trans_;
  StorageManager& m_stor_mgr;
  CpStorage* m_storage;
  CpClass m_cp_class;

  // Event subscription
  EventNotifier dump_start_subs_;
  EventNotifier dump_ongoing_subs_;
  EventNotifier dump_end_subs_;

  // Data rate statistics
  struct timeval mean_start_;
  unsigned long long mean_data_size_;
  TimerManager::Timer* mean_timer_;
  struct timeval period_start_;
  size_t step_data_size_;
  TimerManager::Timer* step_timer_;
};

#endif  // !LOG_PIPE_HDL_H_
