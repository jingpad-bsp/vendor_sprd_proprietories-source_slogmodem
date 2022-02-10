/*
 *  trans_save_evt_log.cpp - The MODEM event log transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-6-13 Zhang Ziyi
 *  Initial version.
 */

#include <algorithm>
#include <cstring>

#include "cp_dir.h"
#include "cp_stor.h"
#include "parse_utils.h"
#include "trans_save_evt_log.h"
#include "wan_modem_log.h"

TransSaveEventLog::TransSaveEventLog(WanModemLogHandler* wan,
                                     CpStorage& stor,
                                     int sim,
                                     int evt,
                                     const CalendarTime& t)
    :TransModem{wan, SAVE_EVENT_LOG},
     DeviceFileHandler{wan->log_dev_path(),
                       kDataBufSize,
                       wan->controller(),
                       wan->multiplexer()},
     wan_modem_{wan},
     storage_{stor},
     sim_{sim},
     event_{evt},
     evt_time_{t},
     has_ta_{},
     time_alignment_{0x12345678, 0, 0, 0},
     read_timer_{},
     idle_timer_{},
     at_finished_{},
     at_result_{},
     data_finished_{},
     data_ok_{},
     log_size_{} {
}

TransSaveEventLog::~TransSaveEventLog() {
  TransSaveEventLog::cancel();
}

void TransSaveEventLog::set_time_alignment(const timeval& tnow, uint32_t cnt) {
  has_ta_ = true;
  time_alignment_.tv_sec = static_cast<uint32_t>(tnow.tv_sec);
  time_alignment_.tv_usec = static_cast<uint32_t>(tnow.tv_usec);
  time_alignment_.sys_cnt = cnt;
}

int TransSaveEventLog::execute() {
  if (LogPipeHandler::CWS_WORKING != wan_modem_->cp_state()) {
    err_log("MODEM not working, can not export event log");
    on_finished(TRANS_R_FAILURE);
    return TRANS_E_SUCCESS;
  }

  evt_file_ = create_file();
  auto f = evt_file_.lock();
  if (!f) {
    err_log("create event log file error");
    on_finished(TRANS_R_FAILURE);
    return TRANS_E_SUCCESS;
  }

  if (has_ta_) {
    f->write(&time_alignment_, sizeof time_alignment_);
  }

  // Get ready for the diag data
  int err = open_reg();
  if (err >= 0) {
    if (!send_command()) {  // Start successfully.
      TimerManager& tmgr = wan_modem_->multiplexer()->timer_mgr();
      read_timer_ = tmgr.create_timer(1500, evt_log_read_timeout, this);
      idle_timer_ = tmgr.create_timer(1000, data_idle_timeout, this);
      on_started();
      err = TRANS_E_STARTED;
    } else {  // Start failed.
      err_log("send event log export AT error");

      close_unreg();
      close_rm_file();

      on_finished(TRANS_R_FAILURE);
      err = TRANS_E_SUCCESS;
    }
  } else {  // Can not open the log device
    err_log("start diag handler error");

    close_rm_file();

    on_finished(TRANS_R_FAILURE);
    err = TRANS_E_SUCCESS;
  }

  return err;
}

std::weak_ptr<LogFile> TransSaveEventLog::create_file() {
  char fn[128];

  snprintf(fn, sizeof fn,
           "exception_%04d%02d%02d-%02d%02d%02d-%03d_sim%d_%x.log",
           evt_time_.year, evt_time_.mon, evt_time_.day,
           evt_time_.hour, evt_time_.min, evt_time_.sec, evt_time_.msec,
           sim_, event_);

  return storage_.create_file(fn, LogFile::LT_UNKNOWN);
}

void TransSaveEventLog::close_rm_file() {
  if (auto f = evt_file_.lock()) {
    f->close();
    f->dir()->remove(f);
    evt_file_.reset();
  }
}

int TransSaveEventLog::send_command() {
  ModemAtController& ctrl = wan_modem_->at_controller();
  return ctrl.start_export_evt_log(this, event_log_cmd_result, 40000);
}

