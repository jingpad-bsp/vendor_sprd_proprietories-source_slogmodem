/*
 *  cp_dir.h - directory object for one CP in one run.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-5-14 Zhang Ziyi
 *  Initial version.
 */
#ifndef _CP_DIR_H_
#define _CP_DIR_H_

#include <memory>

#include "cp_log_cmn.h"
#include "log_file.h"
#include "file_watcher.h"

class CpSetDirectory;

class CpDirectory {
 public:
  CpDirectory(CpSetDirectory* par_dir, CpType ct, const LogString& dir);
  ~CpDirectory();

  const CpSetDirectory* cp_set_dir() const { return m_cp_set_dir; }
  CpSetDirectory* cp_set_dir() { return m_cp_set_dir; }

  const LogString& path() const { return m_path; }

  uint64_t size() const { return m_size; }
  CpType ct() const { return m_ct; }
  bool empty() const { return m_log_files.empty(); }
  const LogList<std::shared_ptr<LogFile>>& log_files() const { return m_log_files; }
  /*
   * add_log_file - add a log file to cp dir's control
   *                and update the size
   *
   * @lf: new Log File
   */
  void add_log_file(LogFile* lf);
  /*  stat - collect file statistics of the directory.
   *
   *  Return 0 on success, -1 on failure.
   */
  int stat();

  /*  create - create the directory.
   *
   *  Return 0 on success, -1 on failure.
   */
  int create();

  /*  remove - remove the directory.
   *
   *  Return 0 on success, -1 on failure.
   */
  int remove();

  /*  remove - remove file if it's in the file list. Update the size.
   *
   *  Return 0 on success, -1 on failure.
   */
  int remove(std::shared_ptr<LogFile>& f);
  /*  remove - remove file of the base name. Update the size.
   *
   *  Return 0 on success, -1 on failure.
   */
  int remove(const LogString& base_name);
  /*  create_file - create new log file.
   *
   *  Return LogFile pointer on success, 0 on failure.
   */
  std::weak_ptr<LogFile> create_log_file(int flags = 0);
  /*  close_log_file - close current log file.
   *
   *  Return 0 on success, -1 on failure.
   */
  int close_log_file();

  // Non-log file
  std::weak_ptr<LogFile> create_file(const LogString& fname, LogFile::LogType t,
                                     int flags = 0);

  /*  file_removed - inform the CpDirectory object of the file removal.
   *  @f: the LogFile that has been removed.
   *
   *  This function shall be called after the f is closed.
   *  This function is to be cleaned up.
   */
  void file_removed(LogFile* f);

  void add_size(size_t len);
  void dec_size(size_t len);

  /*  trim - trim the working directory.
   *  @sz: the size to trim.
   *
   *  This function won't delete the current log.
   *
   *  Return 0 on success, -1 on failure.
   */
  uint64_t trim(uint64_t sz);

  void stop();

  /*  check_quota - check the directories' quota.
   *  @max_size: the quota for the directory.
   *  @overwrite: true - log overwrite is enabled,
   *              false - log overwrite is disabled.
   *
   *  Return Value:
   *    0: there is more room for log
   *    -1: no room for log
   */
  int check_quota(uint64_t max_size, bool overwrite);
  void rm_oldest_log_file();

 private:
  /*
   * trim_log_file_num - check if the log files' number is no greater than num
   *
   * @num: max log files' number
   *
   * Return Value:
   *  True if log number is less or equal to num, otherwise false
   */
  bool trim_log_file_num(unsigned num);

  void cancel_watch();

  const char* type_to_cp_name(CpType ct);

  static void insert_ascending(LogList<std::shared_ptr<LogFile>>& lst,
                               std::shared_ptr<LogFile>&& f);
  /*  log_delete_notify - current log file deletion notification
   *                      function.
   *  @client: pointer to the CpDirectory object
   *  @evt: the file events, represented in IN_xxx macros
   *
   */
  static void log_delete_notify(void* client, uint32_t evt);

 private:
  CpSetDirectory* m_cp_set_dir;
  CpType m_ct;
  LogString m_path;
  // Log size of the directory
  uint64_t m_size;
  // Log list
  LogList<std::shared_ptr<LogFile>> m_log_files;
  // Current log file
  std::weak_ptr<LogFile> m_cur_log;
  // File watch on current log
  FileWatcher::FileWatch* m_log_watch;
  bool m_store_full;
};

#endif  // !_CP_DIR_H_
