/*
 *  int_wcn_log.cpp - The internal WCN log and dump handler class
 *                    implementation declaration.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-8-7 Zhang Ziyi
 *  Initial version.
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "client_mgr.h"
#include "cp_dir.h"
#include "int_wcn_log.h"
#include "log_ctrl.h"
#include "log_file.h"

IntWcnLogHandler::IntWcnLogHandler(LogController* ctrl, Multiplexer* multi,
                                   const LogConfig::ConfigEntry* conf,
                                   StorageManager& stor_mgr,
                                   const char* dump_path, CpClass cp_class)
  : LogPipeHandler(ctrl, multi, conf, stor_mgr, cp_class),
      m_dump_path(dump_path) {}

int IntWcnLogHandler::start(LogConfig::LogMode /*mode*/) {
  return start_logging();
}

int IntWcnLogHandler::stop() { return stop_logging(); }

int IntWcnLogHandler::crash() {
  int fd = ::open("/dev/spipe_wcn5", O_WRONLY);
  if (-1 == fd) {
    err_log("open /dev/spipe_wcn5 error");
    return -1;
  }

  ssize_t nw = write(fd, "AT+SPATASSERT=1\r", 16);
  ::close(fd);

  return 16 == nw ? 0 : -1;
}

int IntWcnLogHandler::start_dump(const struct tm& lt) {
  // Save to dump file directly.
  int err;

  LogFile* mem_file = open_dump_mem_file(lt);
  if (mem_file) {
    err = mem_file->copy(m_dump_path);
    mem_file->close();
    if (err) {
      mem_file->dir()->remove(mem_file);
      err_log("save dump error");
    }
  } else {
    err_log("create dump mem file failed");
    err = -1;
  }

  return err;
}

void IntWcnLogHandler::process_assert(const struct tm& lt,
                                      CpStateHandler::CpEvent evt) {
  if (CWS_DUMP == cp_state()) {  // Dumping now, ignore.
    warn_log("assertion got when dumping");
    return;
  }

  if (CWS_NOT_WORKING == cp_state()) {
    info_log("assertion got when internal WCN nonfunctional");
    notify_dump_end();
    return;
  }

  bool will_reset_cp = will_be_reset();

  info_log("internal wcn %s, cp state is %d, cp log is %s.",
           (will_reset_cp ? "will be reset" : "is asserted"),
           cp_state(), (enabled() ? "enabled" : "disabled"));

  if (will_reset_cp) {
    set_cp_state(CWS_NOT_WORKING);
    end_dump();
    return;
  }

  notify_dump_start();

  // Save wcn dump if log is turned on
  if (enabled()) {
    if (start_dump(lt)) {
      err_log("internal wcn dump error");
    }

    // wcnd requires the property to be set
    set_wcn_dump_prop();
  }

  set_cp_state(CWS_NOT_WORKING);
  end_dump();
}
