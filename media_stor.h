/*
 *  media_stor.h - storage manager for one media.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-5-14 Zhang Ziyi
 *  Initial version.
 */
#ifndef _MEDIA_STOR_H_
#define _MEDIA_STOR_H_

#include <climits>
#include <memory>

#include "cp_log_cmn.h"
#include "log_file.h"

class StorageManager;
class CpDirectory;
class CpSetDirectory;
class FileWatcher;

class MediaStorage {
 public:
  MediaStorage(StorageManager* stor_mgr);
  ~MediaStorage();

  MediaStorage& operator = (const MediaStorage&) = delete;

  /*  media_init - Initialisation of media storage
   *  @log_path: log path of the storage
   *  @priority: priority of log path
   */
  void media_init(const LogString& log_path, unsigned priority);

  const LogString& get_top_dir() const { return m_log_dir; }
  const LogList<CpSetDirectory*>& all_cp_sets() { return m_log_dirs; }
  unsigned priority() const { return m_priority; }

  /*
   *  sync_media - synchronisation of the MediaStorage object when
   *               the media is present.
   *
   *  @priority: priority of the storage path
   *  @log_path: log path of storage
   *  @fw: the FileWatcher object.
   *
   *  Return 0 on success, -1 on failure.
   */
  int sync_media(LogString& root_path, unsigned priority, FileWatcher* fw);
  /*  stor_media_vanished - reset all the parameters when one of the
   *                        media file system is removed in the system.
   *
   *  @mp: media storage priority
   *
   */
  void stor_media_vanished(unsigned mp);

  StorageManager* stor_mgr() { return m_stor_mgr; }

  FileWatcher* file_watcher() { return m_file_watcher; }

  CpDirectory* current_cp_dir(const LogString& name);
  /*  prepare_cp_set - create new CP set directory for log/dump/minidump
   *                   if CP set is not available
   *
   *  Return CpDirectory pointer on success, 0 on failure.
   */
  CpSetDirectory* prepare_cp_set(CpClass cp_class);

  /*  create_cp_set - create a new CP set directory and make it the
   *                  current CP set directory.
   *
   *  This function will create new CP set directory and new CP
   *  directory.
   *
   *  Return CpDirectory pointer on success, 0 on failure.
   */
  CpSetDirectory* create_cp_set(CpClass cp_class);

  CpSetDirectory* get_cp_set(CpClass cp_class, unsigned priority = UINT_MAX);

  // Non-log file
  std::weak_ptr<LogFile> create_file(CpType ct, CpClass cp_class, LogFile::LogType t,
                                     bool& new_cp_dir, int flags = 0,
                                const LogString& fname = LogString(""));

  uint64_t size() const { return m_size; }
  void add_size(size_t len);
  void dec_size(size_t len);

  /*  check_cp_quota - check the subsystem's quota.
   *  @ct: subsystem type
   *  @cp_class: storage stage
   *  @limit: quota size
   *  @max_file: max size for one log file.
   *  @overwrite: true - log overwrite is enabled,
   *              false - log overwrite is disabled.
   *
   *  Return Value:
   *    0: there is more room for log.
   *    -1: there is no more room for log.
   */
  int check_cp_quota(CpType ct, CpClass cp_class,
                     uint64_t limit,
                     uint64_t max_file,
                     bool overwrite);

  void stop(CpClass cp_class, CpType ct);
  /*  stop - stop the media storage.
   *
   *  This function is called when we stop using the media for logging.
   *  The caller shall be sure all CpStorages' have stopped using the
   *  storage media.
   */
  void stop();

  void clear();

 private:
  LogString class_to_path(CpClass cp_class);

  bool is_media_full();

 private:
  StorageManager* m_stor_mgr;
  // Log dir on the media
  LogString m_log_dir;
  LogList<CpSetDirectory*> m_log_dirs;
  uint64_t m_size;
  // File watcher
  FileWatcher* m_file_watcher;
  // priority of media path
  unsigned m_priority;
};

#endif  // !_MEDIA_STOR_H_