void TransSaveEventLog::event_log_cmd_result(void* client,
                                             ModemAtController::ActionType t,
                                             int result) {
  TransSaveEventLog* evt_log = static_cast<TransSaveEventLog*>(client);

  if (!evt_log->at_finished_) {
    evt_log->at_result_ = result;
    evt_log->at_finished_ = true;

    if (evt_log->data_finished_) {
      // Log finished

      int trans_res {TRANS_R_FAILURE};
      if (ModemAtController::RESULT_OK == result &&
          evt_log->data_ok_) {
        trans_res = TRANS_R_SUCCESS;
      }

      evt_log->finish(trans_res);
    }
  }
}

void TransSaveEventLog::cancel() {
  if (TS_EXECUTING == state()) {
    // Delete the timer
    TimerManager& tmgr = wan_modem_->multiplexer()->timer_mgr();
    if (read_timer_) {
      tmgr.del_timer(read_timer_);
      read_timer_ = nullptr;
    }
    if (idle_timer_) {
      tmgr.del_timer(idle_timer_);
      idle_timer_ = nullptr;
    }

    if (!at_finished_) {
      wan_modem_->at_controller().cancel_trans();
      at_finished_ = true;
    }

    wan_modem_->stop_diag_trans();

    close_rm_file();

    on_canceled();
  }
}

bool TransSaveEventLog::process_data(DataBuffer& buffer) {
  // Defer the timeout
  TimerManager& tmgr = wan_modem_->multiplexer()->timer_mgr();
  tmgr.set_new_due_time(idle_timer_, 1000);

  log_size_ += buffer.data_len;

  uint8_t* p = buffer.buffer + buffer.data_start;
  size_t dlen = buffer.data_len;

  auto f = evt_file_.lock();
  if (f) {
    ssize_t nw = f->write(p, dlen);
    if (static_cast<size_t>(nw) != dlen) {  // Write error
      err_log("write event log file error %d",
              static_cast<int>(nw));
      close_rm_file();
      data_finished_ = true;
    } else {
      if (13 <= dlen &&
          find_str(p, dlen,
                   reinterpret_cast<const uint8_t*>("EVT LOG END"),
                   11)) {
        data_ok_ = true;

        f->close();
        evt_file_.reset();

        data_finished_ = true;
      }
    }
  } else {
    err_log("event log file deleted");
    data_finished_ = true;
    evt_file_.reset();
    data_finished_ = true;
  }

  buffer.data_start = 0;
  buffer.data_len = 0;

  if (data_finished_) {
    tmgr.del_timer(read_timer_);
    read_timer_ = nullptr;
    tmgr.del_timer(idle_timer_);
    idle_timer_ = nullptr;

    if (at_finished_) {  // Transaction finished.
      int res {TRANS_R_FAILURE};

      if (ModemAtController::RESULT_OK == at_result_ && data_ok_) {
        res = TRANS_R_SUCCESS;
      }

      finish(res);
    }
  }

  return data_finished_;
}

void TransSaveEventLog::evt_log_read_timeout(void* param) {
  TransSaveEventLog* evt_log = static_cast<TransSaveEventLog*>(param);

  // Stop all timers
  evt_log->read_timer_ = nullptr;

  TimerManager& tmgr = evt_log->wan_modem_->multiplexer()->timer_mgr();
  tmgr.del_timer(evt_log->idle_timer_);
  evt_log->idle_timer_ = nullptr;

  evt_log->on_timeout();
}

void TransSaveEventLog::data_idle_timeout(void* param) {
  TransSaveEventLog* evt_log = static_cast<TransSaveEventLog*>(param);

  // Stop all timers
  evt_log->idle_timer_ = nullptr;

  TimerManager& tmgr = evt_log->wan_modem_->multiplexer()->timer_mgr();
  tmgr.del_timer(evt_log->read_timer_);
  evt_log->read_timer_ = nullptr;

  evt_log->on_timeout();
}

void TransSaveEventLog::on_timeout() {
  if (auto f = evt_file_.lock()) {
    f->close();
    evt_file_.reset();
  }
  data_finished_ = true;

  if (log_size_) {
    info_log("event log received %lu bytes",
             static_cast<unsigned long>(log_size_));
  } else {
    err_log("no event log received");
  }

  if (at_finished_) {
    finish(TRANS_R_SUCCESS);
  }
}
