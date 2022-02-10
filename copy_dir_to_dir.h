/*
 * copy_dir_to_dir.h - a collection of source stuffs and destionationto be copied
 *
 * Copyright (C) 2017 Spreadtrum Communication Inc.
 *
 * History:
 * 2017-9-18 YAN Zhihang
 * Initial version
 */

#ifndef _COPY_DIR_TO_DIR_
#define _COPY_DIR_TO_DIR_

#include "convey_unit.h"
#include "cp_log_cmn.h"
#include "concurrent_queue.h"

class CpDirectory;
class LogFile;

class CopyDirToDirUnit : public ConveyUnit<CpDirectory, CpDirectory> {
 public:
  CopyDirToDirUnit(StorageManager* sm,
                   CpType type,
                   CpClass cpclass,
                   const CpDirectory* src,
                   unsigned src_priority,
                   CpDirectory* dest = nullptr,
                   unsigned dest_priority = UINT_MAX);
  virtual ~CopyDirToDirUnit();

  bool check_src() override;
  bool check_dest() override;
  bool pre_convey() override;
  void convey_method(std::function<unsigned(bool)> inspector,
                     uint8_t* const buf,
                     size_t buf_size) override;
  void post_convey() override;
  void clear_result() override;

 protected:
  ConcurrentQueue<LogFile> copied_dest_files_;
  ConcurrentQueue<LogString> src_base_names_;
};

#endif
