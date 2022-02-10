/*
 *  set_mini_ap.h - set mini ap request class.
 *
 *  Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 *  History:
 *  2020-1-20 Chunlan.Wang
 *  Initial version.
 */
#ifndef SET_MINI_AP_H_
#define SET_MINI_AP_H_

#include "slogm_req.h"

class SetMiniAp : public SlogmRequest {
 public:
    SetMiniAp(bool miniap_on);

 protected:
  /*  do_request - implement the request.
   *
   *  Return 0 on success, -1 on failure.
   */
  int do_request() override;

 private:
  bool m_miniap_on;
};

#endif  // !SET_AG_PCM_H_
