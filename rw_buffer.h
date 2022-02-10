/*
 *  rw_buffer.h - data buffer used for read/write system call.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-11-7 Yan Zhihang
 *  Initial version.
 */
#ifndef RW_BUF_H_
#define RW_BUF_H_

#include <memory>

#include "cp_log_cmn.h"

class RwBuffer {
 public:
  static const size_t MAX_RW_BUFFER_SIZE;

  RwBuffer();
  RwBuffer(const RwBuffer&) = delete;
  RwBuffer& operator=(const RwBuffer&) = delete;
  /*
   * get_buf - get the raw pointer to the allocated buffer,
   *           the poniter must not be deallocated
   *
   * Return Value:
   *  poniter of buffer
   */
  uint8_t* get_buf() const { return buf_ptr_.get(); }
  /*
   * size - get the buffer size
   *
   * Return Value:
   *  size of the buffer
   */
  size_t size() const { return buf_size_; }

 private:
  size_t buf_size_;
  std::unique_ptr<uint8_t[]> buf_ptr_;
};

#endif  //!RW_BUF_H_
