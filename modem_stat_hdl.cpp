/*
 *  modem_stat_hdl.cpp - The MODEM status handler implementation.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-2-16 Zhang Ziyi
 *  Initial version.
 */

#include <sys/types.h>
#include <sys/socket.h>
#ifdef HOST_TEST_
#include "sock_test.h"
#else
#include "cutils/sockets.h"
#endif
#include <unistd.h>

#include "modem_stat_hdl.h"
#include "multiplexer.h"
#include "log_ctrl.h"
#include "parse_utils.h"
#include "wan_modem_log.h"

ModemStateHandler::ModemStateHandler(LogController* ctrl,
                                     Multiplexer* multiplexer,
                                     const char* serv_name,
                                     WanModemLogHandler* modem)
    : CpStateHandler(ctrl, multiplexer, serv_name),
      modem_{modem} {}

int ModemStateHandler::init() {
  CpStateHandler::init();
  modem_->register_dump_start(this, dump_start);
  modem_->register_dump_ongoing(this, dump_ongoing);
  modem_->register_dump_end(this, dump_end);
  return 0;
}

ModemStateHandler::~ModemStateHandler() {
  modem_->unregister_dump_start(this);
  modem_->unregister_dump_ongoing(this);
  modem_->unregister_dump_end(this);
}

CpStateHandler::CpEvent ModemStateHandler::parse_notify(const uint8_t* buf,
                                                        size_t len,
                                                        CpType& type) {
  const uint8_t* p =
      find_str(buf, len,
               reinterpret_cast<const uint8_t*>("DP-ARM Modem Assert"), 19);

  type = CT_WANMODEM;
  if (p) {
    return CE_ASSERT_DP;
  }

  p = find_str(buf, len,
               reinterpret_cast<const uint8_t*>("P-ARM Modem Assert"), 18);
  if (p) {
    type = CT_PM_SH;
    return CE_ASSERT;
  }

  p = find_str(buf, len,
               reinterpret_cast<const uint8_t*>("TD Modem Assert"), 15);
  if (p) {
    return CE_ASSERT;
  }

  p = find_str(buf, len, reinterpret_cast<const uint8_t*>("Modem Reset"), 11);
  if (p) {
    return CE_RESET;
  }

  p = find_str(buf, len, reinterpret_cast<const uint8_t*>("Modem Assert"), 12);
  if (p) {
    CpStateHandler::CpEvent evt;

    p = find_str(buf, len, reinterpret_cast<const uint8_t*>("Panic"), 5);
    if (p) {
      evt = CE_PANIC;
    } else {
      evt = CE_ASSERT;
    }

    return evt;
  }

  p = find_str(buf, len, reinterpret_cast<const uint8_t*>("Modem Alive"), 11);
  if (p) {
    return CE_ALIVE;
  }

  p = find_str(buf, len, reinterpret_cast<const uint8_t*>("Modem Blocked"), 13);
  if (p) {
    return CE_BLOCKED;
  }

  return CE_NONE;
}

void ModemStateHandler::dump_start(void* client) {
  ModemStateHandler* mstat = static_cast<ModemStateHandler*>(client);
  int fd = mstat->fd();
  if (fd >= 0) {
    write(fd, "SLOGMODEM DUMP BEGIN", 20);
  }
}

void ModemStateHandler::dump_ongoing(void* client) {
  ModemStateHandler* mstat = static_cast<ModemStateHandler*>(client);
  int fd = mstat->fd();
  if (fd >= 0) {
    write(fd, "SLOGMODEM DUMP ONGOING", 22);
  }
}

void ModemStateHandler::dump_end(void* client) {
  ModemStateHandler* mstat = static_cast<ModemStateHandler*>(client);
  int fd = mstat->fd();
  if (fd >= 0) {
    write(fd, "SLOGMODEM DUMP COMPLETE", 23);
  }
}
