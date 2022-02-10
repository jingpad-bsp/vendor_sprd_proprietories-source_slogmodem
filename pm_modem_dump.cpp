/*
 *  pm_modem_dump.cpp - The cellular MODEM and PM Sensorhub dump class..
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *
 *  2019-4-10 Elon li
 *  Initial version.
 */

#include <unistd.h>

#include "cp_dir.h"
#include "cp_set_dir.h"
#include "cp_stor.h"
#include "diag_dev_hdl.h"
#ifdef USE_ORCA_MODEM
#include "modem_ioctl.h"
#include "modem_utils.h"
#endif
#include "multiplexer.h"
#include "parse_utils.h"
#include "pm_modem_dump.h"
#include "wan_modem_log.h"

TransPmModemDump::TransPmModemDump(TransactionManager* tmgr,
                                   LogPipeHandler* subsys,
                                   CpStorage& cp_stor,
                                   const struct tm& lt,
                                   const char* dump_path,
                                   const char* prefix,
                                   const char* end_of_content,
                                   uint8_t cmd)
    : TransCpDump{tmgr, MEMORY_DUMP, subsys, cp_stor, lt, prefix},
      dump_path_{dump_path},
      time_{lt},
      cp_stor_{cp_stor},
      end_of_content_{end_of_content},
      cmd_{cmd},
      prefix_{prefix},
      subsys_{subsys},
      client_{},
      ongoing_cb_{},
      dump_rcv_{} {}

TransPmModemDump::~TransPmModemDump() {
  TransPmModemDump::cancel();
}

int TransPmModemDump::execute() {
  dump_rcv_ = new TransDumpReceiver{nullptr, subsys_, cp_stor_,
                                    time_, prefix_,
                                    end_of_content_, cmd_};

  dump_rcv_->set_client(this, recv_result);
  dump_rcv_->set_periodic_callback(this, ongoing_report);

  int err = dump_rcv_->execute();

  switch (err) {
    case Transaction::TRANS_E_STARTED:
      on_started();
      break;
    default:
      on_finished(dump_rcv_->result());
      delete dump_rcv_;
      dump_rcv_ = nullptr;
      break;
  }

  return err;
}

void TransPmModemDump::recv_result(void* client, Transaction* trans) {
  TransPmModemDump* dump = static_cast<TransPmModemDump*>(client);
  int result{trans->result()};

  if (TRANS_R_SUCCESS != result) {
    if (dump->save_mem_file()) {
      result = TRANS_R_SUCCESS;
    } else {
      result = TRANS_R_FAILURE;
    }
  }

  delete trans;
  dump->dump_rcv_ = nullptr;

  dump->finish(result);
}

void TransPmModemDump::ongoing_report(void* client) {
  TransPmModemDump* dump = static_cast<TransPmModemDump*>(client);

  if (dump->ongoing_cb_) {
    dump->ongoing_cb_(dump->client_);
  }
}

bool TransPmModemDump::save_mem_file() {
  bool ret{};

  ret = copy_dump_content("memory", dump_path_);

  if (!access(AON_IRAM_FILE, F_OK)) {
    copy_dump_content("aon-iram", AON_IRAM_FILE);
  }
  if (!access(PUBCP_IRAM_FILE, F_OK)) {
    copy_dump_content("pubcp-iram", PUBCP_IRAM_FILE);
  }

  return ret;
}

void TransPmModemDump::cancel() {
  if (dump_rcv_) {
    dump_rcv_->cancel();
    delete dump_rcv_;
    dump_rcv_ = nullptr;

    on_canceled();
  }
}
