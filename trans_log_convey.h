/*
 *  tans_log_convey.h - The log convey class
 *
 *  Copyright (C) 2015 - 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-10-13 YAN Zhihang
 *  Initial version.
 *
 */

#ifndef _TRANS_LOG_CONVEY_
#define _TRANS_LOG_CONVEY_

#include <memory>

#include "cp_log_cmn.h"
#include "trans_modem.h"

class ConveyUnitBase;
class ConveyWorkshop;

class TransLogConvey : public TransModem {
 public:
  TransLogConvey(TransactionManager* tmgr, TransModem::Type type,
                 ConveyWorkshop* workshop);
  TransLogConvey(const TransLogConvey&) = delete;
  ~TransLogConvey();
  TransLogConvey& operator=(const TransLogConvey&) = delete;

  // Transaction::execute()
  virtual int execute();
  // Transaction::cancel()
  virtual void cancel();

  void add_units(LogVector<std::unique_ptr<ConveyUnitBase>>& items);

 private:
  bool validation_check(std::unique_ptr<ConveyUnitBase>& item);

  static void unit_finish(void* trans, std::unique_ptr<ConveyUnitBase>&& item);

 private:
  ConveyWorkshop* workshop_;
  LogList<std::unique_ptr<ConveyUnitBase>> trans_list_;
  unsigned item_num_;
  unsigned fail_num_;
};

#endif //!_TRANS_LOG_CONVEY_
