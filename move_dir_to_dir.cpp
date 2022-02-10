/*
 * move_dir_to_dir.cpp - move a collection of source stuffs
 *
 * Copyright (C) 2017 Spreadtrum Communication Inc.
 *
 * History:
 * 2017-9-26 YAN Zhihang
 * Initial version
 *
 */
#include "cp_dir.h"
#include "log_file.h"
#include "move_dir_to_dir.h"

void MoveDirToDirUnit::post_convey() {
  while (1) {
    auto lf = copied_dest_files_.get_next(false);

    if (nullptr == lf) {
      break;
    }

    if (!lf->exists()) {
      continue;
    }

    if (0 == lf->get_size()) {
      const_cast<CpDirectory*>(src_)->remove(lf->base_name());
      lf->get_type();
      lf->dir()->add_log_file(lf.release());
    } else {
      lf->remove(lf->dir()->path());
    }
  }
}
