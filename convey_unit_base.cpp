/*
 * convey_unit_base.cpp - workshop's standard base unit
 *
 * Conveyright (C) 2017 Spreadtrum Communication Inc.
 *
 * History:
 * 2017-9-18 YAN Zhihang
 * Initial version
 *
 */

#include "convey_unit_base.h"

ConveyUnitBase::ConveyUnitBase(StorageManager* sm,
                               CpType type,
                               CpClass cpclass,
                               unsigned src_priority,
                               unsigned dest_priority)
    : state_{Wait},
      sm_{sm},
      type_{type},
      cpclass_{cpclass},
      src_priority_{src_priority},
      dest_priority_{dest_priority},
      tag_{UINT8_MAX},
      auto_dest_{UINT_MAX == dest_priority} {}
