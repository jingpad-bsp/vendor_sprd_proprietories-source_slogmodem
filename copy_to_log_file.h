/*
 * copy_to_log_file.h - a collection of source stuffs and destionationto be copied
 *
 * Copyright (C) 2017 Spreadtrum Communication Inc.
 *
 * History:
 * 2017-9-18 YAN Zhihang
 * Initial version
 */

#ifndef _COPY_TO_LOG_
#define _COPY_TO_LOG_

#include <unistd.h>

#include "convey_unit.h"
#include "cp_log_cmn.h"
#include "log_file.h"

class CopyToLogFile : public ConveyUnit<char, LogFile> {
 public:
  CopyToLogFile(StorageManager* sm,
                CpType ct,
                CpClass cpclass,
                const char* src,
                LogFile::LogType logtype,
                size_t size,
                int src_fd = -1,
                const char* dest_file_name = nullptr,
                LogFile* dest = nullptr,
                unsigned dest_priority = UINT_MAX,
                off_t src_offset = 0);
  virtual ~CopyToLogFile();
  CopyToLogFile(const CopyToLogFile&) = delete;
  CopyToLogFile& operator=(const CopyToLogFile&) = delete;

  bool check_src() override;
  bool check_dest() override;
  bool pre_convey() override;
  void convey_method(std::function<unsigned(bool)> inspector,
                     uint8_t* const buf,
                     size_t buf_size) override;
  void post_convey() override;
  void clear_result() override;

 private:
  off_t src_offset_;
  int src_fd_;
  bool close_src_fd_;
  LogFile::LogType log_type_;
  size_t size_;
  const char* dest_file_name_;
};

#endif //!_COPY_TO_LOG_
