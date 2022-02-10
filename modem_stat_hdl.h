/*
 *  modem_stat_hdl.h - The MODEM status handler declaration.
 *
 *  Copyright (C) 2015-2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2015-2-16 Zhang Ziyi
 *  Initial version.
 *
 *  2017-5-15 Zhang Ziyi
 *  Dump events changed to new LogPipeHandler register_/unregister_ API.
 */

#ifndef _MODEM_STAT_HDL_H_
#define _MODEM_STAT_HDL_H_

#include "cp_stat_hdl.h"

class WanModemLogHandler;

class ModemStateHandler : public CpStateHandler {
 public:
  ModemStateHandler(LogController* ctrl, Multiplexer* multiplexer,
                    const char* serv_name,
                    WanModemLogHandler* modem);
  ModemStateHandler(const ModemStateHandler&) = delete;
  ~ModemStateHandler();

  ModemStateHandler& operator = (const ModemStateHandler&) = delete;

  int init();

 private:
  CpEvent parse_notify(const uint8_t* buf, size_t len, CpType& type);

  static void dump_start(void*);
  static void dump_ongoing(void*);
  static void dump_end(void*);

 private:
  WanModemLogHandler* modem_;
};

#endif  // !_MODEM_STAT_HDL_H_
