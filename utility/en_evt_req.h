/*
 *  en_evt_req.h - enable event log request class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-6-9 Zhang Ziyi
 *  Initial version.
 */

#ifndef EN_EVT_REQ_H_
#define EN_EVT_REQ_H_

#include "slogm_req.h"

class EnableEventLogRequest : public SlogmRequest {
 public:
  EnableEventLogRequest(CpType type);
  EnableEventLogRequest(const EnableEventLogRequest&) = delete;

  EnableEventLogRequest& operator = (const EnableEventLogRequest&) = delete;

 protected:
  /*  do_request - implement the request.
   *
   *  Return 0 on success, -1 on failure.
   */
  int do_request() override;

 private:
  CpType subsys_;

  uint8_t* prepare_cmd(size_t& len);
};

#endif  // !EN_EVT_REQ_H_
