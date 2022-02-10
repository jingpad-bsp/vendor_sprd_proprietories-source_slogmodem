/*
 *  modem_at_ctrl.cpp - The MODEM AT controller.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-6-3 Zhang Ziyi
 *  Initial version.
 */

#include <climits>
#include <stdlib.h>
#include <unistd.h>

#include "modem_at_ctrl.h"
#include "multiplexer.h"
#include "parse_utils.h"

ModemAtController::ModemAtController(LogController* ctrl,
                                     Multiplexer* multiplexer)
    :DeviceFileHandler{4096, ctrl, multiplexer},
     state_{IDLE},
     action_{AT_NONE},
     client_{nullptr},
     result_cb_{nullptr},
     int_result_cb_{nullptr},
     event_client_{},
     event_cb_{},
     cmd_timer_{nullptr} {}

ModemAtController::~ModemAtController() {
  if (cmd_timer_) {
    multiplexer()->timer_mgr().del_timer(cmd_timer_);
  }
}

int ModemAtController::start_subscribe_event(void* client,
                                             ResultCallback_t cb) {
  if (AT_NONE != action_) {
    err_log("busy");
    return -1;
  }

  if (fd() < 0) {
    if (open_reg() < 0) {
      err_log("open %s error", ls2cstring(path()));
      return -1;
    }
  }

  // Send subscription command.
  ssize_t nw = ::write(fd(), "AT+ARMLOGEVT=1\r", 15);
  if (15 != nw) {
    err_log("send subscription AT error");
    close_unreg();
    return -1;
  }

  cmd_timer_ = multiplexer()->timer_mgr().create_timer(2500,
                                                       at_cmd_timeout,
                                                       this);

  state_ = BUSY;
  action_ = AT_SUBSCRIBE_EVENT;
  client_ = client;
  result_cb_ = cb;

  return 0;
}

int ModemAtController::start_get_tick(void* client,
                                      ResultCallback_t cb,
                                      IntResultCallback_t int_cb) {
  if (AT_NONE != action_) {
    err_log("busy");
    return -1;
  }
  if (fd() < 0) {
    if (open_reg() < 0) {
      err_log("open %s error", ls2cstring(path()));
      return -1;
    }
  }

  // Send subscription command.
  ssize_t nw = ::write(fd(), "AT+SPTIMEINFO=0,0\r", 18);
  if (18 != nw) {
    err_log("send subscription AT+SPTIMEINFO error");
    return -1;
  }

  cmd_timer_ = multiplexer()->timer_mgr().create_timer(2500,
                                                       at_cmd_timeout,
                                                       this);

  state_ = BUSY;
  action_ = AT_SYNC_TIME;
  client_ = client;
  result_cb_ = cb;
  int_result_cb_ = int_cb;

  return 0;
}

void ModemAtController::at_cmd_timeout(void* client) {
  ModemAtController* at_ctrl = static_cast<ModemAtController*>(client);
  at_ctrl->cmd_timer_ = nullptr;
  at_ctrl->on_command_result(RESULT_TIMEOUT);
}

int ModemAtController::start_unsubscribe_event(void* client,
                                               ResultCallback_t cb) {
  if (AT_NONE != action_) {
    err_log("busy");
    return -1;
  }

  if (fd() < 0) {
    if (open_reg() < 0) {
      err_log("open %s error", ls2cstring(path()));
      return -1;
    }
  }

  // Send unsubscription command.
  ssize_t nw = ::write(fd(), "AT+ARMLOGEVT=0\r", 15);
  if (15 != nw) {
    err_log("send unsubscription AT error");
    return -1;
  }

  cmd_timer_ = multiplexer()->timer_mgr().create_timer(2500,
                                                       at_cmd_timeout,
                                                       this);

  state_ = BUSY;
  action_ = AT_UNSUBSCRIBE_EVENT;
  client_ = client;
  result_cb_ = cb;

  return 0;
}

void ModemAtController::cancel_trans() {
  state_ = IDLE;
  action_ = AT_NONE;
  client_ = nullptr;
  result_cb_ = nullptr;
  int_result_cb_ = nullptr;
}

void ModemAtController::register_events(void* client,
                                        EventCallback_t cb) {
  //Debug
  if (fd() < 0) {
    if (open_reg() < 0) {
      err_log("open %s error", ls2cstring(path()));
      return;
    }
  }

  event_client_ = client;
  event_cb_ = cb;
}

void ModemAtController::unregister_events() {
  event_client_ = nullptr;
  event_cb_ = nullptr;
}

bool ModemAtController::process_data(DataBuffer& buf) {
  // Parse the unsolicited result code
  const uint8_t* p = buf.buffer + buf.data_start;
  const uint8_t* endp = p + buf.data_len;
  size_t len = buf.data_len;

  while (p < endp) {
    // Search for the opening '\n'
    const uint8_t* p1 = static_cast<uint8_t*>(
        const_cast<void*>(memchr(p, 0xa, len)));

    if (!p1) {  // No opening '\n'
      p = endp;
      break;
    }
    p = p1 + 1;
    if (p == endp) {  // '\n' is the last char
      p = p1;
      break;
    }

    const uint8_t* p2 = static_cast<uint8_t*>(
        const_cast<void*>(memchr(p, 0xa, endp - p)));
    if (!p2) {  // No closing '\n'
      p = p1;
      break;
    }

    size_t line_len = p2 - p1;
    if (line_len > 1) {  // Parse the line
      --line_len;
      parse_line(p1 + 1, line_len);
    }
    p = p2;
    len = endp - p;
  }

  len = endp - p;
  if (len) {
    if (len != buf.data_len) {
      memmove(buf.buffer, p, len);
      buf.data_start = 0;
    }
  } else {
    buf.data_start = 0;
  }
  buf.data_len = len;

  return false;
}

