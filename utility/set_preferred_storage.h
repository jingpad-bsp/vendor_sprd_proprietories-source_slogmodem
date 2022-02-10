/*
 *  set_preferred_storage.h - set the preferred storage for log saving
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-11-27 YAN Zhihang
 *  Initial version.
 */
#ifndef SET_PREFERRED_STOR_H_
#define SET_PREFERRED_STOR_H_

#include "cplogctl_cmn.h"
#include "slogm_req.h"

class SetPreferredStorage : public SlogmRequest {
 public:
  SetPreferredStorage(MediaType mt);
  SetPreferredStorage(const SetPreferredStorage&) = delete;
  SetPreferredStorage& operator = (const SetPreferredStorage&) = delete;

 protected:
  /*  do_request - implement the request.
   *
   *  Return 0 on success, -1 on failure.
   */
  int do_request() override;

 private:
  MediaType mt_;
};

#endif  // !SET_PREFERRED_STOR_H_
