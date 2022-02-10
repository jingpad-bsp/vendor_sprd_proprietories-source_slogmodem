/*
 *  cp_convey_mgr.cpp - log pipe handler's convey transaction mgr
 *
 *  Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-10-12 Yan Zhihang
 *  Initial version.
 */

#include "cp_convey_mgr.h"
#include "def_config.h"
#include "log_ctrl.h"
#include "log_pipe_hdl.h"
#include "media_stor.h"
#include "media_stor_check.h"
#include "move_dir_to_dir.h"
#include "stor_mgr.h"
#include "trans_log_convey.h"

int CpConveyManager::start_trans(TransLogConvey* tlc) {
  int ret = Transaction::TRANS_E_STARTED;
  if (number()) {
    enqueue(tlc);
  } else {
    ret = tlc->execute();
    switch (ret) {
      case Transaction::TRANS_E_STARTED:
        enqueue(tlc);
        break;
      default:
        break;
    }
  }

  return ret;
}

void CpConveyManager::trans_finished(Transaction* trans) {
  TransactionManager::trans_finished(trans);

  auto it = transactions_.begin();
  while (it != transactions_.end()) {
    TransModem* mt = static_cast<TransModem*>(*it);
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
