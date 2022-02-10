/*
 *  cp_dir.cpp - directory object for one CP in one run.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-5-14 Zhang Ziyi
 *  Initial version.
 */
#include <algorithm>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

#include "cp_dir.h"
#include "cp_set_dir.h"
#include "def_config.h"
#include "log_file.h"
#include "media_stor.h"
#include "stor_mgr.h"

CpDirectory::CpDirectory(CpSetDirectory* par_dir, CpType ct,
                         const LogString& dir)
    : m_cp_set_dir{par_dir},
      m_ct{ct},
      m_path(dir),
      m_size{},
      m_log_watch{},
      m_store_full{} {}

CpDirectory::~CpDirectory() {
  // TODO: cancel file watch
  // cancel_watch();
}

int CpDirectory::stat() {
  DIR* pd = opendir(ls2cstring(m_path));

  if (!pd) {
    err_log("fail to open %s", ls2cstring(m_path));
    return -1;
  }

  struct dirent* dent;

  while (true) {
    dent = readdir(pd);
    if (!dent) {
      break;
    }
    if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..")) {
      continue;
    }
    LogString file_path = m_path + "/" + dent->d_name;
    struct stat file_stat;
    if (!::stat(ls2cstring(file_path), &file_stat) &&
        S_ISREG(file_stat.st_mode)) {
      auto log =
          std::make_shared<LogFile>(LogString(dent->d_name), this,
                                    LogFile::LT_UNKNOWN, file_stat.st_size);
      log->get_type();
      insert_ascending(m_log_files, std::move(log));
      m_size += file_stat.st_size;
    }
  }

  closedir(pd);

  return 0;
}

void CpDirectory::insert_ascending(LogList<std::shared_ptr<LogFile>>& lst,
                                   std::shared_ptr<LogFile>&& f) {
  LogList<std::shared_ptr<LogFile>>::iterator it;
  for (it = lst.begin(); it != lst.end(); ++it) {
    std::shared_ptr<LogFile>& p = *it;
    if (*f <= *p) {
      break;
    }
  }
  lst.insert(it, f);
}

void CpDirectory::add_log_file(LogFile* lf) {
  add_size(lf->size());
  insert_ascending(m_log_files, std::shared_ptr<LogFile>(lf));
}

