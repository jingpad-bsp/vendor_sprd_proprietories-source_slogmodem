/*
 *  trans_diag.cpp - The MODEM diagnosis device transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-5-31 Zhang Ziyi
 *  Initial version.
 */

#include "trans_diag.h"

TransDiagDevice::TransDiagDevice(TransactionManager* tmgr, Type t)
    :TransModem{tmgr, t},
     diag_hdl_{nullptr} {}
