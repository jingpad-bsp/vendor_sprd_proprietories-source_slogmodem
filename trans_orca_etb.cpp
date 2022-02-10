/*
 *  trans_orca_etb.cpp - The etb dump class.
 *
 *  Copyright (C) 2020 UNISOC Communications Inc.
 *
 *  History:
 *
 *  2020-3-10 Chunlan Wang
 *  Initial version.
*/

#include <unistd.h>

#include "cp_dir.h"
#include "cp_set_dir.h"

#include "cp_stor.h"
#include "diag_dev_hdl.h"
#include "modem_ioctl.h"
#include "multiplexer.h"
#include "parse_utils.h"

#include "wan_modem_log.h"
#include "trans_orca_etb.h"
#include "modem_cmd_ctrl.h"


TransOrcaEtb::TransOrcaEtb(TransactionManager* tmgr,
                             WanModemLogHandler* subsys,
                             CpStorage& cp_stor,
                             const struct tm& lt,
                             CpStateHandler::CpEvent evt,
                             const char* prefix)
    : TransCpDump{tmgr, ORCA_ETB_DUMP, subsys, cp_stor, lt, prefix},
      time_{lt},
      prefix_{prefix},
      evt_{evt},
      subsys_{subsys},
      dump_ongoing_{},
      silent_wins_{} {
}

TransOrcaEtb::~TransOrcaEtb() {
  TransOrcaEtb::cancel();
}

int TransOrcaEtb::start_dump() {
  int ret {-1};
  //send command
  ModemCmdController&cmd_ctrl = subsys_->cmd_controller();
  cmd_ctrl.set_etb_save();
  info_log("send command sucess");

  //open_dump_etb_file
  if (open_dump_etb_file()){
    storage().subscribe_stor_inactive_evt(this, notify_stor_inactive);
    ret = 0;
  }

  return ret;
}


int TransOrcaEtb::execute() {
  info_log("TransOrcaEtb::execute");
  int err = subsys()->start_diag_trans(*this,
                                       LogPipeHandler::CWS_DUMP);
  info_log("start_diag_trans err=%d\n",err);
  if (!err) {
    if (!start_dump()) {  // Start successfully.
      info_log("start_dump");
      TimerManager& tmgr = diag_handler()->multiplexer()->timer_mgr();
      m_timer = tmgr.create_timer(1500, dump_etb_read_check, this);
      err = TRANS_E_STARTED;
    } else {  // Start failed.
      err_log("send command error");
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


bool TransOrcaEtb::process(DataBuffer& buffer) {
  info_log("etb process start");

  bool ret = false;

  // set the dump status
  dump_ongoing_ = true;
  // if the last frame
  bool found = check_etb_ending(buffer);
  TimerManager& tmgr = diag_handler()->multiplexer()->timer_mgr();

  ssize_t n = 0;
  LogFile* f = dump_etb_file();
  bool fail{};

  if (f) {
    n = f->write(buffer.buffer + buffer.data_start, buffer.data_len);
    if (static_cast<size_t>(n) != buffer.data_len) {  // Write file failed.
      remove_dump_etb_file();
      err_log("need to write %lu, write returns %d",
              static_cast<unsigned long>(buffer.data_len),
              static_cast<int>(n));
      fail = true;
    }
  } else {  // The file has been removed.
    err_log("dump file no longer exists");

    fail = true;
  }

  if (fail) {
    tmgr.del_timer(m_timer);
    m_timer = nullptr;
    storage().unsubscribe_stor_inactive_evt(this);

    subsys()->stop_diag_trans();

    finish(TRANS_R_FAILURE);
    ret = true;
  } else {
    buffer.data_start = 0;
    buffer.data_len = 0;
    if (found) {  // Finishing flag found
      info_log("etb process ready to finish");

      tmgr.del_timer(m_timer);
      m_timer = nullptr;

      close_dump_etb_file();
      storage().unsubscribe_stor_inactive_evt(this);

      subsys()->stop_diag_trans();

      finish(TRANS_R_SUCCESS);
      info_log("dump etb complete");
      ret = true;
    }
  }

  return ret;
}

bool TransOrcaEtb::check_etb_ending(DataBuffer& buffer){

  uint32_t m_third_packet_size = 16;
  uint8_t m_third_packet_content[16] = {0x03,0x00,0x00,0x00,0x10,0x00,0x62,0x00,0x33,0x00,0x04,0x00,0x02};

  uint8_t* src_ptr = buffer.buffer + buffer.data_start;
  size_t src_len = buffer.data_len;
  uint8_t* dst_ptr;
  size_t dst_len;
  size_t read_len;
  bool has_frame;
  bool ret = false;

  while (src_len) {
    has_frame =
        parser_.unescape(src_ptr, src_len, &dst_ptr, &dst_len, &read_len);
    src_len -= read_len;
    src_ptr += read_len;
    if (has_frame) {
      if (dst_len == m_third_packet_size &&
          !memcmp(dst_ptr, m_third_packet_content, dst_len)) {
        ret = true;
      }
    }
  }
  return ret;

}

void TransOrcaEtb::notify_stor_inactive(void *client, unsigned priority) {
  TransOrcaEtb* dump_etb = static_cast<TransOrcaEtb*>(client);

  if (TS_EXECUTING == dump_etb->state()) {
    if (!dump_etb->dump_etb_file() ||
        (dump_etb->dump_etb_file() &&
         priority == dump_etb->dump_etb_file()->dir()->cp_set_dir()->priority())) {
      dump_etb->storage().unsubscribe_stor_inactive_evt(dump_etb);
      dump_etb->cancel();
      dump_etb->finish(TRANS_R_FAILURE);
    }
  }
}

void TransOrcaEtb::cancel() {
  if (TS_EXECUTING == state()) {
    TimerManager& tmgr = diag_handler()->multiplexer()->timer_mgr();
    tmgr.del_timer(m_timer);
    m_timer = nullptr;

    remove_dump_etb_file();
    storage().unsubscribe_stor_inactive_evt(this);

    subsys()->stop_diag_trans();

    on_canceled();
  }
}

void TransOrcaEtb::dump_etb_read_check(void* param) {
  TransOrcaEtb* dump_etb = static_cast<TransOrcaEtb*>(param);

  dump_etb->m_timer = nullptr;
  // Check whether silent period is too long.
  if (dump_etb->dump_ongoing_) {
    dump_etb->silent_wins_ = 0;
  } else {
    ++dump_etb->silent_wins_;
    if (dump_etb->silent_wins_ >= 4) {  // Max 4 windows (6 seconds)
      err_log("read timeout, %lu bytes read",
              static_cast<unsigned long>(dump_etb->dump_etb_file()->size()));

      dump_etb->remove_dump_etb_file();
      dump_etb->storage().unsubscribe_stor_inactive_evt(dump_etb);

      dump_etb->subsys()->stop_diag_trans();

      dump_etb->finish(TRANS_R_FAILURE);
      return;
    }
  }

  dump_etb->dump_ongoing_ = false;

  TimerManager& tmgr = dump_etb->diag_handler()->multiplexer()->timer_mgr();
  dump_etb->m_timer = tmgr.create_timer(1500, dump_etb_read_check, dump_etb);
}