int CpDirectory::create() {
  info_log("create cp directory: %s", ls2cstring(m_path));
  int ret = mkdir(ls2cstring(m_path), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (-1 == ret && EEXIST == errno) {
    ret = 0;
  }
  return ret;
}

const char* CpDirectory::type_to_cp_name(CpType ct) {
  const char* name = nullptr;
  switch (ct) {
    case CT_WCDMA:
    case CT_TD:
    case CT_3MODE:
    case CT_4MODE:
    case CT_5MODE:
      //  case CT_WANMODEM:
      name = "md";
      break;
    case CT_WCN:
      name = "wcn";
      break;
    case CT_GNSS:
      name = "gps";
      break;
    case CT_AGDSP:
      name = "ag";
      break;
    case CT_PM_SH:
      name = "sp";
      break;
    case CT_ORCA_MINIAP:
      name = "orca_ap";
      break;
    case CT_ORCA_DP:
      name = "orca_dp";
      break;
    default:
      break;
  }

  return name;
}

std::weak_ptr<LogFile> CpDirectory::create_log_file(int flags) {
  if (CLASS_MIX == m_cp_set_dir->cp_class()) {
    if (false == trim_log_file_num(MIX_FILE_MAX_NUM)) {
      err_log("fail to limit the number of log files to %d",
              MIX_FILE_MAX_NUM);
      return std::weak_ptr<LogFile>();
    }
  }

  time_t t = time(0);
  struct tm lt;

  if (static_cast<time_t>(-1) == t || !localtime_r(&t, &lt)) {
    return std::weak_ptr<LogFile>();
  }

  const char* name = type_to_cp_name(m_ct);
  if (!name) {
    return std::weak_ptr<LogFile>();
  }

  char s[64];
  snprintf(s, sizeof s, "%s_%04d%02d%02d-%02d%02d%02d.log",
           name, lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
           lt.tm_hour, lt.tm_min, lt.tm_sec);

  std::shared_ptr<LogFile> log_file(new LogFile(LogString(s), this, lt));
  if (0 == log_file->create(flags)) {
    m_log_files.push_back(log_file);
    m_cur_log = log_file;
    // TODO: Watch the current log
    // FileWatcher* fw = cp_set_dir()->get_media()->file_watcher();
    // fw->add(this, log_delete_notify, s, m_log_watch);
  }

  return log_file;
}

int CpDirectory::close_log_file() {
  if (auto current_log = m_cur_log.lock()) {
    try {
      current_log->close();
    } catch(...) {
      err_log("current_log->close");
    }
    m_cur_log.reset();
  }
  // TODO: Cancel file watch
  // cancel_watch();
  return 0;
}

void CpDirectory::add_size(size_t len) {
  m_size += len;
  m_cp_set_dir->add_size(len);
}

void CpDirectory::dec_size(size_t len) {
  m_size -= len;
  m_cp_set_dir->dec_size(len);
}

void CpDirectory::stop() {
  if (auto current_log = m_cur_log.lock()) {
    current_log->close();
    m_cur_log.reset();
  }
}

int CpDirectory::remove() {
  int ret = 0;
  // close current log file if any
  close_log_file();

  // delete all log files
  auto it = m_log_files.begin();
  while (it != m_log_files.end()) {
    auto& lf = *it;
    if (lf->remove(m_path)) {
      ret = -1;
      err_log("delete %s error", ls2cstring(lf->base_name()));
      ++it;
    } else {
      dec_size(lf->size());
      it = m_log_files.erase(it);
    }
  }

  if (0 == ret) {
    ret = rmdir(ls2cstring(m_path));
    if (-1 == ret) {
      if (ENOENT == errno) {
        ret = 0;
      } else {
        err_log("fail to delete %s", ls2cstring(m_path));
      }
    }
  }

  return ret;
}

bool CpDirectory::trim_log_file_num(unsigned num) {
  bool ret = true;

  auto it = m_log_files.end();

  while (num && it != m_log_files.begin()) {
    --it;
    auto& lf = *it;
    if (LogFile::LT_LOG == lf->type()) {
      --num;
    }
  }

  // if num of newest log files found,
  // check if older log file exist, if true, delete it.
  for (auto id = m_log_files.begin(); id != it; ) {
    auto& lf = *id;
    if (lf->overwritable()) {
      if (lf->remove(m_path)) {
        ret = false;
        err_log("delete %s error", ls2cstring(lf->base_name()));
        ++id;
      } else {
        m_size -= lf->size();
        id = m_log_files.erase(id);
      }
    } else {
      ++id;
    }
  }

  return ret;
}

int CpDirectory::check_quota(uint64_t max_size, bool overwrite) {
  if (!max_size || max_size > m_size) {
    return 0;
  }

  if (!overwrite) {
    return -1;
  }

  if (!m_store_full) {
    m_store_full = true;
  }

  if (trim(m_size - max_size)) {
    m_store_full = false;
    return 0;
  } else {
    return -1;
  }
}

void CpDirectory::rm_oldest_log_file() {
  auto it = m_log_files.begin();
  while (it != m_log_files.end()) {
    auto& f = *it;
    if (!f->overwritable()) {
      ++it;
      continue;
    }

    if (f->remove(m_path)) {
      err_log("delete %s error", ls2cstring(f->base_name()));
      ++it;
    } else {
      info_log("delete %s ok", ls2cstring(f->base_name()));
      m_size -= f->size();
      it = m_log_files.erase(it);
      break;
    }
  }
}

int CpDirectory::remove(std::shared_ptr<LogFile>& rmf) {
  int ret = -1;
  auto it = m_log_files.begin();

  while (it != m_log_files.end()) {
    auto& f = *it;
    if (rmf == f) {
      f->remove(m_path);
      info_log("remove log file %s",ls2cstring(m_path));
      dec_size(f->size());
      it = m_log_files.erase(it);
      ret = 0;
      break;
    } else {
      ++it;
    }
  }

  return ret;
}

int CpDirectory::remove(const LogString& base_name) {
  auto lf_it = std::find_if(m_log_files.begin(), m_log_files.end(),
                            [&base_name] (const std::shared_ptr<LogFile>& lf) {
                              return base_name == lf->base_name();
                            });

  if (lf_it == m_log_files.end()) {
    err_log("no log file named %s found in %s",
            ls2cstring(base_name), ls2cstring(m_path));
    return -1;
  }

  auto& f = *lf_it;
  f->remove(m_path);
  info_log("remove log file %s",ls2cstring(m_path));
  dec_size(f->size());
  m_log_files.erase(lf_it);

  return 0;
}

uint64_t CpDirectory::trim(uint64_t sz) {
  auto it = m_log_files.begin();
  uint64_t total_dec = 0;

  while (it != m_log_files.end()) {
    auto& f = *it;
    if (!f->overwritable()) {
      ++it;
      continue;
    }

    if (f->remove(m_path)) {
      err_log("delete %s error", ls2cstring(f->base_name()));
      break;
    }

    uint64_t dec_size = static_cast<uint64_t>(f->size());
    it = m_log_files.erase(it);

    total_dec += dec_size;
    if (dec_size >= sz) {
      break;
    }
    sz -= dec_size;
  }

  m_size -= total_dec;

  return total_dec;
}

std::weak_ptr<LogFile> CpDirectory::create_file(const LogString& fname,
                                                LogFile::LogType t,
                                                int flags) {
  std::shared_ptr<LogFile> log_file{new LogFile(fname, this, t)};

  if (!log_file->create(flags)) {
    m_log_files.push_back(log_file);
  } else {
    log_file.reset();
  }

  return log_file;
}

void CpDirectory::cancel_watch() {
  if (m_log_watch) {
    FileWatcher* fw = cp_set_dir()->get_media()->file_watcher();
    if (fw) {
      fw->del(m_log_watch);
    }
    m_log_watch = 0;
  }
}

void CpDirectory::log_delete_notify(void* client, uint32_t evt) {
  CpDirectory* cp_dir = static_cast<CpDirectory*>(client);

  if ((evt & IN_DELETE_SELF) && cp_dir->m_log_watch) {
    // TODO: inform the StorageManager
    // cp_dir->cp_set_dir()->get_media()->stor_mgr()->
    cp_dir->close_log_file();
  }
}

void CpDirectory::file_removed(LogFile* f) {
  bool found = false;

  for (auto it = m_log_files.begin(); it != m_log_files.end(); ++it) {
    auto& pf = *it;
    if (f == pf.get()) {
      m_log_files.erase(it);
      found = true;
      break;
    }
  }

  if (found) {
    m_size -= f->size();
  } else {
    err_log("log %s does not exist in dir %s", ls2cstring(f->base_name()),
            ls2cstring(m_path));
  }
}
