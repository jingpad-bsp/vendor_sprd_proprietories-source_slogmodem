/*
 *  cp_dump.h - The CP dump class.
 *
 *  Copyright (C) 2015-2019 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2015-6-13 Zhang Ziyi
 *  Initial version.
 *
 *  2017-6-3 Zhang Ziyi
 *  Join the Transaction family.
 *
 *  2019-4-10 ELon li
 *  Move copy_dump_content to this class.
 */

#ifndef _CP_DUMP_H_
#define _CP_DUMP_H_

#ifdef USE_ORCA_MODEM
#include <algorithm>
#endif
#include <memory>

#include "log_file.h"
#ifdef USE_ORCA_MODEM
#include "modem_ioctl.h"
#include "modem_utils.h"
#endif
#include "trans_diag.h"

class CpStorage;
class LogFile;
class LogPipeHandler;

class TransCpDump : public TransDiagDevice {
 public:
  TransCpDump(TransactionManager* tmgr,
              Type t,
              LogPipeHandler* subsys,
              CpStorage& cp_stor,
              const struct tm& lt,
              const char* prefix);
  TransCpDump(const TransCpDump&) = delete;
  ~TransCpDump();

  TransCpDump& operator = (const TransCpDump&) = delete;

  const struct tm& time() const { return m_time; }

  LogPipeHandler* subsys() const { return subsys_; }

  CpStorage& storage() const { return storage_; }

 protected:
  bool open_dump_file();
  LogFile* open_dump_mem_file(const char* type = "memory");
  LogFile* dump_file() {
    if (auto dump_file = m_dump_file.lock()) {
      return dump_file.get();
    } else {
      return nullptr;
    }
  }
  void close_dump_file() {
    if (auto dump_file = m_dump_file.lock()) {
      dump_file->close();
      m_dump_file.reset();
    }
  }
  void remove_dump_file();
  bool copy_dump_content(const char* mem_name, const char* mem_path);

  bool open_dump_etb_file();
  LogFile* dump_etb_file() {
    if (auto dump_file = m_dump_etb_file.lock()) {
      return dump_file.get();
    } else {
      return nullptr;
    }
  }
  void close_dump_etb_file() {
    if (auto dump_file = m_dump_etb_file.lock()) {
      dump_file->close();
      m_dump_etb_file.reset();
    }
  }
  void remove_dump_etb_file();

#ifdef USE_ORCA_MODEM
 protected:
  modem_load_info load_info_;
  int region_;
  size_t region_size_;
#endif

 private:
  LogPipeHandler* subsys_;
  CpStorage& storage_;
  struct tm m_time;
  LogString name_prefix_;
  std::weak_ptr<LogFile> m_dump_file;
  std::weak_ptr<LogFile> m_dump_etb_file;
};

#endif  // !_CP_DUMP_H_
