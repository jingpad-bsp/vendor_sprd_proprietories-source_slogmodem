/*
 *  trans_agdsp_col.h - The AGDSP log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-6-15 YAN Zhihang
 *  Initial version.
 */

#ifndef TRANS_AGDSP_COL_H_
#define TRANS_AGDSP_COL_H_

#include "trans_global.h"

class AgDspLogHandler;

class TransAgDspLogCollect : public TransGlobal {
 public:
  TransAgDspLogCollect(AgDspLogHandler* ag);
  TransAgDspLogCollect(const TransAgDspLogCollect&) = delete;
  TransAgDspLogCollect& operator = (const TransAgDspLogCollect&) = delete;

  void cancel() {}
  int execute() override;

 private:
  AgDspLogHandler* ag_dsp_;
};

#endif  // !TRANS_AGDSP_COL_H_
