/*
 *  trans_agdsp_col.cpp - The AGDSP log collection transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-6-15 YAN Zhihang
 *  Initial version.
 */

#include "agdsp_log.h"
#include "trans_agdsp_col.h"

TransAgDspLogCollect::TransAgDspLogCollect(AgDspLogHandler* ag)                   
    :TransGlobal{nullptr, AGDSP_LOG_COLLECT},
     ag_dsp_{ag} {}

int TransAgDspLogCollect::execute() {
  if (ag_dsp_->save_audio_parameter()) {
    on_finished(TRANS_R_FAILURE);
  } else {
    on_finished(TRANS_R_SUCCESS);
  }

  return TRANS_E_SUCCESS;
}
