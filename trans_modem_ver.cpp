/*
 *  trans_modem_ver.cpp - The CP SleepLog class.
 *
 *  Copyright (C) 2016 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2016-6-8 Zhang Ziyi
 *  Initial version.
 *
 *  2017-6-2 Zhang Ziyi
 *  Join the Transaction class family.
 */

#include <unistd.h>

#include "diag_dev_hdl.h"
#include "multiplexer.h"
#include "trans_modem_ver.h"
#include "wan_modem_log.h"

TransWanModemVerQuery::TransWanModemVerQuery(WanModemLogHandler* modem)
    :TransDiagDevice{modem, VERSION_QUERY},
     wan_modem_{modem},
     m_max_times{3},
     m_failed_times{0},
     m_timer{nullptr},
     m_ver_len{0},
     m_modem_ver{nullptr} {}

TransWanModemVerQuery::~TransWanModemVerQuery() {
  TransWanModemVerQuery::cancel();
  if (m_modem_ver) {
    delete [] m_modem_ver;
  }
}

int TransWanModemVerQuery::send_query() {
  static uint8_t s_ver_req[] = { 0x7e, 0, 0, 0, 0, 8, 0, 0, 0, 0x7e };
  DiagDeviceHandler* dev = diag_handler();
  ssize_t nwr = write(dev->fd(), s_ver_req, sizeof s_ver_req);
  info_log("send_query path=%s, dev=%d, nwr=%d.", ls2cstring(dev->path()), dev->fd(), nwr);

  return nwr == sizeof s_ver_req ? 0 : -1;
}

int TransWanModemVerQuery::start_query() {
  // Send version query command
  if (send_query()) {
    err_log("send MODEM version query error");
    return -1;
  }

  info_log("querying MODEM version ...");

  // Start timer
  DiagDeviceHandler* dev = diag_handler();
  TimerManager& tmgr = dev->multiplexer()->timer_mgr();

  m_timer = tmgr.create_timer(2000, ver_query_timeout, this);

  return 0;
}

int TransWanModemVerQuery::execute() {
  int ret = wan_modem_->start_diag_trans(*this,
                                         LogPipeHandler::CWS_QUERY_VER);
  if (!ret) {
    if (!start_query()) {
      on_started();
      ret = TRANS_E_STARTED;
    } else {
      wan_modem_->stop_diag_trans();
      on_finished(TRANS_R_FAILURE);
    }
  } else {
    err_log("start diag handler error");
    on_finished(TRANS_R_FAILURE);
    ret = TRANS_E_SUCCESS;
  }

  return ret;
}

void TransWanModemVerQuery::cancel() {
  if (TS_EXECUTING == state()) {
    DiagDeviceHandler* dev = diag_handler();
    TimerManager& tmgr = dev->multiplexer()->timer_mgr();
    tmgr.del_timer(m_timer);
    m_timer = nullptr;

    parser_.reset();

    wan_modem_->stop_diag_trans();

    on_canceled();
  }
}

bool TransWanModemVerQuery::process(DataBuffer& buffer) {
  // Parse the data
  bool ret {false};

  while (buffer.data_len) {
    uint8_t* frame_ptr;
    size_t frame_len;
    size_t parsed_len;
    bool has_frame = parser_.unescape(buffer.buffer + buffer.data_start,
                                      buffer.data_len,
                                      &frame_ptr, &frame_len,
                                      &parsed_len);
    if (has_frame && frame_len > 8 && !frame_ptr[6] && !frame_ptr[7]) {  // Version result
      DiagDeviceHandler* dev = diag_handler();
      TimerManager& tmgr = dev->multiplexer()->timer_mgr();
      tmgr.del_timer(m_timer);
      m_timer = nullptr;

      m_ver_len = frame_len - 8;
      m_modem_ver = new char[m_ver_len];
      memcpy(m_modem_ver, frame_ptr + 8, m_ver_len);

      buffer.data_len = 0;

      wan_modem_->stop_diag_trans();

      ret = true;
      break;
    }

    buffer.data_start += parsed_len;
    buffer.data_len -= parsed_len;
  }

  if (buffer.data_len) {
    if (buffer.data_start) {
      memmove(buffer.buffer, buffer.buffer + buffer.data_start, buffer.data_len);
      buffer.data_start = 0;
    }
  } else {
    buffer.data_start = 0;
  }

  if (ret) {
    finish(TRANS_R_SUCCESS);
  }

  return ret;
}

void TransWanModemVerQuery::ver_query_timeout(void* param) {
  err_log("MODEM version query timeout");

  TransWanModemVerQuery* query = static_cast<TransWanModemVerQuery*>(param);

  query->m_timer = nullptr;

  ++query->m_failed_times;
  if (query->m_failed_times >= query->m_max_times) {  // Query failed
    query->wan_modem_->stop_diag_trans();
    query->finish(TRANS_R_FAILURE);
    return;
  }

  // Reset the parser state
  query->parser_.reset();

  if (query->start_query()) {
    query->wan_modem_->stop_diag_trans();
    query->finish(TRANS_R_FAILURE);
  }
}
