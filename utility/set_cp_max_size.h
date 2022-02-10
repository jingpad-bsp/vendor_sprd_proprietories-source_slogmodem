/*
 *  set_cp_max_size.h - set subsystem's max size for log saving.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-04-12 YAN Zhihang
 *  Initial version.
 */
#ifndef SET_CP_MAX_SIZE_H_
#define SET_CP_MAX_SIZE_H_

#include "cplogctl_cmn.h"
#include "slogm_req.h"

class SetCpMaxSize : public SlogmRequest {
 public:
  SetCpMaxSize(CpType ct, MediaType mt, unsigned size);

 protected:
  /*  do_request - implement the request.
   *
   *  Return 0 on success, -1 on failure.
   */
  int do_request() override;

 private:
  CpType m_type;
  MediaType mt_;
  unsigned m_size;
};

#endif  // !SET_CP_MAX_SIZE_H_
