/*
 *  ext_gnss_log.cpp - The external GNSS log and dump handler class declaration.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-8-26 George YAN
 *  Initial version.
 */

#include <poll.h>

#include "client_mgr.h"
#include "diag_dev_hdl.h"
#include "ext_gnss_log.h"
#include "log_ctrl.h"

ExtGnssLogHandler::ExtGnssLogHandler(LogController* ctrl, Multiplexer* multi,
                                     const LogConfig::ConfigEntry* conf,
                                     StorageManager& stor_mgr, CpClass cp_class)
    : LogPipeHandler(ctrl, multi, conf, stor_mgr, cp_class) {}

int ExtGnssLogHandler::start_dump(const struct tm& lt) { return -1; }

int ExtGnssLogHandler::start(LogConfig::LogMode /*mode*/) {
  return start_logging();
}

int ExtGnssLogHandler::stop() { return stop_logging(); }

void ExtGnssLogHandler::trans_finished(Transaction* trans) {
  TransactionManager::trans_finished(trans);

  // Start the next transaction
  auto it = transactions_.begin();
  while (it != transactions_.end()) {
    Transaction* mt = *it;
    if (Transaction::TS_NOT_BEGIN == mt->state()) {
      int err = mt->execute();
      if (Transaction::TRANS_E_STARTED == err) {  // Started
        break;
      }

      it = transactions_.erase(it);
      if (mt->has_client()) {
        mt->notify_result();
      } else {
        delete mt;
      }
    } else {  // Transaction started or finished.
      ++it;
    }
  }
}

