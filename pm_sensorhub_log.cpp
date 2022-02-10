/*
 *  pm_sensorhub_log.cpp - The power management, sensorhub log
 *                         and dump handler class declaration.
 *
 *  Copyright (C) 2015-2018 Spreadtrum Communications Inc.
 *  Copyright (C) 2018,2019 UNISOC Communications Inc.
 *
 *  History:
 *  2016-3-2 YAN Zhihang
 *  Initial version.
 *
 *  2019-4-15 Zhang Ziyi
 *  Update to use the new TransCpDump class.
 */
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "client_mgr.h"
#include "diag_dev_hdl.h"
#include "log_config.h"
#include "log_ctrl.h"
#include "pm_modem_dump.h"
#include "pm_sensorhub_log.h"
#ifdef USE_ORCA_MODEM
#include "trans_orca_dump.h"
#endif

PmSensorhubLogHandler::PmSensorhubLogHandler(LogController* ctrl,
                                             Multiplexer* multi,
                                             const LogConfig::ConfigEntry* conf,
                                             StorageManager& stor_mgr,
                                             CpClass cp_class)
  : LogPipeHandler(ctrl, multi, conf, stor_mgr, cp_class),
    ctrl_file_{-1},
    reopen_timer_{} {}

PmSensorhubLogHandler::~PmSensorhubLogHandler() {
  if (reopen_timer_) {
    TimerManager& tmgr = multiplexer()->timer_mgr();
    tmgr.del_timer(reopen_timer_);
  }

  if (ctrl_file_ >= 0) {
    ::close(ctrl_file_);
  }
}

int PmSensorhubLogHandler::start_dump(const struct tm& lt,
                                      CpStateHandler::CpEvent evt) {
  LogController::save_sipc_dump(storage(), lt);

  int err {Transaction::TRANS_E_ERROR};

#ifdef USE_ORCA_MODEM

  TransOrcaDump* dump;

  dump = new TransOrcaDump{this,
                           this, *storage(),
                           lt,
                           evt, "sh",
                           "\nPM_memdump_finish", 0x31};

#else  //!USE_ORCA_MODEM

  TransPmModemDump* dump;

  dump = new TransPmModemDump{this,
                              this, *storage(),
                              lt, kPmMemDevPath,
                              "sh","\nPM_memdump_finish",
                              0x31};

#endif  //USE_ORCA_MODEM

  dump->set_client(this, dump_result);
  if (number()) {
    enqueue(dump);
    err = Transaction::TRANS_E_STARTED;
  } else {
    err = dump->execute();
    switch (err) {
      case Transaction::TRANS_E_STARTED:
        enqueue(dump);
        break;
      case Transaction::TRANS_E_SUCCESS:
        delete dump;
        break;
      default:
        delete dump;
        break;
    }
  }

  return err;
}

void PmSensorhubLogHandler::dump_result(void* client,
                                        Transaction* trans) {
  PmSensorhubLogHandler* cp = static_cast<PmSensorhubLogHandler*>(client);

  cp->end_dump();

  info_log("%s dump %s", ls2cstring(cp->name()),
           Transaction::TRANS_R_SUCCESS == trans->result() ?
               "succeeded" : "failed");

  delete trans;
}

int PmSensorhubLogHandler::check_ctrl_file() {
  int ret{};

  if (-1 == ctrl_file_) {
    ctrl_file_ = ::open(PM_LOG_CTRL, O_WRONLY);
    if (-1 == ctrl_file_) {
      err_log("open %s fail", PM_LOG_CTRL);
      ret = -1;
    }
  }

  return ret;
}

int PmSensorhubLogHandler::log_switch(bool on/* = true*/) {
  ssize_t nw;
  int ret = 0;
  if (on) { // open log
    nw = ::write(ctrl_file_, "log on", 6);
    if (6 != nw) {
      err_log("open pm log fail");
      ret = -1;
    }
  } else { // close log
    nw = ::write(ctrl_file_, "log off", 7);
    if (7 != nw) {
      err_log("close pm log fail");
      ret = -1;
    }
  }

  return ret;
}

