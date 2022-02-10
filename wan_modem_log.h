/*
 *  wan_modem_log.h - The WAN MODEM log and dump handler class declaration.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-7-13 Zhang Ziyi
 *  Initial version.
 */
#ifndef WAN_MODEM_LOG_H_
#define WAN_MODEM_LOG_H_

#include "log_pipe_hdl.h"
#include "modem_at_ctrl.h"
#include "modem_cmd_ctrl.h"
#include "stor_mgr.h"
#include "wcdma_iq_mgr.h"
#include "def_config.h"

class CpDirectory;
class LogFile;
class PowerOnLogUnifier;
class Transaction;
class TransModemLastLog;
class TransModemTimeSync;
class TransSaveSleepLog;
class TransStartEventLog;
class TransStartNormalLog;
class TransStopCellularLog;

#ifdef MODEM_TALIGN_AT_CMD_
class WanModemTimeSyncAtCmd;
#else
class WanModemTimeSyncRefnotify;
#endif

class WanModemLogHandler : public LogPipeHandler {
 public:
  WanModemLogHandler(LogController* ctrl, Multiplexer* multi,
                     const LogConfig::ConfigEntry* conf,
                     StorageManager& stor_mgr, const char* dump_path,
                     CpClass cp_class);
  WanModemLogHandler(const WanModemLogHandler&) = delete;
  ~WanModemLogHandler();

  WanModemLogHandler& operator = (const WanModemLogHandler&) = delete;

  ModemAtController& at_controller() { return at_ctrl_; }
  ModemCmdController& cmd_controller() { return cmd_ctrl_; }

  // LogPipeHandler::start()
  int start(LogConfig::LogMode mode = LogConfig::LM_NORMAL) override;
  // LogPipeHandler::stop()
  int stop() override;

  // LogPipeHandler::flush()
  void flush() override;

  // LogPipeHandler::process_alive()
  void process_alive() override;
  // LogPipeHandler::process_assert()
  void process_assert(const struct tm& lt,
                      CpStateHandler::CpEvent evt) override;

  // LogPipeHandler::pause()
  int pause() override;
  // LogPipeHandler::resume()
  int resume() override;

  /*  enable_wcdma_iq - enable WCDMA I/Q saving.
   *
   *  Return LCR_SUCCESS on success, LCR_xxx on error.
   */
  int enable_wcdma_iq();
  /*  disable_wcdma_iq - disable WCDMA I/Q saving.
   *
   *  Return LCR_SUCCESS on success, LCR_xxx on error.
   */
  int disable_wcdma_iq();
  /*  set_md_save - enable minidump save or nor according to parameter
   *  @enable - true if enable minidump
   */
  void set_md_save(bool enable) { m_save_md = enable; }
  /*  set_md_internal - set minidump storage position
   *  @int_pos - true if save position is internal storage
   */
  void set_md_internal(bool int_pos) { m_md_to_int = int_pos; }

  void set_poweron_period(int period) { m_poweron_period = period; }

  /*  set_agdsp_log_dest - set the destination of AG DSP LOG in MODEM. */
  int set_agdsp_log_dest(LogConfig::AgDspLogDestination dest);

  /*  set_agdsp_pcm_dump - set the pcm dump or not */
  int set_agdsp_pcm_dump(bool pcm);

  /*  set_miniap_log_status - set the miniAp log on or off */
  int set_miniap_log_status(bool status);

  /*  set_orca_log - set the orca log on or off */
  int set_orca_log(LogString cmd, bool status);

  /*  set_mipi_log_info - set the mipi log information
   *  @MipiLogList: mipi log info list
   */
  int set_mipi_log_info(LogConfig::MipiLogList mipilog_list);

  /*  crash - crash the MODEM administratively.
   *
   *  Return 0 on success, -1 on error.
   */
  int crash();

