/*
 *  trans_dump_rcv.cpp - The cellular MODEM dump receiver class.
 *
 *  Copyright (C) 2015-2018 Spreadtrum Communications Inc.
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *
 *  2015-6-15 Zhang Ziyi
 *  Initial version.
 *
 *  2017-6-3 Zhang Ziyi
 *  Join the Transaction class family.
 *
 *  2019-4-10 Elon li
 *  Only do data receiving transactions.
 */

#include <unistd.h>

#include "cp_dir.h"
#include "cp_set_dir.h"
#include "cp_stor.h"
#include "diag_dev_hdl.h"
#include "multiplexer.h"
#include "parse_utils.h"
#include "trans_dump_rcv.h"
#include "wan_modem_log.h"

TransDumpReceiver::TransDumpReceiver(TransactionManager* tmgr,
                                     LogPipeHandler* subsys,
                                     CpStorage& cp_stor,
                                     const struct tm& lt,
                                     const char* prefix,
                                     const char* end_of_content,
                                     uint8_t cmd)
    : TransCpDump{tmgr, DUMP_RECEIVER, subsys, cp_stor, lt, prefix},
      m_timer{},
      m_end_of_content{end_of_content},
      m_end_content_size{strlen(end_of_content)},
      m_cmd{cmd},
      dump_ongoing_{},
      silent_wins_{},
      client_{},
      on_going_cb_{} {
}

TransDumpReceiver::~TransDumpReceiver() {
  TransDumpReceiver::cancel();
}

int TransDumpReceiver::start_dump() {
  // Send command
  int ret {-1};
  uint8_t dump_cmd[2] = {m_cmd, 0xa};
  DiagDeviceHandler* diag = diag_handler();

  ssize_t n = ::write(diag->fd(), dump_cmd, 2);
  if (2 == n) {
    if (open_dump_file()) {
      storage().subscribe_stor_inactive_evt(this, notify_stor_inactive);
      ret = 0;
    }
  }

  return ret;
}

int TransDumpReceiver::execute() {
  int err = subsys()->start_diag_trans(*this,
                                       LogPipeHandler::CWS_DUMP);
  if (!err) {
    if (!start_dump()) {  // Start successfully.
      on_started();
      // notify dump ongoing
      if (on_going_cb_) {
        on_going_cb_(client_);
      }

      TimerManager& tmgr = diag_handler()->multiplexer()->timer_mgr();
      m_timer = tmgr.create_timer(1500, dump_read_check, this);
      err = TRANS_E_STARTED;
    } else {  // Start failed.
      err_log("send command error");
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

bool TransDumpReceiver::process(DataBuffer& buffer) {
  bool ret = false;

  // set the dump status
  dump_ongoing_ = true;
  // if the last frame
  bool found = check_ending(buffer);

  TimerManager& tmgr = diag_handler()->multiplexer()->timer_mgr();

  ssize_t n = 0;
  LogFile* f = dump_file();
  bool fail{};

  if (f) {
    n = f->write(buffer.buffer + buffer.data_start, buffer.data_len);
    if (static_cast<size_t>(n) != buffer.data_len) {  // Write file failed.
      remove_dump_file();
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
      tmgr.del_timer(m_timer);
      m_timer = nullptr;

      close_dump_file();
      storage().unsubscribe_stor_inactive_evt(this);

      subsys()->stop_diag_trans();

      finish(TRANS_R_SUCCESS);
      info_log("dump complete");
      ret = true;
    }
  }

  return ret;
}

void TransDumpReceiver::dump_read_check(void* param) {
  TransDumpReceiver* dump = static_cast<TransDumpReceiver*>(param);

  dump->m_timer = nullptr;

  // Notify ongoing
  if (dump->on_going_cb_) {
    dump->on_going_cb_(dump->client_);
  }

  // Check whether silent period is too long.
  if (dump->dump_ongoing_) {
    dump->silent_wins_ = 0;
  } else {
    ++dump->silent_wins_;
    if (dump->silent_wins_ >= 4) {  // Max 4 windows (6 seconds)
      err_log("read timeout, %lu bytes read",
              static_cast<unsigned long>(dump->dump_file()->size()));

      dump->remove_dump_file();
      dump->storage().unsubscribe_stor_inactive_evt(dump);

      dump->subsys()->stop_diag_trans();

      dump->finish(TRANS_R_FAILURE);
      return;
    }
  }

  dump->dump_ongoing_ = false;

  TimerManager& tmgr = dump->diag_handler()->multiplexer()->timer_mgr();
  dump->m_timer = tmgr.create_timer(1500, dump_read_check, dump);
}

bool TransDumpReceiver::check_ending(DataBuffer& buffer) {
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
      uint8_t* data_ptr = parser_.get_payload(dst_ptr);
      size_t data_len = dst_len - parser_.get_head_size();
      if (data_len == m_end_content_size &&
          !memcmp(data_ptr, m_end_of_content, data_len)) {
        ret = true;
      }
    }
  }
  return ret;
}

void TransDumpReceiver::notify_stor_inactive(void *client, unsigned priority) {
  TransDumpReceiver* dump = static_cast<TransDumpReceiver*>(client);

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

void TransDumpReceiver::cancel() {
  if (TS_EXECUTING == state()) {
    TimerManager& tmgr = diag_handler()->multiplexer()->timer_mgr();
    tmgr.del_timer(m_timer);
    m_timer = nullptr;

    remove_dump_file();
    storage().unsubscribe_stor_inactive_evt(this);

    subsys()->stop_diag_trans();

    on_canceled();
  }
}
