/*
 *  orca_dp_log.h - The ORCA miniap log handler class declaration.
 *
 *  Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 *  History:
 *  2020-3-18 Chunlan Wang
 *  Initial version.
 */
#ifndef ORCA_DP_LOG_H_
#define ORCA_DP_LOG_H_

#include "log_pipe_hdl.h"
#include "wan_modem_log.h"
#include "stor_mgr.h"
#include "def_config.h"

class CpDirectory;
class LogFile;
#ifdef MODEM_TALIGN_AT_CMD_
class WanModemTimeSyncAtCmd;
#else
class WanModemTimeSyncRefnotify;
#endif
class OrcaDpLogHandler : public WanModemLogHandler {
 public:
  OrcaDpLogHandler(LogController* ctrl, Multiplexer* multi,
                     const LogConfig::ConfigEntry* conf,
                     StorageManager& stor_mgr, CpClass cp_class, WanModemLogHandler* wm);
  OrcaDpLogHandler(const OrcaDpLogHandler&) = delete;
  ~OrcaDpLogHandler();

  OrcaDpLogHandler& operator = (const OrcaDpLogHandler&) = delete;

  void on_new_log_file(LogFile* f) override;
  // LogPipeHandler::start()
  int start(LogConfig::LogMode mode = LogConfig::LM_NORMAL) override;
  // LogPipeHandler::stop()
  int stop() override;
  static void check_version_update(void* param);
  static void check_time_update(void* param);
  bool save_timestamp(LogFile* f);

 private:
  // number of bytes predicated for modem version when not available
  static const size_t PRE_MODEM_VERSION_LEN = 200;
  LogString m_modem_ver;
  bool m_timestamp_miss;
  WanModemLogHandler::ModemVerState m_ver_state;
  WanModemLogHandler::ModemVerStoreState m_ver_stor_state;
  WanModemLogHandler* wan_handler_;
#ifdef MODEM_TALIGN_AT_CMD_
  WanModemTimeSyncAtCmd* time_sync_mgr_;
#else
  WanModemTimeSyncRefnotify* time_sync_mgr_;
#endif
  size_t m_reserved_version_len;

};

#endif  // !ORCA_CM4_LOG_H_
