/*
 *  trans_stop_cellular_log.h - The cellular log stop transaction class.
 *
 *  Copyright (C) 2017,2018 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-6-22 Zhang Ziyi
 *  Initial version.
 *
 *  2018-5-11 Zhang Ziyi
 *  Renamed to TransStopCellularLog.
 */

#ifndef TRANS_STOP_CELLULAR_LOG_H_
#define TRANS_STOP_CELLULAR_LOG_H_

#include "modem_at_ctrl.h"
#include "trans_modem.h"

class WanModemLogHandler;

class TransStopCellularLog : public TransModem {
 public:
  TransStopCellularLog(WanModemLogHandler* modem);
  TransStopCellularLog(const TransStopCellularLog&) = delete;
  ~TransStopCellularLog();

  TransStopCellularLog& operator = (const TransStopCellularLog&) = delete;

  int execute() override;
  void cancel() override;

 private:
  static void unsubscribe_result(void* client,
      ModemAtController::ActionType act, int result);

 private:
  WanModemLogHandler* wan_modem_;
};

#endif  // !TRANS_STOP_CELLULAR_LOG_H_
