/*
 *  cp_convey_mgr.h - log pipe handler's convey transaction mgr
 *
 *  Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-10-12 Yan Zhihang
 *  Initial version.
 */

#ifndef _CP_CONVEY_MGR_
#define _CP_CONVEY_MGR_

#include "trans_mgr.h"

class Transaction;
class TransLogConvey;

class CpConveyManager : public TransactionManager {
 public:
  CpConveyManager() {}
  virtual ~CpConveyManager() {}
  /*
   * start_trans - start transaction
   *
   * @tls: log convey transaction
   *
   * Return Value:
   *   Transaction::TRANS_E_STARTED: transaction started and not finished.
   *   Transaction::TRANS_E_SUCCESS: transaction finished successfully.
   */
  int start_trans(TransLogConvey* tls);
  void trans_finished(Transaction* trans) override;
};

#endif //!_CP_CONVEY_MGR_
