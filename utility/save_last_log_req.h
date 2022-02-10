/*
 *  save_last_log.h - save last log request class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-9-4 YAN Zhihang
 *  Initial version.
 */

#ifndef SAVE_LAST_LOG_H_
#define SAVE_LAST_LOG_H_

#include "cp_log_cmn.h"
#include "slogm_req.h"

class SaveLastLogRequest : public SlogmRequest {
 public:
  SaveLastLogRequest() {}

  SaveLastLogRequest(const SaveLastLogRequest&) = delete;
  SaveLastLogRequest& operator = (const SaveLastLogRequest&) = delete;

 protected:
  /*  do_request - implement the request.
   *
   *  Return 0 on success, -1 on failure.
   */
  int do_request() override;

};

#endif  // !SAVE_LAST_LOG_H_
