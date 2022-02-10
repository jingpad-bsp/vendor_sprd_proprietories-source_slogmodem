/*
 *  ext_wcn_dump.cpp - The external WCN dump class.
 *
 *  Copyright (C) 2015-2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2015-6-15 Zhang Ziyi
 *  Initial version.
 *
 *  2017-6-3 Zhang Ziyi
 *  Join the Transaction class family.
 */

#include "cp_dir.h"
#include "cp_set_dir.h"
#include "cp_stor.h"
#include "diag_dev_hdl.h"
#include "ext_wcn_dump.h"
#include "ext_wcn_log.h"
#include "multiplexer.h"
#include "parse_utils.h"

TransExtWcnDump::TransExtWcnDump(TransactionManager* tmgr,
                                 ExtWcnLogHandler* wcn,
                                 CpStorage& cp_stor,
                                 const struct tm& lt,
                                 const char* prefix)
    : TransCpDump{tmgr, EXT_WCN_MEMORY_DUMP, wcn, cp_stor, lt, prefix},
      read_timer_{nullptr} {}

TransExtWcnDump::~TransExtWcnDump() {
  TransExtWcnDump::cancel();
}

int TransExtWcnDump::execute() {
  int err = subsys()->start_diag_trans(*this,
                                       LogPipeHandler::CWS_DUMP);
  if (!err) {
    if (!start()) {  // Start successfully.
      on_started();
      TimerManager& tmgr = diag_handler()->multiplexer()->timer_mgr();
      read_timer_ = tmgr.create_timer(60000, dump_read_timeout, this);
      err = TRANS_E_STARTED;
    } else {  // Start failed.
      err_log("start ext WCN dump error");
      on_finished(TRANS_R_FAILURE);
      subsys()->stop_diag_trans();
      err = TRANS_E_SUCCESS;
    }
  } else {
    err_log("start diag handler error");
    on_finished(TRANS_R_FAILURE);
    err = TRANS_E_SUCCESS;
  }

  return err;
}

int TransExtWcnDump::start() {
  if (!open_dump_file()) {
    err_log("open dump file failed!");
    return -1;
  }
  storage().subscribe_stor_inactive_evt(this, notify_stor_inactive);

  return 0;
}

bool TransExtWcnDump::process(DataBuffer& buffer) {
  bool ret = false;
  int trans_res = TRANS_R_SUCCESS;

  LogFile* f = dump_file();
  size_t len = buffer.data_len;
  const uint8_t* p = buffer.buffer + buffer.data_start;
  ssize_t n = f->write(p, len);

  if (static_cast<size_t>(n) != len) {
    remove_dump_file();
    trans_res = TRANS_R_FAILURE;
    ret = true;
  } else if (find_str(p, len,
                      reinterpret_cast<const uint8_t*>
                          ("marlin_memdump_finish"), 
                      21)) {
    close_dump_file();
    ret =  true;
  }

  buffer.data_start = 0;
  buffer.data_len = 0;

  if (ret) {
    TimerManager& tmgr = diag_handler()->multiplexer()->timer_mgr();
    tmgr.del_timer(read_timer_);
    read_timer_ = nullptr;

    storage().unsubscribe_stor_inactive_evt(this);
    subsys()->stop_diag_trans();
    finish(trans_res);
  }

  return ret;
}

void TransExtWcnDump::notify_stor_inactive(void *client, unsigned priority) {
  TransExtWcnDump* dump = static_cast<TransExtWcnDump*>(client);
  if (TS_EXECUTING == dump->state()) {
    if (!dump->dump_file() ||
        (dump->dump_file() &&
         priority == dump->dump_file()->dir()->cp_set_dir()->priority())) {
      dump->storage().unsubscribe_stor_inactive_evt(dump);
      dump->cancel();
      dump->finish(TRANS_R_FAILURE);
    }
  }
}

void TransExtWcnDump::cancel() {
  if (TS_EXECUTING == state()) {
    TimerManager& tmgr = diag_handler()->multiplexer()->timer_mgr();
    tmgr.del_timer(read_timer_);
    read_timer_ = nullptr;

    remove_dump_file();
    storage().unsubscribe_stor_inactive_evt(this);

    subsys()->stop_diag_trans();

    on_canceled();
  }
}

void TransExtWcnDump::dump_read_timeout(void* param) {
  err_log("ext WCN read dump timeout");
  TransExtWcnDump* dump = static_cast<TransExtWcnDump*>(param);
  TimerManager& tmgr = dump->diag_handler()->multiplexer()->timer_mgr();
  tmgr.del_timer(dump->read_timer_);
  dump->read_timer_ = nullptr;

  dump->remove_dump_file();
  dump->storage().unsubscribe_stor_inactive_evt(dump);

  dump->subsys()->stop_diag_trans();

  dump->finish(TRANS_R_FAILURE);
}
