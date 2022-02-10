/*
 *  get_ag_pcm.h - get mipi log request class.
 *
 *  Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 *  History:
 *  2020-3-10 Chunlan Wang
 *  Initial version.
 */
#ifndef GET_MIPI_LOG_H_
#define GET_MIPI_LOG_H_

#include "slogm_req.h"

class GetMipiLog : public SlogmRequest {
 public:
    GetMipiLog(LogString type);

 protected:
  /*  do_request - implement the request.
   *
   *  Return 0 on success, -1 on failure.
   */
  int do_request() override;
  int parse_query_result();

 private:
  LogString m_type;
};

#endif  // !GET_MIPI_LOG_H_
