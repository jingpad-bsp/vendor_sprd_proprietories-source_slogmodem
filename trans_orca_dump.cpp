/*
 *  trans_orca_dump.cpp - The ORCA MODEM/PM-SensorHub dump class.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *
 *  2019-4-10 Elon li
 *  Initial version.
 */

#include <unistd.h>

#include "cp_stor.h"
#include "diag_dev_hdl.h"
#include "modem_ioctl.h"
#include "multiplexer.h"
#include "parse_utils.h"
#include "trans_orca_dump.h"
#include "wan_modem_log.h"

TransOrcaDump::TransOrcaDump(TransactionManager* tmgr,
                             LogPipeHandler* subsys,
                             CpStorage& cp_stor,
                             const struct tm& lt,
                             CpStateHandler::CpEvent evt,
                             const char* prefix,
                             const char* end_of_content,
                             uint8_t cmd)
    : TransCpDump{tmgr, ORCA_DUMP, subsys, cp_stor, lt, prefix},
      time_{lt},
      cp_stor_{cp_stor},
      end_of_content_{end_of_content},
      cmd_{cmd},
      prefix_{prefix},
      evt_{evt},
      subsys_{subsys},
      client_{},
      ongoing_cb_{},
      dump_rcv_{} {
  CpType cp_type{subsys_->type()};

  // MODEM & Non-DP: try sending command first.
  if ((CT_WCDMA <= cp_type && cp_type <= CT_5MODE) ||
      CT_WANMODEM == cp_type) {
    if (CpStateHandler::CE_PANIC == evt_) {
      fault_subsys_ = FS_ORCA_SYS;
      dump_path_ = kModemDevPath;
    } else if (CpStateHandler::CE_ASSERT == evt_ ||
               CpStateHandler::CE_BLOCKED == evt_) {  // Non-DP
      fault_subsys_ = FS_MODEM;
      dump_path_ = kModemDevPath;
    } else {
      fault_subsys_ = FS_MODEM_DP;
      dump_path_ = kDpDevPath;
    }
  } else {
    fault_subsys_ = FS_PM_SH;
    dump_path_ = kPmMemDevPath;
  }
}

TransOrcaDump::~TransOrcaDump() {
  TransOrcaDump::cancel();
}

int TransOrcaDump::execute() {
  int err{TRANS_E_ERROR};

  if (FS_MODEM == fault_subsys_) {  // Try sending command first.
    dump_rcv_ = new TransDumpReceiver{nullptr, subsys_, cp_stor_,
                                      time_,
                                      prefix_, end_of_content_,
                                      cmd_};

    dump_rcv_->set_client(this, recv_result);
    dump_rcv_->set_periodic_callback(this, ongoing_report);

    err = dump_rcv_->execute();
    switch (err) {
      case Transaction::TRANS_E_STARTED:
        on_started();
        return Transaction::TRANS_E_STARTED;
      default:
        delete dump_rcv_;
        dump_rcv_ = nullptr;
        break;
    }
  }

  // Save memory device file directly.
  if (save_mem_file()) {
    on_finished(TRANS_R_SUCCESS);
    err = TRANS_E_SUCCESS;
  } else {
    on_finished(TRANS_R_FAILURE);
    err = TRANS_E_ERROR;
  }

  return err;
}

void TransOrcaDump::recv_result(void* client, Transaction* trans) {
  TransOrcaDump* dump = static_cast<TransOrcaDump*>(client);
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

void TransOrcaDump::ongoing_report(void* client) {
  TransOrcaDump* dump = static_cast<TransOrcaDump*>(client);

  if (dump->ongoing_cb_) {
    dump->ongoing_cb_(dump->client_);
  }
}

bool TransOrcaDump::save_mem_file() {
  if (get_load_info(ls2cstring(dump_path_), &load_info_)) {
    err_log("load info error");
    return false;
  }

  const char* mid_name{"memory"};

  switch (fault_subsys_) {
    case FS_ORCA_SYS:
      region_ = 0xff;
      region_size_ = load_info_.all_size;
      break;
    case FS_MODEM://dump all. only modem : region_ = 0xfe; region_size_ = load_info_.modem_size;
      region_ = 0xff;
      region_size_ = load_info_.all_size;
      break;
    case FS_MODEM_DP:
      region_size_ = load_info_.regions[region_].size;
      mid_name = "memory_dp";
      break;
    default:  // FS_PM_SH
      region_size_ = load_info_.regions[region_].size;
      break;
  }

  info_log("%s loadinfo size is %d", ls2cstring(dump_path_), region_size_);

  bool ret{};

  if (!set_dump_region(ls2cstring(dump_path_), region_)) {
    ret = copy_dump_content(mid_name, ls2cstring(dump_path_));
  } else {
    err_log("set_dump_region %s error", ls2cstring(dump_path_));
  }

  return ret;
}

void TransOrcaDump::cancel() {
  if (dump_rcv_) {
    dump_rcv_->cancel();
    delete dump_rcv_;
    dump_rcv_ = nullptr;

    on_canceled();
  }
}
