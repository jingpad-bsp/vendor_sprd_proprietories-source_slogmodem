/*
 *  start_evt_req.h - start event log request class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-6-16 Zhang Ziyi
 *  Initial version.
 */

#ifndef START_EVT_REQ_H_
#define START_EVT_REQ_H_

#include "slogm_req.h"

class StartEventLogRequest : public SlogmRequest {
 public:
  StartEventLogRequest() {}
  StartEventLogRequest(const StartEventLogRequest&) = delete;

  StartEventLogRequest& operator = (const StartEventLogRequest&) = delete;

 protected:
  /*  do_request - implement the request.
   *
   *  Return 0 on success, -1 on failure.
   */
  int do_request() override;
};

#endif  // !START_EVT_REQ_H_
