/*
 * write_to_log_file.h - a collection of source stuffs and destionationto be copied
 *
 * Copyright (C) 2017 Spreadtrum Communication Inc.
 *
 * History:
 * 2017-9-18 YAN Zhihang
 * Initial version
 */

#ifndef _WRITE_TO_LOG_
#define _WRITE_TO_LOG_

#include <unistd.h>

#include "convey_unit.h"
#include "cp_dir.h"
#include "cp_log_cmn.h"
#include "cp_set_dir.h"

class WriteToLogFile : public ConveyUnit<uint8_t, LogFile> {
 public:
  WriteToLogFile(StorageManager* sm,
                 CpType ct,
                 CpClass cpclass,
                 const uint8_t* src,
                 size_t size,
                 LogFile::LogType logtype,
                 unsigned dest_priority = UINT_MAX,
                 const char* dest_file_name = nullptr,
                 LogFile* dest = nullptr);
  virtual ~WriteToLogFile();
  WriteToLogFile(const WriteToLogFile&) = delete;
  WriteToLogFile& operator=(const WriteToLogFile&) = delete;

  bool check_src() override;
  bool check_dest() override;
  bool pre_convey() override;
  void convey_method(std::function<unsigned(bool)> inspector,
                     uint8_t* const buf,
                     size_t buf_size) override;
  void post_convey() override;
  void clear_result() override;

 private:
  size_t size_;
  LogFile::LogType log_type_;
  const char* dest_file_name_;
};

#endif
