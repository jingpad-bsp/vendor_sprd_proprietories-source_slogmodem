/*
 *  trans_modem.h - The MODEM category transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-5-26 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_MODEM_H_
#define TRANS_MODEM_H_

#include "trans.h"

class TransModem : public Transaction {
 public:
  enum Type {
    START_NORMAL_LOG,
    START_EVENT_LOG,
    STOP_LOG,
    MEMORY_DUMP,
    EXT_WCN_MEMORY_DUMP,
    SAVE_SLEEP_LOG,
    SAVE_RING_BUF,
    VERSION_QUERY,
    MODEM_TICK_QUERY,
    COLLECT_MODEM_LOG,
    COLLECT_WCN_LOG,
    COLLECT_AGDSP_LOG,
    COLLECT_PMSH_LOG,
    COLLECT_GNSS_LOG,
    SAVE_EVENT_LOG,
    SAVE_MODEM_LAST_LOG,
    CONVEY_LOG,
    MODEM_PARSE_LIB_SAVE,
    STOP_WCN_LOG,
    DUMP_RECEIVER,
    ORCA_DUMP,
    ORCA_ETB_DUMP
  };

  TransModem(TransactionManager* mgr, Type t);
  TransModem(const TransModem&) = delete;

  TransModem& operator = (const TransModem&) = delete;

  Type type() const { return type_; }

  bool use_diag() const { return use_diag_; }

 private:
  Type type_;
  bool use_diag_;
};

#endif  // !TRANS_MODEM_H_
