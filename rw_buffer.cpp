/*
 *  rw_buffer.cpp - data buffer used for read/write system call.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-11-7 Yan Zhihang
 *  Initial version.
 */

#include "rw_buffer.h"

const size_t RwBuffer::MAX_RW_BUFFER_SIZE = 1024 * 256;

RwBuffer::RwBuffer()
    : buf_size_{MAX_RW_BUFFER_SIZE},
      buf_ptr_{new uint8_t[MAX_RW_BUFFER_SIZE]} {}
