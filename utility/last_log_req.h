/*
 *  last_log_req.h - last log saving request class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-11-17 Zhang Ziyi
 *  Initial version.
 */

#ifndef LAST_LOG_REQ_H_
#define LAST_LOG_REQ_H_

#include "slogm_req.h"

class LastLogRequest : public SlogmRequest {
 public:
  LastLogRequest(CpType ct):subsys_{ct} {}
  LastLogRequest(const LastLogRequest&) = delete;
  LastLogRequest(LastLogRequest&&) = delete;

  LastLogRequest& operator = (const LastLogRequest&) = delete;
  LastLogRequest& operator = (LastLogRequest&&) = delete;

 protected:
  /*  do_request - implement the request.
   *
   *  Return 0 on success, -1 on failure.
   */
  int do_request() override;

 private:
  CpType subsys_;
};

#endif  // !LAST_LOG_REQ_H_
