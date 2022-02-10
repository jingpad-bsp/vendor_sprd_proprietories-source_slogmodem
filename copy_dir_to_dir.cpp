/*
 * copy_dir_to_dir.cpp - a collection of source stuffs and destionationto be copied
 *
 * Copyright (C) 2017 Spreadtrum Communication Inc.
 *
 * History:
 * 2017-9-18 YAN Zhihang
 * Initial version
 *
 */
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "copy_dir_to_dir.h"
#include "cp_dir.h"
#include "cp_set_dir.h"
#include "log_file.h"
#include "media_stor.h"
#include "stor_mgr.h"

CopyDirToDirUnit::CopyDirToDirUnit(StorageManager* sm,
                                   CpType type,
                                   CpClass cpclass,
                                   const CpDirectory* src,
                                   unsigned src_priority,
                                   CpDirectory* dest,
                                   unsigned dest_priority)
    : ConveyUnit<CpDirectory, CpDirectory>(sm, type, cpclass, src, src_priority,
                                           dest, dest_priority) {}

CopyDirToDirUnit::~CopyDirToDirUnit() {
  while (1) {
    auto lf = copied_dest_files_.get_next(false);
    if (nullptr == lf) {
      break;
    }

    if (!lf->exists()) {
      continue;
    }

    lf->remove(lf->dir()->path());
  }
}

bool CopyDirToDirUnit::check_src() {
  bool ret = true;

  if (access(ls2cstring(src_->path()), R_OK)) {
    ret = false;
    err_log("not possible to access src %s", ls2cstring(src_->path()));
  } else if (0 == src_->size()) {
    ret = false;
    err_log("empty directory %s",
            ls2cstring(src_->path()));
  } else {
    src_base_names_.clear();

    for (auto logfile : src_->log_files()) {
      info_log("%s to be conveyed",
               ls2cstring(logfile->dir()->path() +
                          "/" + logfile->base_name()));
      std::unique_ptr<LogString> aname{new LogString(logfile->base_name())};
      src_base_names_.push(std::move(aname));
    }
  }

  return ret;
}

bool CopyDirToDirUnit::check_dest() {
  return nullptr != dest_ &&
         0 == access(ls2cstring(dest_->path()), R_OK | W_OK);
}

bool CopyDirToDirUnit::pre_convey() {
  MediaStorage* ms = sm_->get_media_stor();
  if (nullptr == ms) {
    if (!sm_->check_media_change()) {  // Can not find a media to use
      err_log("No media storage available for copy directory");
    } else {
      ms = sm_->get_media_stor();
    }
  }

  if (nullptr != ms) {
    CpSetDirectory* cpset = ms->prepare_cp_set(cpclass_);
    if (cpset) {
      bool nd = false;
      dest_ = cpset->prepare_cp_dir(type_, nd);
    }
  }

  if (dest_) {
    dest_priority_ = dest_->cp_set_dir()->priority();
    info_log("dest = %s, prio = %d", dest_->cp_set_dir()->path().c_str(),
             dest_priority_);
    return true;
  } else {
    info_log("prepare dest fail");
    return false;
  }
}

void CopyDirToDirUnit::post_convey() {
  while (1) {
    auto lf = copied_dest_files_.get_next(false);

    if (nullptr == lf) {
      break;
    }

    if (!lf->exists()) {
      continue;
    }

    if (0 == lf->get_size()) {
      lf->get_type();
      lf->dir()->add_log_file(lf.release());
    } else {
      lf->remove(lf->dir()->path());
    }
  }
}

void CopyDirToDirUnit::clear_result() {
  while (1) {
    auto lf = copied_dest_files_.get_next(false);

    if (nullptr == lf) {
      break;
    }

    lf->remove(lf->dir()->path());
  }
}

void CopyDirToDirUnit::convey_method(std::function<unsigned(bool)> inspector,
                                     uint8_t* const buf,
                                     size_t buf_size) {
  bool no_evt = true;
  unsigned src_count = 0;
  unsigned dest_count = 0;

  while (1) {
    // get the source file name
    auto base_name = src_base_names_.get_next(false);
    if (nullptr == base_name) {
      break;
    }

    ++src_count;

    // prepare the destination file
    std::unique_ptr<LogFile> lf{new LogFile(*base_name, dest_)};

    if (lf->exists()) {
      ++dest_count;
      err_log("%s already exists. skip it.",
              ls2cstring(lf->dir()->path() + "/" + lf->base_name()));
      continue;
    }

    if (lf->create()) {
      err_log("fail to create file: %s", ls2cstring(lf->base_name()));
      continue;
    }

    LogString src_file_path = src_->path() + "/" + *base_name;
    info_log("src file: %s", ls2cstring(src_file_path));
    int fd_src = ::open(ls2cstring(src_file_path), O_RDONLY);
    if (fd_src < 0) {
      lf->remove(lf->dir()->path());
      err_log("can not open source file %s", ls2cstring(src_file_path));
      continue;
    }

    while (no_evt) {
      ssize_t n = read(fd_src, buf, buf_size);

      if (n < 0) {
        err_log("fail to read %s", ls2cstring(lf->base_name()));
        break;
      }

      if (!n) {  // End of file
        ++dest_count;
        info_log("copy %s to %s is finished",
                 ls2cstring(src_file_path), ls2cstring(lf->dir()->path()));
        break;
      }

      // thread unsafe file write
      ssize_t nwr = lf->write_raw(buf, n);
      if (nwr != n) {
        break;
      }

      // inspect event
      no_evt = inspect_event_check(inspector(false));
    }

    ::close(fd_src);
    lf->close();

    copied_dest_files_.push(std::move(lf));

    if (!no_evt) {
      break;
    }
  }

  if (no_evt) {
    if (src_count == dest_count) {
      unit_done(Done);
    } else {
      unit_done(Failed);
    }
  }
  // clear the src base names
  src_base_names_.clear();
}
