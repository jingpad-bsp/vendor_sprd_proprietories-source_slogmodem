/*
 * copy_to_log_file.cpp - a collection of source stuffs and destionationto be copied
 *
 * Copyright (C) 2017 Spreadtrum Communication Inc.
 *
 * History:
 * 2017-9-18 YAN Zhihang
 * Initial version
 *
 */
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "copy_to_log_file.h"
#include "cp_dir.h"
#include "cp_set_dir.h"
#include "media_stor.h"
#include "stor_mgr.h"

CopyToLogFile::CopyToLogFile(StorageManager* sm,
                             CpType ct,
                             CpClass cpclass,
                             const char* src,
                             LogFile::LogType logtype,
                             size_t size,
                             int src_fd,
                             const char* dest_file_name,
                             LogFile* dest,
                             unsigned dest_priority,
                             off_t src_offset)
    : ConveyUnit<char, LogFile>(sm, ct, cpclass, src, UINT_MAX,
                                dest, dest_priority),
      src_offset_{src_offset},
      src_fd_{src_fd},
      log_type_{logtype},
      size_{size},
      dest_file_name_{dest_file_name} {
  if (src_fd < 0) {
    close_src_fd_ = true;
  } else {
    close_src_fd_ = false;
  }
}

CopyToLogFile::~CopyToLogFile() {
  if (dest_) {
    dest_->close();
    LogString path;
    if (dest_->dir()) {
      path = dest_->dir()->path();
      dest_->remove(path);
    }
    delete dest_;
    dest_ = nullptr;
  }

  if (close_src_fd_ && src_fd_ >= 0) {
    ::close(src_fd_);
    src_fd_ = -1;
  }
}

bool CopyToLogFile::check_src() {
  if (-1 == src_fd_) {
    src_fd_ = ::open(src_, O_RDONLY);
    if (src_fd_ < 0) {
      err_log("fail to open %s", src_);
      return false;
    }
  }

  if (src_offset_) {
    if (-1 == lseek(src_fd_, src_offset_, SEEK_SET)) {
      if (close_src_fd_) {
        ::close(src_fd_);
        src_fd_ = -1;
      }
      err_log("lseek fail");
      return false;
    }
  }

  return true;
}

bool CopyToLogFile::check_dest() {
  return nullptr != dest_ &&
         0 == access(ls2cstring(dest_->dir()->path() + "/" +
                                dest_->base_name()),
                     W_OK);
}

bool CopyToLogFile::pre_convey() {
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
      CpDirectory* cd = cpset->prepare_cp_dir(type_, nd);
      if (cd && dest_file_name_) {
        dest_ = new LogFile(dest_file_name_, cd, log_type_);
        if (!dest_->exists()) {
          if (dest_->create()) {
            err_log("fail to create file %s", dest_file_name_);
            delete dest_;
            dest_ = nullptr;
          } else {
            dest_priority_ = cpset->priority();
          }
        } else {
          err_log("%s already exists", dest_file_name_);
          delete dest_;
          dest_ = nullptr;
        }
      }
    }
  }

  if (dest_) {
    return true;
  } else {
    info_log("prepare dest fail");
    return false;
  }
}

void CopyToLogFile::post_convey() {
  // add log file to destination directory control
  if (0 == dest_->get_size()) {
    dest_->dir()->add_log_file(dest_);
    dest_ = nullptr;
  } else {
    dest_->remove(dest_->dir()->path());
    delete dest_;
    dest_ = nullptr;
  }

  if (close_src_fd_ && src_fd_ >= 0) {
    ::close(src_fd_);
    src_fd_ = -1;
  }
}

void CopyToLogFile::clear_result() {
  dest_->remove(dest_->dir()->path());
  delete dest_;
  dest_ = nullptr;
}

void CopyToLogFile::convey_method(std::function<unsigned(bool)> inspector,
                                  uint8_t* const buf,
                                  size_t buf_size) {
  size_t cum = 0;
  size_t size_written = 0;
  bool evt_received = false;

  while (cum < size_) {
    size_t io_len = size_ - cum;
    if (io_len > buf_size) {
      io_len = buf_size;
    }

    ssize_t n = read(src_fd_, buf, io_len);
    if (n <= 0) {
      break;
    }

    cum += n;

    // thread unsafe file write
    ssize_t nwr = dest_->write_raw(buf, n);
    if (nwr != n) {
      break;
    }

    size_written += nwr;

    // inspect event
    if (!inspect_event_check(inspector(false))) {
      evt_received = true;
      break;
    }
  }

  if (!evt_received) {
    if (size_written == size_) {
      info_log("copy %s to %s is finished", src_,
               ls2cstring(dest_->dir()->path() + "/" + dest_->base_name()));
      unit_done(Done);
    } else {
      info_log("copy %s to %s is failed", src_,
               ls2cstring(dest_->dir()->path() + "/" + dest_->base_name()));
      unit_done(Failed);
    }
  }

  dest_->close();
}
