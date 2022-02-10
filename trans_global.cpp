/*
 *  trans_global.cpp - The global category transaction class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-5-26 Zhang Ziyi
 *  Initial version.
 */

#include "trans_global.h"

TransGlobal::TransGlobal(TransactionManager* mgr, Type t)
    :Transaction{mgr, GLOBAL},
     type_{t} {}
