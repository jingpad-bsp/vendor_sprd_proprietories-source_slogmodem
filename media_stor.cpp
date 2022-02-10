/*
 *  media_stor.cpp - storage manager for one media.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-5-14 Zhang Ziyi
 *  Initial version.
 */

#include <climits>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

#include "cp_dir.h"
#include "cp_set_dir.h"
#include "def_config.h"
#include "file_watcher.h"
#include "media_stor.h"
#include "stor_mgr.h"

MediaStorage::MediaStorage(StorageManager* stor_mgr)
    : m_stor_mgr{stor_mgr},
      m_size{0},
      m_file_watcher{0},
      m_priority{UINT_MAX} {}

MediaStorage::~MediaStorage() { clear_ptr_container(m_log_dirs); }

void MediaStorage::media_init(const LogString& log_path,
                              unsigned priority) {
  m_log_dir = log_path;
  m_priority = priority;
}

int MediaStorage::sync_media(LogString& log_path, unsigned priority,
                             FileWatcher* fw) {
  m_file_watcher = fw;

  int err = mkdir(ls2cstring(log_path),
                  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  if (-1 == err && EEXIST != errno) {
    err_log("create log dir %s error", ls2cstring(log_path));
    return -1;
  }

  DIR* pd = opendir(ls2cstring(log_path));
  if (!pd) {
    err_log("opendir %s error", ls2cstring(log_path));
    return -1;
  }

  while (true) {
    struct dirent* dent = readdir(pd);
    if (!dent) {
      break;
    }
    if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..")) {
      continue;
    }
    struct stat file_stat;
    LogString path = log_path + "/" + dent->d_name;
    if (!stat(ls2cstring(path), &file_stat) && S_ISDIR(file_stat.st_mode)) {
      CpClass cp_class;
      if (!strcmp(dent->d_name, "modem")) {
        cp_class = CLASS_MODEM;
      } else if (!strcmp(dent->d_name, "connectivity")) {
        cp_class = CLASS_CONNECTIVITY;
      } else if (!strcmp(dent->d_name, "sensorhub")) {
        cp_class = CLASS_SENSOR;
      } else if (!strcmp(dent->d_name, "audio")) {
        cp_class = CLASS_AUDIO;
      } else if (!strcmp(dent->d_name, "poweron")) {
        cp_class = CLASS_MIX;
      } else {
        continue;
      }

      CpSetDirectory* cp_set = new CpSetDirectory(this, cp_class,
                                                  path, priority);
      if (!cp_set->stat()) {
        m_log_dirs.push_back(cp_set);
        m_size += cp_set->size();
      } else {
        delete cp_set;
      }
    }
  }

  closedir(pd);

  return 0;
}

void MediaStorage::stor_media_vanished(unsigned mp) {
  LogList<CpSetDirectory*>::iterator it;

  for (it = m_log_dirs.begin(); it != m_log_dirs.end();) {
    CpSetDirectory* cp_set = *it;
    if (cp_set->priority() == mp) {
      m_size -= cp_set->size();
      it = m_log_dirs.erase(it);
      delete cp_set;
    } else {
      ++it;
    }
  }
}

LogString MediaStorage::class_to_path(CpClass cp_class) {
  const char* subdir = nullptr;
  switch (cp_class) {
    case CLASS_MIX:
      subdir = "poweron";
      break;
    case CLASS_CONNECTIVITY:
      subdir = "connectivity";
      break;
    case CLASS_AUDIO:
      subdir = "audio";
      break;
    case CLASS_MODEM:
      subdir = "modem";
      break;
    case CLASS_SENSOR:
      subdir = "sensorhub";
      break;
    default:
      err_log("Not supported cp class");
      break;
  }

  LogString path;
  if (subdir) {
    path = m_log_dir + "/" + subdir;
  }

  return path;
}

CpSetDirectory* MediaStorage::create_cp_set(CpClass cp_class) {

  CpSetDirectory* cp_set = new CpSetDirectory(this, cp_class,
                                              class_to_path(cp_class),
                                              m_priority);
  if (cp_set->create()) {
    err_log("create CP set %s failed", ls2cstring(cp_set->path()));
    delete cp_set;
    cp_set = nullptr;
  } else {
    m_log_dirs.push_back(cp_set);
  }

  return cp_set;
}

void MediaStorage::add_size(size_t len) { m_size += len; }

void MediaStorage::dec_size(size_t len) { m_size -= len; }

void MediaStorage::stop() {
  for (auto cp_set : m_log_dirs) {
    cp_set->stop();
  }
}

