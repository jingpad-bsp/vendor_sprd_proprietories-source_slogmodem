/*
 *  cp_set_dir.cpp - directory object for one run.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-5-14 Zhang Ziyi
 *  Initial version.
 */

#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "cp_set_dir.h"
#include "cp_dir.h"
#include "log_config.h"
#include "media_stor.h"

CpSetDirectory::CpSetDirectory(MediaStorage* media, CpClass cp_class,
        	               const LogString& dir, unsigned priority)
    : m_media{media},
      m_cp_class{cp_class},
      m_path(dir),
      m_priority{priority},
      m_size{0} {}

CpSetDirectory::~CpSetDirectory() { clear_ptr_container(m_cp_dirs); }

int CpSetDirectory::stat() {
  CpType ct = CT_UNKNOWN;

  if (CLASS_MODEM == m_cp_class) {
    ct = LogConfig::get_wan_modem_type();
  } else if (CLASS_SENSOR == m_cp_class) {
    ct = CT_PM_SH;
  } else if (CLASS_AUDIO == m_cp_class) {
    ct = CT_AGDSP;
  }

  if (ct != CT_UNKNOWN) {
    CpDirectory* cp_dir = new CpDirectory(this, ct, m_path);
    if (!cp_dir->stat()) {
        m_cp_dirs.push_back(cp_dir);
        m_size += cp_dir->size();
    } else {
      delete cp_dir;
    }

    if(CLASS_MODEM == m_cp_class) {
      DIR* pd = opendir(ls2cstring(m_path));
      LogString d_path;
      struct stat file_stat;

      if (!pd) {
        err_log("fail to open directory %s", ls2cstring(m_path));
        return -1;
      }
      while(true) {
        struct dirent* dent = readdir(pd);
        if (!dent) {
          break;
        }

        if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..")) {
          continue;
        }

        d_path = m_path + "/" + dent->d_name;
        if (!::stat(ls2cstring(d_path), &file_stat) && S_ISDIR(file_stat.st_mode)) {
          info_log("CpSetDirectory::stat d_path=%s",ls2cstring(d_path));

          if (!strcmp(dent->d_name, "miniap")) {
            ct = CT_ORCA_MINIAP;
          } else if (!strcmp(dent->d_name, "dp")) {
            ct = CT_ORCA_DP;
          } else {
            continue;
          }
        }

        CpDirectory* cp_dir = new CpDirectory(this, ct, d_path);
        if (!cp_dir->stat()) {
          m_cp_dirs.push_back(cp_dir);
          m_size += cp_dir->size();
        } else {
          delete cp_dir;
        }
      }
      closedir(pd);
    }
  } else {
    DIR* pd = opendir(ls2cstring(m_path));
    if (!pd) {
      err_log("fail to open directory %s", ls2cstring(m_path));
      return -1;
    }

    LogString d_path;
    while (true) {
      struct dirent* dent = readdir(pd);
      if (!dent) {
        break;
      }
      if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..")) {
        continue;
      }
      d_path = m_path + "/" + dent->d_name;
      struct stat file_stat;

      if (!::stat(ls2cstring(d_path), &file_stat) && S_ISDIR(file_stat.st_mode)) {

        if (!strcmp(dent->d_name, "wifi_bt_fm")) {
          ct = CT_WCN;
        } else if (!strcmp(dent->d_name, "gnss")) {
          ct = CT_GNSS;
        } else if (!strcmp(dent->d_name, "modem") && (m_cp_class == CLASS_MIX)) {
          // currently only wan modem power on log is supported
          ct = LogConfig::get_wan_modem_type();
        } else {
          continue;
        }

        CpDirectory* cp_dir = new CpDirectory(this, ct, d_path);
        if (!cp_dir->stat()) {
          m_cp_dirs.push_back(cp_dir);
          m_size += cp_dir->size();
        } else {
          delete cp_dir;
        }
      }
    }

    closedir(pd);
  }
  return 0;
}

