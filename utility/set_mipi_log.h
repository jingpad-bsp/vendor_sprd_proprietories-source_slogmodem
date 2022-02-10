/*
 *  set_mipi_log.h - set mipi log request class.
 *
 *  Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 *  History:
 *  2020-3-10 Chunlan Wang
 *  Initial version.
 */
#ifndef SET_MIPI_LOG_H_
#define SET_MIPI_LOG_H_

#include "slogm_req.h"

class SetMipiLog : public SlogmRequest {
 public:
    SetMipiLog(LogString type, LogString channel, LogString freq);

 protected:
  /*  do_request - implement the request.
   *
   *  Return 0 on success, -1 on failure.
   */
  int do_request() override;

 private:
  LogString m_type;
  LogString m_channel;
  LogString m_freq;
};

#endif  // !SET_MIPI_LOG_H_
