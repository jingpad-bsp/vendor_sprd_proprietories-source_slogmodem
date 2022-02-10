/*
 *  get_preferred_storage.h - get the preferred storage for log saving
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-11-27 YAN Zhihang
 *  Initial version.
 */
#ifndef GET_PREFERRED_STOR_H_
#define GET_PREFERRED_STOR_H_

#include "slogm_req.h"

class GetPreferredStorage : public SlogmRequest {
 public:
  GetPreferredStorage() {}
  GetPreferredStorage(const GetPreferredStorage&) = delete;
  GetPreferredStorage& operator = (const GetPreferredStorage&) = delete;

 protected:
  /*  do_request - implement the request.
   *
   *  Return 0 on success, -1 on failure.
   */
  int do_request() override;

 private:
  int parse_query_result();
};

#endif  // !GET_PREFERRED_STOR_H_
