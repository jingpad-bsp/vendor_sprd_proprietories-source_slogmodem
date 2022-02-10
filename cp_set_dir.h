/*
 *  cp_set_dir.h - directory object for one run.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-5-14 Zhang Ziyi
 *  Initial version.
 */
#ifndef _CP_SET_DIR_H_
#define _CP_SET_DIR_H_

#include <memory>

#include "cp_log_cmn.h"
#include "log_file.h"

class MediaStorage;
class CpDirectory;

class CpSetDirectory {
 public:
  /*  CpSetDirectory - constructor
   *  @media: MediaStorage pointer
   *  @dir: full path of the CP set directory
   *
   */
  CpSetDirectory(MediaStorage* media, CpClass cp_class,
                 const LogString& dir, unsigned priority);
  ~CpSetDirectory();

  const LogString& path() const { return m_path; }

  MediaStorage* get_media() { return m_media; }
  unsigned priority() const { return m_priority; }
  bool empty() const { return m_cp_dirs.empty(); }
  uint64_t size() const { return m_size; }
  CpClass cp_class() const { return m_cp_class; }
  const LogList<CpDirectory*>& all_cp_dirs() const { return  m_cp_dirs; }
  /*  stat - collect file statistics of the directory.
   *
   *  Return 0 on success, -1 on failure.
   */
  int stat();

  /*  create - create the CP set directory.
   *
   *  Return 0 on success, -1 on failure.
   */
  int create();

  /*  remove - remove the CP set directory.
   *
   *  Return 0 on success, -1 on failure.
   */
  int remove();
  /* prepare_cp_dir - prepare cp directory for log/minidump/ememdump
   * @cp_name - CP name (wcn/wan modem... ...)
   * @new_cp_dir - if a new cp directory is created.
   *
   * Return cp directory.
   */
  CpDirectory* prepare_cp_dir(CpType ct, bool& new_cp_dir);

  CpDirectory* get_cp_dir(CpType ct);

  std::weak_ptr<LogFile> create_file(CpType ct, const LogString& fname,
                                     LogFile::LogType t, int flags = 0);

  /*  recreate_log_file - recreate the log file after it's deleted.
   *  @ct: the CP type
   *
   */
  std::weak_ptr<LogFile> recreate_log_file(CpType ct, bool& new_cp_dir,
                                           int flags = 0);

  void add_size(size_t len);
  void dec_size(size_t len);

  void stop();
  void stop(CpType ct);

  /*  check_cp_quota - check the specified subsystem's quota.
   *  @ct: CP type.
   *  @limit: the quota for the subsystem.
   *  @overwrite: true - log overwrite is enabled,
   *              false - log overwrite is disabled.
   *
   *  Return Value:
   *    0: there is more room for log
   *    -1: no room for log
   */
  int check_cp_quota(CpType ct, uint64_t limit, bool overwrite);

 private:
  MediaStorage* m_media;
  CpClass m_cp_class;
  // Full path of the directory
  LogString m_path;
  LogList<CpDirectory*> m_cp_dirs;
  unsigned m_priority;
  uint64_t m_size;

  LogString type_to_cp_path(CpType ct);

};

#endif  // !_CP_SET_DIR_H_
