/*
 *  get_cp_max_size.h - get storage's max size for log saving.
 *
 *  Copyright (C) 2016 Spreadtrum Communications Inc.
 *
 *  History:
 *  2016-11-2 YAN Zhihang
 *  Initial version.
 */
#ifndef GET_CP_MAX_SIZE_
#define GET_CP_MAX_SIZE_

#include "cplogctl_cmn.h"
#include "slogm_req.h"

class GetCpMaxSize : public SlogmRequest {
 public:
  GetCpMaxSize(CpType type, MediaType mt);

  GetCpMaxSize(const GetCpMaxSize&) = delete;
  GetCpMaxSize& operator=(const GetCpMaxSize&) = delete;

 protected:
  /*  do_request - implement the request.
   *
   *  Return 0 on success, -1 on failure.
   */
  int do_request() override;

 private:
  CpType m_type;
  MediaType mt_;

  int parse_query_result();
};

#endif  // !GET_CP_MAX_SIZE_
