/*
 *  trans_start_evt_log.h - The event triggered log start transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-5-26 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_START_EVT_LOG_H_
#define TRANS_START_EVT_LOG_H_

#include "modem_at_ctrl.h"
#include "trans_modem.h"

class WanModemLogHandler;

class TransStartEventLog : public TransModem {
 public:
  TransStartEventLog(WanModemLogHandler* modem);
  TransStartEventLog(const TransStartEventLog&) = delete;
  ~TransStartEventLog();

  TransStartEventLog& operator = (const TransStartEventLog&) = delete;

  int execute() override;
  void cancel() override;

 private:
  static void subscribe_result(void* client,
      ModemAtController::ActionType act, int result);

 private:
  WanModemLogHandler* wan_modem_;
};

#endif  // !TRANS_START_EVT_LOG_H_
