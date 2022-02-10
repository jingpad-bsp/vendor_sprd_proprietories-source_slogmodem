/*
 *  orca_miniap_log.h - The ORCA miniap log handler class declaration.
 *
 *  Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 *  History:
 *  2020-3-18 Chunlan Wang
 *  Initial version.
 */
#ifndef ORCA_MINIAP_LOG_H_
#define ORCA_MINIAP_LOG_H_

#include "log_pipe_hdl.h"
#include "wan_modem_log.h"
#include "stor_mgr.h"
#include "def_config.h"

class CpDirectory;
class LogFile;

class OrcaMiniapLogHandler : public WanModemLogHandler {
 public:
  OrcaMiniapLogHandler(LogController* ctrl, Multiplexer* multi,
                       const LogConfig::ConfigEntry* conf,
                       StorageManager& stor_mgr, CpClass cp_class, WanModemLogHandler* wm);
  OrcaMiniapLogHandler(const OrcaMiniapLogHandler&) = delete;
  ~OrcaMiniapLogHandler();

  OrcaMiniapLogHandler& operator = (const OrcaMiniapLogHandler&) = delete;

  void on_new_log_file(LogFile* f) override;
  // LogPipeHandler::start()
  int start(LogConfig::LogMode mode = LogConfig::LM_NORMAL) override;
  // LogPipeHandler::stop()
  int stop() override;
  static void check_version_update(void* param);
//  static void check_time_update(void* param);
 private:
  // number of bytes predicated for modem version when not available
  static const size_t PRE_MODEM_VERSION_LEN = 200;
  LogString m_modem_ver;

  WanModemLogHandler::ModemVerState m_ver_state;
  WanModemLogHandler::ModemVerStoreState m_ver_stor_state;
  WanModemLogHandler* wan_handler_;

  size_t m_reserved_version_len;

};

#endif  // !ORCA_MINIAP_LOG_H_
