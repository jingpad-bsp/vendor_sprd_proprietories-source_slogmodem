/*
 *  ext_wcn_log.h - The external WCN log and dump handler class declaration.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-7-13 Zhang Ziyi
 *  Initial version.
 */

#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "client_mgr.h"
#include "diag_dev_hdl.h"
#include "ext_wcn_dump.h"
#include "ext_wcn_log.h"
#include "log_ctrl.h"
#include "trans_stop_wcn_log.h"

ExtWcnLogHandler::ExtWcnLogHandler(LogController* ctrl, Multiplexer* multi,
                                   const LogConfig::ConfigEntry* conf,
                                   StorageManager& stor_mgr, CpClass cp_class)
    : LogPipeHandler(ctrl, multi, conf, stor_mgr, cp_class) {}

int ExtWcnLogHandler::start(LogConfig::LogMode /*mode*/) {
  return start_logging();
}

int ExtWcnLogHandler::stop() { return stop_logging(); }

int ExtWcnLogHandler::stop_log(void* client,
                               Transaction::ResultCallback cb,
                               TransStopWcnLog*& trans) {
  int ret;
  TransStopWcnLog* stop_trans = new TransStopWcnLog{this};

  stop_trans->set_client(client, cb);
  if (!number()) {
    ret = stop_trans->execute();
    switch (ret) {
      case Transaction::TRANS_E_SUCCESS:
        trans = stop_trans;
        break;
      case Transaction::TRANS_E_STARTED:
        enqueue(stop_trans);
        trans = stop_trans;
        break;
      default:
        delete stop_trans;
        break;
    }
  } else {
    enqueue(stop_trans);
    trans = stop_trans;
    ret = Transaction::TRANS_E_STARTED;
  }

  return ret;
}

int ExtWcnLogHandler::crash() {
  int fd_crash = ::open("/proc/mdbg/at_cmd", O_WRONLY);
  if (-1 == fd_crash) {
    err_log("open /proc/mdbg/at_cmd error");
    return -1;
  }

  ssize_t nw = write(fd_crash, "AT+SPATASSERT=1\r", 16);
  ::close(fd_crash);

  return 16 == nw ? 0 : -1;
}

int ExtWcnLogHandler::start_dump(const struct tm& lt) {
  LogController::save_sipc_dump(storage(), lt);

  int err {-1};
  TransExtWcnDump* dump;

  dump = new TransExtWcnDump(this, this, *storage(), lt, "wcn");

  dump->set_client(this, dump_result);

  if (!cur_diag_trans()) {
    err = dump->execute();
    switch (err) {
      case Transaction::TRANS_E_STARTED:
        enqueue(dump);
        err = 0;
        break;
      default:
        delete dump;
        err = -1;
        break;
    }
  } else {
    delete dump;
  }

  return err;
}

void ExtWcnLogHandler::dump_result(void* client,
                                   Transaction* trans) {
  ExtWcnLogHandler* cp = static_cast<ExtWcnLogHandler*>(client);

  cp->end_dump();
  set_wcn_dump_prop();

  info_log("%s dump %s", ls2cstring(cp->name()),
           Transaction::TRANS_R_SUCCESS == trans->result() ?
               "succeeded" : "failed");

  delete trans;
}

void ExtWcnLogHandler::process_assert(const struct tm& lt,
                                      CpStateHandler::CpEvent evt) {
  if (CWS_DUMP == cp_state()) {  // Dumping now, ignore.
    warn_log("assertion got when dumping");
    return;
  }

  if (CWS_NOT_WORKING == cp_state()) {
    info_log("assertion got when external WCN nonfunctional");
    notify_dump_end();
    return;
  }

  bool will_reset_cp = will_be_reset();

  info_log("external wcn %s, cp state is %d, cp log is %s.",
           (will_reset_cp ? "will be reset" : "is asserted"),
           cp_state(), (enabled() ? "enabled" : "disabled"));

  if (will_reset_cp) {
    set_cp_state(CWS_NOT_WORKING);
    notify_dump_end();
    return;
  }

  // Save wcn dump if log is turned on
  if (enabled()) {
    notify_dump_start();

    if (start_dump(lt)) {  // Failed to start dump
      // wcnd requires the property to be set
      set_wcn_dump_prop();

      err_log("Start dump transaction error");
      close_on_assert();
      notify_dump_end();
    }
  } else {
    close_on_assert();
    notify_dump_end();
  }
}

void ExtWcnLogHandler::trans_finished(Transaction* trans) {
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
