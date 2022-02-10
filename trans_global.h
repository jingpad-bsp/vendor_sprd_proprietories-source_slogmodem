/*
 *  trans_global.h - The global category transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-5-26 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_GLOBAL_H_
#define TRANS_GLOBAL_H_

#include "trans.h"

class TransGlobal : public Transaction {
 public:
  enum Type {
    LOG_COLLECT,
    AGDSP_LOG_COLLECT,
    ENABLE_LOG,
    DISABLE_LOG,
    WCN_LAST_LOG
  };

  TransGlobal(TransactionManager* mgr, Type t);
  TransGlobal(const TransGlobal&) = delete;

  TransGlobal& operator = (const TransGlobal&) = delete;

  Type type() const { return type_; }

 private:
  Type type_;
};

#endif  // !TRANS_GLOBAL_H_
