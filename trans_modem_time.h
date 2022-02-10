/*
 *  trans_modem_time.h - The MODEM time sync transaction class.
 *
 *  Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 *  History:
 *  2018-09-28 Gloria He
 *  Initial version.
 */


#ifndef TRANS_MODEM_TIME_H_
#define TRANS_MODEM_TIME_H_
#include "cp_log_cmn.h"
#include "trans_modem.h"
#include "wan_modem_log.h"

class TransModemTimeSync : public TransModem {
 public:
  TransModemTimeSync(WanModemLogHandler* modem);

  TransModemTimeSync(const TransModemTimeSync&) = delete;

  ~TransModemTimeSync();

  TransModemTimeSync& operator = (const TransModemTimeSync&) = delete;

  int execute() override;

  void cancel() override;

  uint32_t get_modem_tick();

 private:
  static void subscribe_result(void* client,
      ModemAtController::ActionType act, int result);
  static void process_timeinfo(void* client,const uint8_t* line, size_t len);

 private:
  uint32_t tick_;
  WanModemLogHandler* wan_modem_;
};

#endif  // !TRANS_MODEM_TIME_H_