void MediaStorage::stop(CpClass cp_class, CpType ct) {
  for (auto cp_set : m_log_dirs) {
    if (cp_set->cp_class() == cp_class) {
      cp_set->stop(ct);
    }
  }
}

bool MediaStorage::is_media_full() {
  struct statvfs vstat;
  bool is_full{};

  if (!statvfs(ls2cstring(m_log_dir), &vstat)) {
    if (vstat.f_bsize / vstat.f_frsize * vstat.f_bfree
            <= vstat.f_blocks / 10) {
      is_full = true;
    }
  }

  return is_full;
}

int MediaStorage::check_cp_quota(CpType ct, CpClass cp_class,
                                 uint64_t limit,
                                 uint64_t max_file,
                                 bool overwrite) {
  // TODO: poweron dir shall be considered.
  if (CLASS_MIX == cp_class) {
    return 0;
  }

  uint64_t log_size{0};
  for (auto cp_set : m_log_dirs) {
    if (cp_set->cp_class() == cp_class) {
      CpDirectory* cp_dir = cp_set->get_cp_dir(ct);
      if (!cp_dir) {
        continue;
      }
      log_size += cp_dir->size();
    }
  }

  int ret{};
  uint64_t trim_size{};
  bool is_full = is_media_full();

  if (overwrite) {
    // Check used capacity of the media
    if (is_full) {
      trim_size = max_file;
      info_log("media full: trim %lu",
               static_cast<unsigned long>(trim_size));
    } else if (limit && log_size > limit) {
      trim_size = log_size - limit;
    }
  } else {  // No overwrite
    if (is_full) {
      ret = -1;
    } else if (limit && log_size >= limit) {  // No quota
      ret = -1;
    }
  }

  // Need to make room for new log.
  uint64_t n{0};
  if (trim_size) {
    for (auto cp_set : m_log_dirs) {
      if (cp_set->cp_class() == cp_class) {
        CpDirectory* cp_dir = cp_set->get_cp_dir(ct);
        if (!cp_dir) {
          continue;
        }
        n += cp_dir->trim(trim_size);
        if (n >= trim_size) {
          break;
        }
      }
    }

    if (n < trim_size) {
      ret = -1;
    }
  }

  return ret;
}

CpSetDirectory* MediaStorage::get_cp_set(CpClass cp_class,
                                         unsigned priority) {
  if (UINT_MAX == priority) {
    priority = m_priority;
  }

  CpSetDirectory* cpset = nullptr;
  for (auto cp_set : m_log_dirs) {
    if (cp_set->cp_class() == cp_class &&
        cp_set->priority() == priority) {
      cpset = cp_set;
      break;
    }
  }

  return cpset;
}

std::weak_ptr<LogFile> MediaStorage::create_file(CpType ct,
                                                 CpClass cp_class,
                                                 LogFile::LogType t,
                                                 bool& new_cp_dir,
                                                 int flags,
                                                 const LogString& fname) {
  std::weak_ptr<LogFile> f;
  CpSetDirectory* cp_set = prepare_cp_set(cp_class);

  if (cp_set) {
    if (LogFile::LT_LOG == t) {
      f = cp_set->recreate_log_file(ct, new_cp_dir, flags);
    } else {
      f = cp_set->create_file(ct, fname, t, flags);
    }
  }

  return f;
}

void MediaStorage::clear() {
  for (auto csd : m_log_dirs) {
    csd->remove();
  }
  info_log("csd->remove()");

  clear_ptr_container(m_log_dirs);
  m_size = 0;
}

CpSetDirectory* MediaStorage::prepare_cp_set(CpClass cp_class) {
  // Check the top directory
  if (access(ls2cstring(m_log_dir), R_OK | W_OK | X_OK)) {
    // The modem_log does not exist
    m_stor_mgr->proc_working_dir_removed(this);
    clear_ptr_container(m_log_dirs);
    m_size = 0;
    if (mkdir(ls2cstring(m_log_dir), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
      err_log("create top dir (%s) error", ls2cstring(m_log_dir));
      return nullptr;
    }
  }

  CpSetDirectory* cp_set = get_cp_set(cp_class);

  // Check the working directory
  if (!cp_set) {
    cp_set = create_cp_set(cp_class);
  } else if (access(ls2cstring(cp_set->path()), R_OK | W_OK | X_OK)) {
    ll_remove(m_log_dirs, cp_set);
    m_size -= cp_set->size();
    delete cp_set;

    cp_set = create_cp_set(cp_class);
  }

  return cp_set;
}
