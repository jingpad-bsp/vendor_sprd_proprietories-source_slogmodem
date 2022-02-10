/*
 *  collect_req.h - log collection request class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-6-9 Zhang Ziyi
 *  Initial version.
 */

#ifndef COLLECT_REQ_H_
#define COLLECT_REQ_H_

#include "slogm_req.h"

class CollectRequest : public SlogmRequest {
 public:
  CollectRequest(const LogVector<CpType>& types);
  CollectRequest(const CollectRequest&) = delete;

  CollectRequest& operator = (const CollectRequest&) = delete;

 protected:
  /*  do_request - implement the request.
   *
   *  Return 0 on success, -1 on failure.
   */
  int do_request() override;

 private:
  uint8_t* prepare_cmd(size_t& len);

 private:
  LogVector<CpType> sys_list_;
};

#endif  // !COLLECT_REQ_H_
