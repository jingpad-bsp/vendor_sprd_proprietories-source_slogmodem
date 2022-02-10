/*
 *  trans_normal_log.h - The normal log start transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-6-20 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_NORMAL_LOG_H_
#define TRANS_NORMAL_LOG_H_

#include "modem_at_ctrl.h"
#include "trans_modem.h"

class WanModemLogHandler;

class TransStartNormalLog : public TransModem {
 public:
  TransStartNormalLog(WanModemLogHandler* modem);
  TransStartNormalLog(const TransStartNormalLog&) = delete;
  ~TransStartNormalLog();

  TransStartNormalLog& operator = (const TransStartNormalLog&) = delete;

  int execute() override;
  void cancel() override;

 private:
  static void unsubscribe_result(void* client,
      ModemAtController::ActionType act, int result);

 private:
  WanModemLogHandler* wan_modem_;
};

#endif  // !TRANS_NORMAL_LOG_H_