int CpSetDirectory::create() {
  info_log("create cp set directory: %s", ls2cstring(m_path));
  int ret = mkdir(ls2cstring(m_path), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (-1 == ret && EEXIST == errno) {
    ret = 0;
  }
  return ret;
}

int CpSetDirectory::remove() {
  int ret = 0;

  auto it = m_cp_dirs.begin();
  while ( it != m_cp_dirs.end()) {
    auto cpdir = *it;
    if (cpdir->remove()) {
      ret = -1;
      ++it;
    } else {
      it = m_cp_dirs.erase(it);
      dec_size(cpdir->size());
      delete cpdir;
      cpdir = nullptr;
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

LogString CpSetDirectory::type_to_cp_path(CpType ct) {
  LogString path = m_path;
  switch(ct) {
    case CT_WCN:
      path += "/wifi_bt_fm";
      break;
    case CT_GNSS:
      path += "/gnss";
      break;
    case CT_WCDMA:
    case CT_TD:
    case CT_3MODE:
    case CT_4MODE:
    case CT_5MODE:
    // case CT_WANMODEM:
      if (m_cp_class == CLASS_MIX) {
        path += "/modem";
      }
      break;
    case CT_ORCA_MINIAP:
      path += "/miniap";
      break;
    case CT_ORCA_DP:
      path += "/dp";
      break;
    default:
      break;
  }

  return path;
}

int CpSetDirectory::check_cp_quota(CpType ct, uint64_t limit,
                                   bool overwrite) {
  int ret{};
  CpDirectory* cp_dir = get_cp_dir(ct);

  if (cp_dir) {
    ret = cp_dir->check_quota(limit, overwrite);
  }

  return ret;
}

CpDirectory* CpSetDirectory::prepare_cp_dir(CpType ct,
                                            bool& new_cp_dir) {
  CpDirectory* cp_dir = get_cp_dir(ct);

  if (!cp_dir) {
    cp_dir = new CpDirectory(this, ct, type_to_cp_path(ct));
    if (cp_dir->create()) {
      err_log("fail to create %s", ls2cstring(cp_dir->path()));
      delete cp_dir;
      cp_dir = nullptr;
    } else {
      new_cp_dir = true;
      m_cp_dirs.push_back(cp_dir);
    }
  } else if (access(ls2cstring(cp_dir->path()), R_OK | W_OK | X_OK)) {
    // Remove the CpDirectory
    ll_remove(m_cp_dirs, cp_dir);
    m_size -= cp_dir->size();
    delete cp_dir;

    cp_dir = new CpDirectory(this, ct, type_to_cp_path(ct));
    if (cp_dir->create()) {
      err_log("fail to create %s", ls2cstring(cp_dir->path()));
      delete cp_dir;
      cp_dir = nullptr;
    } else {
      new_cp_dir = true;
      m_cp_dirs.push_back(cp_dir);
    }
  }

  return cp_dir;
}

void CpSetDirectory::add_size(size_t len) {
  m_size += len;
  m_media->add_size(len);
}

void CpSetDirectory::dec_size(size_t len) {
  m_size -= len;
  m_media->dec_size(len);
}

void CpSetDirectory::stop() {
  LogList<CpDirectory*>::iterator it;

  for (it = m_cp_dirs.begin(); it != m_cp_dirs.end(); ++it) {
    CpDirectory* p = *it;
    p->stop();
  }
}

void CpSetDirectory::stop(CpType ct) {
  for (auto cp_dir : m_cp_dirs) {
    if (cp_dir->ct() == ct) {
      cp_dir->stop();
    }
  }
}

std::weak_ptr<LogFile> CpSetDirectory::create_file(CpType cp_type,
                                                   const LogString& fname,
                                                   LogFile::LogType t,
                                                   int flags) {
  bool created = false;
  // Get the CP directory
  CpDirectory* cp_dir = prepare_cp_dir(cp_type, created);
  std::weak_ptr<LogFile> f;

  if (cp_dir) {
    f = cp_dir->create_file(fname, t, flags);
  }

  return f;
}

CpDirectory* CpSetDirectory::get_cp_dir(CpType ct) {
  LogList<CpDirectory*>::iterator it;
  CpDirectory* cp_dir = nullptr;

  for (it = m_cp_dirs.begin(); it != m_cp_dirs.end(); ++it) {
    CpDirectory* cp = *it;
    if (cp->ct() == ct) {
      cp_dir = cp;
      break;
    }
  }

  return cp_dir;
}

std::weak_ptr<LogFile> CpSetDirectory::recreate_log_file(CpType cp_type,
                                                    bool& new_cp_dir,
                                                    int flags) {
  std::weak_ptr<LogFile> log_file;
  CpDirectory* cp_dir = prepare_cp_dir(cp_type, new_cp_dir);

  if (cp_dir) {
    log_file = cp_dir->create_log_file(flags);
  }

  return log_file;
}
