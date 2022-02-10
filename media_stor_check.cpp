/*
 *  media_stor_check.h - check the availability of the specific storage
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-2-20 Yan Zhihang
 *  Initial version.
 */

#include <climits>
#include <fcntl.h>
#include <unistd.h>

#include "def_config.h"
#include "media_stor_check.h"
#include "parse_utils.h"

MediaStorCheck::MediaStorCheck(StorageManager* stor_mgr,
                               MediaStorage* ms,
                               StorageManager::MediaType mt,
                               StorageManager::MediaPrior mp,
                               bool active,
                               const LogString& path)
    : permit_{active},
      stor_mgr_{stor_mgr},
      ms_{ms},
      mt_{mt},
      mp_{mp},
      in_use_{},
      path_{path},
      mount_state_{MMS_NOT_MOUNTED},
      sync_{},
      m_major_num{UINT_MAX},
      m_minor_num{UINT_MAX} {
  if (StorageManager::MT_EXT_STOR == mt) {
    UeventMonitor* um = stor_mgr->uevt_monitor();
    if (um) {
      um->subscribe_event_notifier(this,
                                   UeventMonitor::SST_BLOCK,
                                   uevent_callback);
    }
  }
}

MediaStorCheck::~MediaStorCheck() {
  if (StorageManager::MT_EXT_STOR == mt_) {
    UeventMonitor* um = stor_mgr_->uevt_monitor();
    if (um) {
      um->unsubscribe_event_notifier(this);
    }
  }
}

void MediaStorCheck::uevent_callback(void* client,
                                     UeventMonitor::Event event,
                                     const UeventMonitor::ActionInfo& info) {
  MediaStorCheck* msc = static_cast<MediaStorCheck*>(client);

  // If the current media is unmounted, stop writing files to it.
  switch (event) {
    case UeventMonitor::ADD:
      // The major and minor number is unknown now, just change the state
      if (MMS_MOUNTED != msc->mount_state_) {
        msc->mount_state_ = MMS_MEDIA_INSERTED;
      }
      break;
    case UeventMonitor::REMOVE:
      if (msc->m_major_num == info.dn.major_num &&
          msc->m_minor_num == info.dn.minor_num &&
          msc->mount_state_ != MMS_MEDIA_REMOVED) {

        msc->mount_state_ = MMS_MEDIA_REMOVED;
        if (msc->stor_mgr_->current_storage() == msc) {
          msc->stor_mgr_->stop_all_cps();
        }
      }
      break;
    default:
      break;
  }
}

static long get_time_stamp() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec;
}

/*
 * because sd card is remounted many times while booting,
 * we don't use sd until 10s passed after its first ready.
 */
static bool is_external_ready() {
  static bool is_first = true;
  static int64_t begin_time = get_time_stamp();
  const int wait_time = 10;

  if (!is_first) return true;

  int64_t current_time = get_time_stamp();
  if ((current_time - begin_time) > wait_time) {
    is_first = false;
    info_log("external storage is ready to use");
    return true;
  }

  info_log("not use external storage until 10s passed");
  return false;
}

StorageManager::StorageState MediaStorCheck::get_state() {
  StorageManager::StorageState se = StorageManager::NOT_READY;
  LogString path;

  if (str_empty(path_)) {
    if (StorageManager::MT_EXT_STOR == mt_) {
      if (parse_external_storage_mount(path_)) {
        path = path_;
      }
    } else if (StorageManager::MT_INT_STOR == mt_ &&
             StorageManager::MPRIOR_0 == mp_) {
    // due to system privilege control, /data root directory can not
    // be accessed, thus use /data/ylog
      path_ = LogString(DATA_PATH);
      path = path_+ "/" + PARENT_LOG;
    } else {
      #ifdef INT_STORAGE_PATH
      path_ = INT_STORAGE_PATH;
      path = path_;
      #endif
    }
  } else if (StorageManager::MT_INT_STOR == mt_ &&
             StorageManager::MPRIOR_0 == mp_) {
    // due to system privilege control, /data root directory can not
    // be accessed, thus use /data/ylog
    path = path_+ "/" + PARENT_LOG;
  } else {
    path = path_;
  }

  if (!str_empty(path)) {
    if (access(ls2cstring(path), F_OK | R_OK | X_OK)) {
      if (in_use_) {
        se = StorageManager::VANISHED;
      }
    } else {
      if (StorageManager::MT_EXT_STOR == mt_ && !is_external_ready()) {
        se = StorageManager::NOT_READY;
      } else {
        se = StorageManager::STOR_READY;
      }
      // even if fuse mount point is accessible, it may be
      // removed physically
      if (mount_state_ == MMS_MEDIA_REMOVED) {
        if (in_use_) {
          se = StorageManager::VANISHED;
        } else {
          se = StorageManager::NOT_READY;
        }
      } else {
        mount_state_ = MMS_MOUNTED;
      }
    }
  }

  return se;
}

