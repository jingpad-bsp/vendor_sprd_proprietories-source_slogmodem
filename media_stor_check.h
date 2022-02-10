/*
 *  media_stor_check.h - check the availability of the specific storage
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-2-20 Yan Zhihang
 *  Initial version.
 */

#ifndef MEDIA_STOR_CHECK_H
#define MEDIA_STOR_CHECK_H

#include "stor_mgr.h"
#include "uevent_monitor.h"

class MediaStorage;

class MediaStorCheck {
 public:
  enum MediaMountState {
    MMS_NOT_MOUNTED,        // Not mounted
    MMS_MEDIA_INSERTED,     // Got media insert event
    MMS_MOUNTED,            // File system mounted and accessible
    MMS_MEDIA_REMOVED,      // Got media removal event
    MMS_START_UNMOUNT,      // File system unmount notification from storage subsystem
    MMS_FS_MOUNTED          // File system mount notification from storage subsystem
  };

  MediaStorCheck(StorageManager* stor_mgr,
                 MediaStorage* ms,
                 StorageManager::MediaType mt,
                 StorageManager::MediaPrior mp,
                 bool active,
                 const LogString& path);
  ~MediaStorCheck();

  MediaStorCheck(const MediaStorCheck&) = delete;
  MediaStorCheck& operator = (const MediaStorCheck&) = delete;

  /*  get_state - check if the stor_info_ is usable
   *
   *  Return Value:
   *    StorageManager::NOT_READY: not ready
   *    StorageManager::VANISHED: media in use and vanished
   *    StorageManager::STOR_READY: ready to use
   */
  StorageManager::StorageState get_state();

  const LogString path() const { return path_; }
  StorageManager::MediaType mt() const { return mt_; }
  StorageManager::MediaPrior mp() const { return mp_; }
  MediaStorage* ms() { return ms_; }
  void set_path_vanished() { path_.clear(); }
  void set_use(bool use) { in_use_ = use; }
  void set_sync(bool sync) { sync_ = sync; }

  bool in_use() const { return in_use_; }
  bool log_synchronized() const  { return sync_; }

  bool permit() const { return permit_; }
  void set_permission(bool permit) { permit_ = permit; }

  bool operator<(const MediaStorCheck& msc) const;

 private:
  /*  parse_external_storage_mount - get the external storage path.
   *
   *  Return Value:
   *    Return true if mounted, false otherwise.
   */
  bool parse_external_storage_mount(LogString& sd_root);
  // get major and minor number from mount file for external storage device
  int get_major_minor_in_mount(const uint8_t* mount_info, size_t len);

  static void uevent_callback(void* client,
                              UeventMonitor::Event event,
                              const UeventMonitor::ActionInfo& info);

 private:
  // if permission is given for storage selection
  bool permit_;
  StorageManager* stor_mgr_;
  MediaStorage* ms_;
  StorageManager::MediaType mt_;
  StorageManager::MediaPrior mp_;
  bool in_use_;
  LogString path_;
  MediaMountState mount_state_;
  bool sync_;
  // Major number of storage device
  unsigned m_major_num;
  // Minor number of storage device
  unsigned m_minor_num;
};

#endif
