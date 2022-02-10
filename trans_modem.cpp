/*
 *  trans_modem.cpp - The MODEM category transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-5-26 Zhang Ziyi
 *  Initial version.
 */

#include "trans_modem.h"

TransModem::TransModem(TransactionManager* mgr, Type t)
    :Transaction{mgr, MODEM},
     type_{t},
     use_diag_{START_EVENT_LOG != t && MODEM_TICK_QUERY != t} {}