bool MediaStorCheck::parse_external_storage_mount(LogString& sd_root) {
  FILE* pf;

  pf = fopen("/proc/mounts", "r");
  if (!pf) {
    err_log("can't open /proc/mounts");
    return false;
  }

  bool sd_present = false;
  char mnt_line[1024];

  while (fgets(mnt_line, 1024, pf)) {
    // Skip the first field
    size_t tlen;
    const uint8_t* tok =
        get_token(reinterpret_cast<const uint8_t*>(mnt_line), tlen);
    if (!tok) {
      continue;
    }

    if (get_major_minor_in_mount(tok, tlen)) {
      continue;
    }

    size_t path_len;
    const char* mnt_point = reinterpret_cast<const char*>(
        get_token(reinterpret_cast<const uint8_t*>(tok + tlen), path_len));
    if (!mnt_point) {
      continue;
    }

    tok =
        get_token(reinterpret_cast<const uint8_t*>(mnt_point + path_len), tlen);

    if (tok && ((4 == tlen && !memcmp(tok, "vfat", 4)) ||
                (5 == tlen && !memcmp(tok, "exfat", 5)))) {
      LogString vfat_path;

      str_assign(vfat_path, mnt_point, path_len);

      sd_present = !access(ls2cstring(vfat_path), R_OK | W_OK | X_OK);
      if (sd_present) {
        sd_root = vfat_path;
        info_log("sd card is mounted at path: %s.", ls2cstring(sd_root));
      }

      break;
    }
  }

  if (stor_mgr_->use_fuse_stor()) {  // Use FUSE mounts
    if (sd_present) {  // VFAT/exFAT file system got
      sd_present = false;
      // Search for the FUSE mount point under /storage
      while (fgets(mnt_line, 1024, pf)) {
        // Skip the first field
        size_t tlen;
        const uint8_t* tok =
            get_token(reinterpret_cast<const uint8_t*>(mnt_line), tlen);
        if (!tok) {
          continue;
        }

        // The mount point
        tok += tlen;
        tok = get_token(reinterpret_cast<const uint8_t*>(tok), tlen);
        if (!tok) {
          continue;
        }
        if (tlen <= 8 || memcmp(tok, "/storage/", 9) ||
            memchr(tok + 9, '/', tlen - 9)) {
          continue;
        }

        // File system type
        const uint8_t* fs_type;
        size_t fs_len;

        fs_type = get_token(reinterpret_cast<const uint8_t*>(tok + tlen),
                            fs_len);
        if (!fs_type) {
          continue;
        }

        if (4 == fs_len && !memcmp(fs_type, "fuse", 4)) {
          sd_present = true;
        } else if (8 == fs_len && !memcmp(fs_type, "sdcardfs", 8)) {
          sd_present = true;
        } else {
          continue;
        }

        str_assign(sd_root, reinterpret_cast<const char*>(tok), tlen);
        info_log("ext SD fuse path %s", ls2cstring(sd_root));
        break;
      }
    }
  }

  fclose(pf);

  return sd_present;
}

bool MediaStorCheck::operator<(const MediaStorCheck& msc) const {
  return mt_ < msc.mt_ ? true : (mp_ < msc.mp_);
}