void ModemAtController::parse_line(const uint8_t* line, size_t len) {
  size_t tlen;
  const uint8_t* tok = get_token(line, len, tlen);

  if (!tok) {  // Empty line
    return;
  }

  if (2 == tlen && !memcmp(tok, "OK", 2)) {
    multiplexer()->timer_mgr().del_timer(cmd_timer_);
    cmd_timer_ = nullptr;
    on_command_result(RESULT_OK);
  } else if (5 == tlen && !memcmp(tok, "ERROR", 5)) {
    multiplexer()->timer_mgr().del_timer(cmd_timer_);
    cmd_timer_ = nullptr;
    on_command_result(RESULT_ERROR);
  } else if (tlen >= 12 && !memcmp(tok, "+ARMLOGEVTU:", 12)) {
    process_event(tok + 12, line + len - tok - 12);
  } else if (tlen >= 12 && !memcmp(tok, "+SPTIMEINFO:", 12)) {
    if (int_result_cb_) {
     int_result_cb_(client_, tok + 12, line + len - tok - 12);
    }
  }
}

void ModemAtController::on_command_result(int res) {
  if (AT_NONE != action_) {
    ActionType atype = action_;

    state_ = IDLE;
    action_ = AT_NONE;
    if (result_cb_) {
      result_cb_(client_, atype, res);
    }
  } else {
    info_log("result code %d when no command", res);
  }
}

void ModemAtController::process_event(const uint8_t* line, size_t len) {
  const uint8_t* p = line;
  const uint8_t* endp = line + len;

  while (p < endp) {
    if (!isblank(*p)) {
      break;
    }
    ++p;
  }
  if (p == endp) {
    return;
  }

  unsigned sim;
  unsigned evt;
  size_t parsed;

  if (parse_number(p, endp - p, sim, parsed)) {
    return;
  }

  p += parsed;
  while (p < endp) {
    if (!isspace(*p)) {
      break;
    }
    ++p;
  }
  if (p == endp) {
    return;
  }

  if (',' != *p || p + 1 == endp) {
    return;
  }

  ++p;
  while (p < endp) {
    if (!isblank(*p)) {
      break;
    }
    ++p;
  }
  if (p == endp) {
    return;
  }

  if (parse_number(p, endp - p, evt, parsed)) {
    return;
  }

  // There shall only be spaces afterwards.
  p += parsed;
  while (p < endp) {
    if (!isspace(*p)) {
      return;
    }
    ++p;
  }

  if (event_cb_) {
    event_cb_(event_client_, static_cast<int>(sim),
              static_cast<int>(evt));
  }
}

bool ModemAtController::is_evt_valid(unsigned long evt) {
  bool valid {false};

  if (evt <= ME_LIMITED_SERVICE) {
    valid = true;
  } else if (evt >= ME_REG_REJECTED && evt <= ME_PDP_ACT_REJECTED) {
    valid = true;
  } else if (evt >= ME_IMS_LINK_FAILURE && evt <= ME_IMS_REG_REJECTED) {
    valid = true;
  } else if (evt >= ME_LTE_REG_FAILURE && evt <= ME_LTE_MAX_RLC_RETR) {
    valid = true;
  } else if (evt >= ME_WCDMA_REG_FAILURE && evt <= ME_WCDMA_DECRYPT_FAILURE) {
    valid = true;
  } else if (evt >= ME_GSM_BAD_SACCH_REC && evt <= ME_GSM_MODE_SWITCH_FAILURE) {
    valid = true;
  }

  return valid;
}

int ModemAtController::crash() {
  bool need_close {false};

  if (fd() < 0) {
    if (open() < 0) {
      err_log("open %s error", ls2cstring(path()));
      return -1;
    }
    need_close = true;
  }

  ssize_t nw = write(fd(), "AT+SPATASSERT=1\r", 16);

  if (need_close) {
    close();
  }

  return 16 == nw ? 0 : -1;
}

int ModemAtController::start_export_evt_log(void* client,
                                            ResultCallback_t cb,
                                            unsigned to) {
  if (AT_NONE != action_) {
    err_log("busy");
    return -1;
  }

  if (fd() < 0) {
    if (open_reg() < 0) {
      err_log("open %s error", ls2cstring(path()));
      return -1;
    }
  }

  //Debug
  info_log("send AT+SPATDEBUG=1,1 ...");

  // Send export command.
  //ssize_t nw = ::write(fd(), "AT+SPATDEBUG=1,1\r", 17);
  ssize_t nw = ::write(fd(), "AT+SPMTADUPM\r", 13);
  if (13 != nw) {
    //err_log("send AT+SPATDEBUG=1,1 error");
    err_log("send AT+SPMTADUPM error");
    close_unreg();
    return -1;
  }

  cmd_timer_ = multiplexer()->timer_mgr().create_timer(to,
                                                       at_cmd_timeout,
                                                       this);

  state_ = BUSY;
  action_ = AT_EXPORT_LOG;
  client_ = client;
  result_cb_ = cb;

  return 0;
}
