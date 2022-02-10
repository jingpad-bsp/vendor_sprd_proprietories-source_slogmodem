/*
 *  trans_diag.h - The MODEM diagnosis device transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-5-31 Zhang Ziyi
 *  Initial version.
 */

#ifndef TRANS_DIAG_H_
#define TRANS_DIAG_H_

#include "trans_modem.h"

class DiagDeviceHandler;
class WanModemLogHandler;
struct DataBuffer;

class TransDiagDevice : public TransModem {
 public:
  TransDiagDevice(TransactionManager* tmgr, Type t);
  TransDiagDevice(const TransDiagDevice&) = delete;

  TransDiagDevice& operator = (const TransDiagDevice&) = delete;

  void bind(DiagDeviceHandler* diag_hdl) { diag_hdl_ = diag_hdl; }

  DiagDeviceHandler* diag_handler() const { return diag_hdl_; }

  /*  process - process data for the transaction.
   *  @buffer: data buffer.
   *
   *  Return Value:
   *    If the transaction is finished, return true; otherwise return
   *    false.
   */
  virtual bool process(DataBuffer& buffer) = 0;

 private:
  DiagDeviceHandler* diag_hdl_;
};

#endif  // !TRANS_DIAG_H_