int PmSensorhubLogHandler::start(LogConfig::LogMode /*mode*/) {
  if (!check_ctrl_file()) {
    if (log_switch()) {
      // Failing to turn on log is not fatal.
      info_log("failed to turn on PM/Sensor Hub log");
    }
  } else {
    // The PM/Sensor Hub control file can not be opened now.
    // Try opening it later.
    TimerManager& tmgr = multiplexer()->timer_mgr();
    reopen_timer_ = tmgr.create_timer(500, enable_output_timer, this);
  }
  return start_logging();
}

int PmSensorhubLogHandler::stop() {
  if (reopen_timer_) {
    TimerManager& tmgr = multiplexer()->timer_mgr();
    tmgr.del_timer(reopen_timer_);
    reopen_timer_ = nullptr;
  } else {
    if (!check_ctrl_file()) {
      if (log_switch(false)) {
        // Failing to turn off log is not fatal.
        info_log("failed to turn off PM/Sensor Hub log");
      }
    }
  }
  return stop_logging();
}

void PmSensorhubLogHandler::enable_output_timer(void* client) {
  PmSensorhubLogHandler* pmsh = static_cast<PmSensorhubLogHandler*>(client);

  pmsh->reopen_timer_ = nullptr;

  if (!pmsh->check_ctrl_file()) {
    if (pmsh->log_switch()) {
      // Failing to turn on log is not fatal.
      err_log("failed to turn on PM/Sensor Hub log");
    } else {
      info_log("PM/Sensor Hub log output on");
    }
  } else {
    // The PM/Sensor Hub control file can not be opened now.
    // Try opening it later.
    TimerManager& tmgr = pmsh->multiplexer()->timer_mgr();
    pmsh->reopen_timer_ = tmgr.create_timer(500, enable_output_timer, pmsh);
  }
}

int PmSensorhubLogHandler::crash() {
  int ret{-1};

  if (!check_ctrl_file()) {
    ssize_t nw = ::write(ctrl_file_, "assert on", 9);
    if (9 == nw) {
      ret = 0;
    }
  }

  return ret;
}

void PmSensorhubLogHandler::process_assert(const struct tm& lt,
                                           CpStateHandler::CpEvent evt) {
  if (reopen_timer_) {
    TimerManager& tmgr = multiplexer()->timer_mgr();
    tmgr.del_timer(reopen_timer_);
    reopen_timer_ = nullptr;
  }

  if (CWS_DUMP == cp_state()) {  // Dumping now, ignore.
    warn_log("assertion got when dumping");
    return;
  }

  if (CWS_NOT_WORKING == cp_state()) {
    info_log("assertion got when PM/Sensor Hub nonfunctional");
    notify_dump_end();
    return;
  }

  set_cp_state(CWS_DUMP);

  bool will_reset_cp = will_be_reset();

  info_log("PM/Sensor Hub %s, cp state is %d, cp log is %s.",
           will_reset_cp ? "will be reset" : "is asserted",
           cp_state(),
           enabled() ? "enabled" : "disabled");

  if (will_reset_cp) {
    set_cp_state(CWS_NOT_WORKING);
    notify_dump_end();
    return;
  }

  // Save wcn dump if log is turned on
  if (enabled()) {
    notify_dump_start();

    int result;
    int err = start_dump(lt, evt);

    switch (err) {
      case Transaction::TRANS_E_SUCCESS:  // Finished
        close_on_assert();
        notify_dump_end();
        break;
      case Transaction::TRANS_E_STARTED:  // Executing
        if (log_diag_dev_same()) {
          del_events(POLLIN);
        }
        break;
      default:  // Failure
        err_log("Start dump transaction error");
        close_on_assert();
        notify_dump_end();
        break;
    }
  } else {
    close_on_assert();
    notify_dump_end();
  }
}

void PmSensorhubLogHandler::trans_finished(Transaction* trans) {
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

