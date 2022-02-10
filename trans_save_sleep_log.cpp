/*
 *  trans_save_sleep_log.cpp - The MODEM sleep log transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-5-31 Zhang Ziyi
 *  Initial version.
 */

#include <unistd.h>

#include "cp_dir.h"
#include "cp_stor.h"
#include "diag_dev_hdl.h"
#include "trans_save_sleep_log.h"
#include "wan_modem_log.h"

TransSaveSleepLog::TransSaveSleepLog(WanModemLogHandler* wan,
                                     CpStorage& stor)
    :TransDiagDevice{wan, SAVE_SLEEP_LOG},
     wan_modem_{wan},
     storage_{stor},
     m_timer{nullptr} {}

TransSaveSleepLog::~TransSaveSleepLog() {
  TransSaveSleepLog::cancel();
}

int TransSaveSleepLog::execute() {
  if (!wan_modem_->enabled()) {
    on_finished(TRANS_R_LOG_NOT_ENABLED);
    return TRANS_E_SUCCESS;
  }

  if (LogPipeHandler::CWS_NOT_WORKING == wan_modem_->cp_state()) {
    on_finished(TRANS_R_CP_ASSERTED);
    return TRANS_E_SUCCESS;
  }

  int err = wan_modem_->start_diag_trans(*this,
                                         LogPipeHandler::CWS_SAVE_SLEEP_LOG);
  if (!err) {
    if (!start()) {  // Start successfully.
      on_started();
      err = TRANS_E_STARTED;
    } else {  // Start failed.
      err_log("send command error");
      on_finished(TRANS_R_FAILURE);
      wan_modem_->stop_diag_trans();
      err = TRANS_E_SUCCESS;
    }
  } else {
    err_log("start diag handler error");
    on_finished(TRANS_R_FAILURE);
    err = TRANS_E_SUCCESS;
  }

  return err;
}

int TransSaveSleepLog::start() {
  m_file = open_file();
  if (0 == m_file.use_count()) {
    return -1;
  }

  uint8_t* buf;
  size_t len;
  parser_.frame(DIAG_READ_SLEEP_LOG, DIAG_REQ_SLEEP_LOG, NULL, 0, &buf, &len);
  DiagDeviceHandler* dev = diag_handler();
  ssize_t n = write(dev->fd(), buf, len);
  delete[] buf;
  if (static_cast<size_t>(n) != len) {
    close_rm_file();
    return -1;
  }
  // Set read timeout
  TimerManager& tmgr = dev->multiplexer()->timer_mgr();
  m_timer = tmgr.create_timer(3000, sleep_log_read_timeout, this);
  return 0;
}

bool TransSaveSleepLog::process(DataBuffer& buffer) {
  uint8_t* src_ptr = buffer.buffer + buffer.data_start;
  size_t src_len = buffer.data_len;
  uint8_t* dst_ptr;
  size_t dst_len;
  size_t read_len;
  bool has_frame;
  bool ret = false;

  // Reset timer
  TimerManager& tmgr = diag_handler()->multiplexer()->timer_mgr();
  tmgr.set_new_due_time(m_timer, 3000);

  while (src_len) {
    has_frame =
        parser_.unescape(src_ptr, src_len, &dst_ptr, &dst_len, &read_len);
    src_len -= read_len;
    src_ptr += read_len;
    if (has_frame) {
      if (DIAG_READ_SLEEP_LOG == parser_.get_type(dst_ptr)) {
        uint8_t subtype = parser_.get_subytpe(dst_ptr);

        if (DIAG_RSP_SLEEP_LOG_OK == subtype) {
          tmgr.del_timer(m_timer);
          m_timer = nullptr;
          close_file();
          wan_modem_->stop_diag_trans();
          finish(TRANS_R_SUCCESS);
          ret = true;
          break;
        } else if (DIAG_DATA_SLEEP_LOG == subtype) {
          uint8_t* data_ptr = parser_.get_payload(dst_ptr);
          size_t data_len = dst_len - parser_.get_head_size();
          auto f = m_file.lock();
          ssize_t n = 0;
          if (f) {
            n = f->write(data_ptr, data_len);
          }

          if (static_cast<size_t>(n) != data_len) {
            tmgr.del_timer(m_timer);
            m_timer = nullptr;
            close_rm_file();
            wan_modem_->stop_diag_trans();
            finish(TRANS_R_FAILURE);
            ret = true;
            break;
          }
        } else if (DIAG_REQ_SLEEP_LOG == subtype) {
          tmgr.del_timer(m_timer);
          m_timer = nullptr;
          close_rm_file();
          wan_modem_->stop_diag_trans();
          finish(TRANS_R_SLEEP_LOG_NOT_SUPPORTED);
          ret = true;
          break;
        } else {  // Unknown response
          tmgr.del_timer(m_timer);
          m_timer = nullptr;
          close_rm_file();
          wan_modem_->stop_diag_trans();
          finish(TRANS_R_FAILURE);
          ret = true;
          break;
        }
      }
    }
  }
  if (!ret) {
    buffer.data_start = 0;
    buffer.data_len = 0;
  }
  return ret;
}

void TransSaveSleepLog::sleep_log_read_timeout(void* param) {
  err_log("read sleep log timeout");

  TransSaveSleepLog* trans = static_cast<TransSaveSleepLog*>(param);

  trans->m_timer = nullptr;
  trans->close_file();
  trans->wan_modem_->stop_diag_trans();
  trans->finish(TRANS_R_FAILURE);
}

void TransSaveSleepLog::cancel() {
  if (TS_EXECUTING == state()) {
    // Delete the timer
    TimerManager& tmgr = diag_handler()->multiplexer()->timer_mgr();
    tmgr.del_timer(m_timer);
    m_timer = nullptr;
    close_rm_file();

    wan_modem_->stop_diag_trans();

    on_canceled();
  }
}

std::weak_ptr<LogFile> TransSaveSleepLog::open_file() {
  char log_name[64];
  time_t t;
  struct tm lt;
  std::weak_ptr<LogFile> f;

  t = time(0);
  if (static_cast<time_t>(-1) == t || !localtime_r(&t, &lt)) {
    return f;
  }

  snprintf(log_name, sizeof log_name,
           "md_sleeplog_%04d%02d%02d-%02d%02d%02d.log",
           lt.tm_year + 1900,
           lt.tm_mon + 1,
           lt.tm_mday,
           lt.tm_hour,
           lt.tm_min,
           lt.tm_sec);
  LogString file_name = log_name;
  f = storage_.create_file(file_name, LogFile::LT_SLEEPLOG);
  if (f.expired()) {
    err_log("open sleep log file %s failed", ls2cstring(file_name));
  }

  return f;
}

void TransSaveSleepLog::close_file() {
  if (auto f = m_file.lock()) {
    f->close();
    m_file.reset();
  }
}

void TransSaveSleepLog::close_rm_file() {
  if (auto f = m_file.lock()) {
    f->close();
    f->dir()->remove(f);
    m_file.reset();
  }
}
