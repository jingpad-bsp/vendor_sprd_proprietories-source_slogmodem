/*
 *  set_orca_dp.h - set orca dp request class.
 *
 *  Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 *  History:
 *  2020-4-4 Chunlan.Wang
 *  Initial version.
 */
#ifndef SET_ORCA_DP_H_
#define SET_ORCA_DP_H_

#include "slogm_req.h"

class SetOrcaDp : public SlogmRequest {
 public:
    SetOrcaDp(bool orcadp_on);

 protected:
  /*  do_request - implement the request.
   *
   *  Return 0 on success, -1 on failure.
   */
  int do_request() override;

 private:
  bool m_orcadp_on;
};

#endif  // !SET_ORCA_DP_H_