  /*  save_sleep_log - Start a sleep log saving transaction.
   *  @client: the client pointer.
   *  @cb: the result callback function.
   *  @trans: the transaction object pointer.
   *
   *  Return Values:
   *    Transaction::TRANS_E_STARTED: transaction started and not finished.
   *    Transaction::TRANS_E_SUCCESS: transaction finished successfully.
   *    other:                        error
   */
  int save_sleep_log(void* client,
                     Transaction::ResultCallback cb,
                     TransSaveSleepLog*& trans);

  /*  start_event_log - Start event triggered log.
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
  int start_event_log(void* client,
                      Transaction::ResultCallback cb,
                      TransStartEventLog*& trans);

  /*  start_normal_log - Start normal log.
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
  int start_normal_log(void* client,
                       Transaction::ResultCallback cb,
                       TransStartNormalLog*& trans);

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
               TransStopCellularLog*& trans);

  /*
   *  save_last_log - Start last log saving transaction.
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
  int save_last_log(void* client,
                    Transaction::ResultCallback cb,
                    TransModemLastLog*& trans);

  /*
   *  sync_modem_time - Start sync modem time transaction.
   *  @client: the client pointer.
   *  @cb: the result callback function.
   *  @trans: the transaction object pointer.
   *
   *  Return Values:
   *    Transaction::TRANS_E_STARTED: transaction started and not finished.
   *    Transaction::TRANS_E_ERROR:   transaction start fail.
   *    other:                        error
   */
  int sync_modem_time(void* client,
                      Transaction::ResultCallback cb,
                      TransModemTimeSync*& trans);

  /*  change_log_mode - change the log mode.
   *  @mode: the new log mode.
   *
   */
  void change_log_mode(LogConfig::LogMode mode);

  // TransactionManager::trans_finished()
  void trans_finished(Transaction* trans) override;

  //Debug
  static void modem_event_notify(void* client, int sim, int event);

  bool is_miniap_log_open() {
    bool miniap_open = false;
    char mini_ap_log[PROPERTY_VALUE_MAX] = {0};
    property_get(MINI_AP_KERNEL_LOG_PROP,mini_ap_log,"");
    if (!strcmp(mini_ap_log, "1")) {
      miniap_open = true;
    }
    return miniap_open;
  }
  /*
   *    requery_modem_ver - Try to query modem version again.
   *
   *    This function is called when query modem version failed.
   */
  static void requery_modem_ver(void* param);

  /* calculate_modem_timestamp - calculate wan modem timestamp with the
   *                             system count and uptime received from refnotify.
   * @ts: time sync info consist of cp boot system count and uptime
   * @modem_timestamp: value result
   */
  static void calculate_modem_timestamp(const time_sync& ts,
                                        modem_timestamp& mp);

  static void notify_modem_time_update(void* client, const time_sync& ts);
  void correct_ver_info();

  enum ModemVerState {
    MVS_NOT_BEGIN,
    MVS_QUERY,
    MVS_GOT,
    MVS_FAILED
  };

  enum ModemVerStoreState {
    MVSS_NOT_SAVED,
    MVSS_SAVED
  };
  LogString get_md_ver() {
    return m_modem_ver;
  }
  ModemVerState get_md_ver_state() {
    return m_ver_state;
  }
  bool get_time_state() {
    return m_timestamp_miss;
  }
#ifdef MODEM_TALIGN_AT_CMD_
  WanModemTimeSyncAtCmd* get_time_sync_mgr(void) {
    return time_sync_mgr_;
  }
#else
  WanModemTimeSyncRefnotify* get_time_sync_mgr(void) {
    return time_sync_mgr_;
  }
#endif
  bool save_timestamp(LogFile* f);
  /* frame_version_info - frame the version info
   * @payload - payload of version info
   * @pl_len - payload length
   * @len_reserved - spaces reserved for the resulted frame if not equal 0
   * @frame_len - length of resulted frame
   *
   * Return pointer to the framed info.
   */
  uint8_t* frame_version_info(const uint8_t* payload, size_t pl_len,
                              size_t len_reserved, size_t& frame_len);
  /* frame_noversion_and_reserve - frame the unavailable information
   *                               and reserve spaces for the the modem
   *                               version to come.
   * @length - reserved length to be written to log file
   *
   * Return pointer to the framed info.
   */
  uint8_t* frame_noversion_and_reserve(size_t& length);
 private:
  // number of bytes predicated for modem version when not available
  static const size_t PRE_MODEM_VERSION_LEN = 200;

