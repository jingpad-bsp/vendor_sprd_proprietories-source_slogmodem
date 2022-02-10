/*
 * write_to_log_file.cpp - a collection of source stuffs and destionationto be copied
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

#include "write_to_log_file.h"
#include "rw_buffer.h"
#include "stor_mgr.h"

WriteToLogFile::WriteToLogFile(StorageManager* sm,
                               CpType ct,
                               CpClass cpclass,
                               const uint8_t* src,
                               size_t size,
                               LogFile::LogType logtype,
                               unsigned dest_priority,
                               const char* dest_file_name,
                               LogFile* dest)
    : ConveyUnit<uint8_t, LogFile>(sm, ct, cpclass, src,
                                   UINT_MAX, dest, dest_priority),
      size_{size},
      log_type_{logtype},
      dest_file_name_{dest_file_name} {}

WriteToLogFile::~WriteToLogFile() {
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
}

bool WriteToLogFile::check_src() {
  return src_ && size_ && size_ < RwBuffer::MAX_RW_BUFFER_SIZE;
}

bool WriteToLogFile::check_dest() {
  return nullptr != dest_ &&
         0 == access(ls2cstring(dest_->dir()->path() + "/" + dest_->base_name()),
                     R_OK | W_OK);
}

bool WriteToLogFile::pre_convey() {
  MediaStorage* ms = sm_->get_media_stor();
  if (nullptr == ms) {
    if (!sm_->check_media_change()) {  // Can not find a media to use
      err_log("No media storage available for writing log file");
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
    return false;
  }
}

void WriteToLogFile::post_convey() {
  if (0 == dest_->get_size()) {
    // add the log file to the directory's control
    dest_->dir()->add_log_file(dest_);
    dest_ = nullptr;
  } else {
    // remove the resulted file if no size info available
    LogString path;
    if (dest_->dir()) {
      path = dest_->dir()->path();
      dest_->remove(path);
    }
    delete dest_;
    dest_ = nullptr;
  }
}

void WriteToLogFile::clear_result() {
  if (dest_) {
    LogString path;
    if (dest_->dir()) {
      path = dest_->dir()->path();
      dest_->remove(path);
    }
    delete dest_;
    dest_ = nullptr;
  }
}

void WriteToLogFile::convey_method(std::function<unsigned(bool)> inspector,
                                   uint8_t* const buf,
                                   size_t buf_size) {
  if (inspect_event_check(inspector(false))) {
    if (static_cast<size_t>(dest_->write_raw(src_, size_)) != size_) {
      unit_done(Failed);
      err_log("fail to write to %s",
              ls2cstring(dest_->dir()->path() + "/" + dest_->base_name()));
    } else {
      unit_done(Done);
      info_log("write file %s finished",
               ls2cstring(dest_->dir()->path() + "/" + dest_->base_name()));
    }
  }

  dest_->close();
}
