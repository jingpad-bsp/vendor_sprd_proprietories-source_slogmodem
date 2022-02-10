/*
 *  cp_dump.cpp - The CP dump class.
 *
 *  Copyright (C) 2015-2019 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2015-6-13 Zhang Ziyi
 *  Initial version.
 *
 *  2017-6-3 Zhang Ziyi
 *  Join the Transaction class family.
 *
 *  2019-4-10 ELon li
 *  Move copy_dump_content to this class.
*/

#include <poll.h>
#include "cp_dir.h"
#include "cp_dump.h"
#include "cp_stor.h"
#include "def_config.h"
#include "log_file.h"
#include "log_pipe_hdl.h"

TransCpDump::TransCpDump(TransactionManager* tmgr,
                         Type t,
                         LogPipeHandler* subsys,
                         CpStorage& cp_stor,
                         const struct tm& lt,
                         const char* prefix)
    : TransDiagDevice{tmgr, t},
      subsys_{subsys},
      storage_{cp_stor},
      m_time{lt},
#ifdef USE_ORCA_MODEM
      load_info_{},
      region_{},
      region_size_{},
#endif
      name_prefix_{prefix} {}

TransCpDump::~TransCpDump() {
  if (auto dump_file = m_dump_file.lock()) {
    dump_file->close();
  }
}

bool TransCpDump::open_dump_file() {
  bool ret = true;
  char log_name[64];

  snprintf(log_name, sizeof log_name,
           "_%04d%02d%02d-%02d%02d%02d.dmp", m_time.tm_year + 1900,
           m_time.tm_mon + 1, m_time.tm_mday, m_time.tm_hour, m_time.tm_min,
           m_time.tm_sec);
  LogString dump_file_name = name_prefix_ + log_name;
  m_dump_file = storage_.create_file(dump_file_name, LogFile::LT_DUMP);
  if (0 == m_dump_file.use_count()) {
    ret = false;
    err_log("open dump file %s failed", ls2cstring(dump_file_name));
  }

  return ret;
}

LogFile* TransCpDump::open_dump_mem_file(const char* type) {
  char log_name[64];

  snprintf(log_name, sizeof log_name,
           "_%s_%04d%02d%02d-%02d%02d%02d.mem", type, m_time.tm_year + 1900,
           m_time.tm_mon + 1, m_time.tm_mday, m_time.tm_hour, m_time.tm_min,
           m_time.tm_sec);
  LogString mem_file_name = name_prefix_ + log_name;

  if (auto f = storage_.create_file(mem_file_name, LogFile::LT_DUMP).lock()) {
    return f.get();
  } else {
    err_log("open dump mem file %s failed", ls2cstring(mem_file_name));
    return nullptr;
  }
}

void TransCpDump::remove_dump_file() {
  auto dump_file = m_dump_file.lock();
  if (dump_file) {
    dump_file->close();
    dump_file->dir()->remove(dump_file);
    m_dump_file.reset();
  }
}

bool TransCpDump::copy_dump_content(const char* mem_name,
                                    const char* mem_path) {
  LogFile* mem_file = open_dump_mem_file(mem_name);
  bool ret{};

  if (mem_file) {
#ifdef USE_ORCA_MODEM
    size_t buf_size = std::min(region_size_, DUMP_ONCE_READ_SIZE);

    mem_file->reset_buffer(buf_size);
    modem_iocmd(MODEM_READ_LOCK_CMD, mem_path, nullptr);
#endif
    if (mem_file->copy(mem_path)) {
      err_log("dump memory: %s failed.", mem_path);
    } else {
      ret = true;
      info_log("dump memory: %s successfully.", mem_path);
    }
    mem_file->close();
#ifdef USE_ORCA_MODEM
    modem_iocmd(MODEM_READ_UNLOCK_CMD, mem_path, nullptr);
#endif
  } else {
    err_log("create dump file %s failed", mem_name);
  }

  return ret;
}

bool TransCpDump::open_dump_etb_file() {
  bool ret = true;
  char log_name[64];

  snprintf(log_name, sizeof log_name,
           "_orca_etb_%04d%02d%02d-%02d%02d%02d.bin", m_time.tm_year + 1900,
           m_time.tm_mon + 1, m_time.tm_mday, m_time.tm_hour, m_time.tm_min,
           m_time.tm_sec);
  LogString etb_file_name = name_prefix_ +log_name;
  m_dump_etb_file = storage_.create_file(etb_file_name, LogFile::LT_DUMP);
  if (0 == m_dump_etb_file.use_count()) {
    ret = false;
    err_log("open dump file %s failed", ls2cstring(etb_file_name));
  }

  return ret;
}

void TransCpDump::remove_dump_etb_file() {
  auto dump_etb_file = m_dump_etb_file.lock();
  if (dump_etb_file) {
    dump_etb_file->close();
    dump_etb_file->dir()->remove(dump_etb_file);
    dump_etb_file.reset();
  }
}