  void get_tty_path();
  void init_cmd_handler();

  // Override base class function
  int create_storage() override;

  /*  get_cur_modem_cnt - get current MODEM counter value.
   *  cnt: if the AP/MODEM time alignment information is available, return
   *       the current MODEM counter value in cnt.
   *
   *  Return 0 if the AP/MODEM time alignment information is available;
   *  return -1 otherwise.
   */
  int get_cur_modem_cnt(uint32_t& cnt) const;


  /*  save_minidump - Save the mini dump.
   *
   *  @t: the time to be used as the file name
   */
  void save_minidump(const struct tm& t);
  /*  save_sipc_and_minidump - save sipc dump memory and
   *                           minidump if minidump enabled
   *  @lt: system time for file name generation
   */
  void save_sipc_and_minidump(const struct tm& lt);

  void on_new_log_file(LogFile* f) override;
  /*  start_dump - WAN MODEM memory dump.
   *  Return Value:
   *    Return 0 if the dump transaction is started successfully,
   *    return -1 if the dump transaction can not be started,
   */
  int start_dump(const struct tm& lt, CpStateHandler::CpEvent evt);

  /*  start_dump_etb - WAN MODEM etb memory dump.
   *  Return Value:
   *    Return 0 if the etb_dump transaction is started successfully,
   *    return -1 if the etb_dump transaction can not be started,
   */
  int start_dump_etb(const struct tm& lt,CpStateHandler::CpEvent evt);

  int start_ver_query();

  void cancel_all_trans(int result);

  static void fill_smp_header(uint8_t* buf, size_t len,
                              unsigned lcn, unsigned type);

  static void fill_diag_header(uint8_t* buf, uint32_t sn, unsigned len,
                               unsigned cmd, unsigned subcmd);
  static void power_on_stor_finish(void* param);

  static void dump_etb_result(void* client, Transaction* trans);

  static void dump_result(void* client, Transaction* trans);
  static void ongoing_report(void* client);
  static void modem_ver_result(void* client, Transaction* trans);

  static void event_log_result(void* client, Transaction* trans);
  static void init_evt_log_result(void* client, Transaction* trans);

  static void media_change_notify(void* client);
  static void parse_lib_result(void* client, Transaction* trans);

 private:
  const char* m_dump_path;
  // WCDMA I/Q manager
  WcdmaIqManager m_wcdma_iq_mgr;
  // Save WCDMA I/Q
  bool m_save_wcdma_iq;
  // Save minidump or not
  bool m_save_md;
  // Save minidump to internal storage
  bool m_md_to_int;
  // Initialization state
  bool init_evt_log_;
  TransStartEventLog* start_evt_trans_;
#ifdef MODEM_TALIGN_AT_CMD_
  WanModemTimeSyncAtCmd* time_sync_mgr_;
#else
  WanModemTimeSyncRefnotify* time_sync_mgr_;
#endif
  bool m_timestamp_miss;
  ModemVerState m_ver_state;
  ModemVerStoreState m_ver_stor_state;
  LogString m_modem_ver;
  size_t m_reserved_version_len;
  int m_poweron_period;
  // MODEM AT controller
  ModemAtController at_ctrl_;
  // MODEM command controller
  ModemCmdController cmd_ctrl_;
  // Exception event list file
  std::weak_ptr<LogFile> event_list_file_;
  CpStateHandler::CpEvent m_evt;

  bool m_mini_ap_log;
  bool m_orcadp_log;
};

#endif  // !WAN_MODEM_LOG_H_
