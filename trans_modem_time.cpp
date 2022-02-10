/*
 *  trans_modem_time.cpp - The MODEM time sync transaction class.
 *
 *  Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 *  History:
 *  2018-09-28 Gloria He
 *  Initial version.
 */
#include <sys/sysinfo.h>

#include "trans_log_convey.h"
#include "trans_modem_time.h"
#include "wan_modem_log.h"
#include "parse_utils.h"

TransModemTimeSync::TransModemTimeSync(WanModemLogHandler* modem)
    :TransModem{modem, MODEM_TICK_QUERY},
     tick_{0},
     wan_modem_{modem} {}

TransModemTimeSync::~TransModemTimeSync() {
  TransModemTimeSync::cancel();
}

int TransModemTimeSync::execute() {
  int ret{TRANS_E_SUCCESS};
  ModemAtController& at_ctrl {wan_modem_->at_controller()};
  ret = at_ctrl.start_get_tick(this, subscribe_result,process_timeinfo);

  if (!ret) {
      on_started();
      ret = TRANS_E_STARTED;
  } else {
      err_log("start get tick error");
      on_finished(TRANS_R_FAILURE);
      ret = TRANS_E_ERROR;
  }
  return ret;
}

void TransModemTimeSync::cancel() {
  if (TS_EXECUTING == state()) {
    wan_modem_->at_controller().cancel_trans();
    on_canceled();
  }
}

uint32_t TransModemTimeSync::get_modem_tick() {
  return tick_;
}

void TransModemTimeSync::process_timeinfo(void* client,
                                          const uint8_t* line,
                                          size_t len) {

  LogString stime;
  str_assign(stime, reinterpret_cast<const char*>(line), len);
  info_log("time info len=%d, time_info=%s",len,ls2cstring(stime));

  TransModemTimeSync* trans_time_sync =
  static_cast<TransModemTimeSync*>(client);
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

  unsigned sub;
  unsigned time_type;
  unsigned time_info;
  size_t parsed;

  if (parse_number(p, endp - p, sub, parsed)) {
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

  if (parse_number(p, endp - p, time_type, parsed)) {
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

  if (parse_number(p, endp - p, time_info, parsed)) {
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

  switch (sub) {
    case 0:
      switch(time_type) {
        case 0:
          trans_time_sync->tick_ = time_info;
          info_log("MODEM time info: sys_cnt = %u",
          static_cast<unsigned>(trans_time_sync->tick_));
          break;
        default :
          err_log("not support time_type=%d",time_type);
          break;
      }
      break;
    default :
        err_log("not support sub=%d",sub);
        break;
  }
}

void TransModemTimeSync::subscribe_result(void* client,
    ModemAtController::ActionType act, int result) {
  TransModemTimeSync* trans_time_sync = static_cast<TransModemTimeSync*>(client);

  if (TS_EXECUTING == trans_time_sync->state()) {
    int res {TRANS_R_FAILURE};

    if (!result) {
      res = TRANS_R_SUCCESS;
      info_log("at sync modem time success");
    } else {
      err_log("at sync modem time error %d", result);
    }

    trans_time_sync->finish(res);
  }
}
