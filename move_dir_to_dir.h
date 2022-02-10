/*
 * move_dir_to_dir.h - move a collection of source stuffs
 *
 * Copyright (C) 2017 Spreadtrum Communication Inc.
 *
 * History:
 * 2017-9-26 YAN Zhihang
 * Initial version
 *
 */

#ifndef _MOVE_DIR_TO_DIR_
#define _MOVE_DIR_TO_DIR_

#include <unistd.h>

#include "copy_dir_to_dir.h"

class CpDirectory;
class LogFile;

class MoveDirToDirUnit : public CopyDirToDirUnit {
 public:
  MoveDirToDirUnit(StorageManager* sm,
                   CpType type,
                   CpClass cpclass,
                   CpDirectory* src,
                   unsigned src_priority,
                   CpDirectory* dest = nullptr,
                   unsigned dest_priority = UINT_MAX)
      : CopyDirToDirUnit(sm, type, cpclass, src, src_priority,
                         dest, dest_priority) {}

  void post_convey() override;
};

#endif
