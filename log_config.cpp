/*
 *  log_config.cpp - The configuration class implementation.
 *
 *  Copyright (C) 2015-2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2015-2-16 Zhang Ziyi
 *  Initial version.
 *
 *  2017-6-29 Zhang Ziyi
 *  Communication exception log added.
 */
#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include<iostream>
#include<cstdio>

#ifdef HOST_TEST_
#include "prop_test.h"
#else
#include <cutils/properties.h>
#endif

#include "log_config.h"
#include "parse_utils.h"

CpType LogConfig::s_wan_modem_type{CT_UNKNOWN};

LogConfig::LogConfig()
    : m_dirty{},
      m_enable_md{},
      m_md_save_to_int{},
#ifdef SUPPORT_AGDSP
      m_agdsp_log_dest{AGDSP_LOG_DEST_OFF},
      m_agdsp_pcm_dump{},
#endif
      ext_stor_active_{true},
      use_work_conf_file_{false} {}

LogConfig::~LogConfig() {
  clear_ptr_container(m_iq_config);
  clear_ptr_container(m_config);
}

void LogConfig::set_conf_files(const LogString& init_conf,
                               const LogString& work_conf) {
  m_initial_config_file = init_conf;
  m_config_file = work_conf;
}

int LogConfig::parse_config_file(const char* config_file_path) {
  FILE* pf = fopen(config_file_path, "rt");
  if (!pf) {
    return -1;
  }

  int line_num = 0;
  uint8_t* buf = new uint8_t[1024];

  // Read config file
  while (true) {
    char* p = fgets(reinterpret_cast<char*>(buf), 1024, pf);
    if (!p) {
      break;
    }
    ++line_num;
    int err = parse_line(buf);
    if (-1 == err) {
      err_log("invalid line: %d in %s\n", line_num, config_file_path);
    }
  }

  delete[] buf;
  fclose(pf);

  return 0;
}

void LogConfig::change_modem_log_dest(unsigned num) {
  if (num < 2) {  // Don't save to SD.
    for (auto c: m_config) {
      if (s_wan_modem_type == c->type) {
        if (LM_OFF != c->mode) {
          c->mode = LM_OFF;
          m_dirty = true;
        }
        break;
      }
    }
  } else if (2 == num) {  // Log -> SD
    for (auto c: m_config) {
      if (s_wan_modem_type == c->type) {
        if (LM_OFF == c->mode) {
          c->mode = LM_NORMAL;
          m_dirty = true;
        }
        break;
      }
    }
  }
}

void LogConfig::change_wcn_log_dest(unsigned num) {
  if (num < 2) {  // Don't save to SD.
    for (auto c: m_config) {
      if (CT_WCN == c->type) {
        if (LM_OFF != c->mode) {
          c->mode = LM_OFF;
          m_dirty = true;
        }
        break;
      }
    }
  } else if (2 == num) {  // Log -> SD
    for (auto c: m_config) {
      if (CT_WCN == c->type) {
        if (LM_NORMAL != c->mode) {
          c->mode = LM_NORMAL;
          m_dirty = true;
        }
        break;
      }
    }
  }
}

int LogConfig::read_config() {
#ifdef INT_STORAGE_PATH
  m_internal_path = INT_STORAGE_PATH;
  info_log("internal log path is appointed to %s", INT_STORAGE_PATH);
#endif

#ifdef EXT_STORAGE_PATH
  m_mount_point = EXT_STORAGE_PATH;
  info_log("external log path is appointed to %s", EXT_STORAGE_PATH);
#endif
  property_get("ro.build.type", version, "");
  property_get("ro.carrier", oper_rst, "");
  info_log("read_config version=%s, oper_rst=%s.",version, oper_rst);

  // parse initial system configuration file
  if (parse_config_file(ls2cstring(m_initial_config_file))) {
    err_log("Fail to parse %s", ls2cstring(m_initial_config_file));
    return -1;
  }

  // parse slogmodem proper configuration file
  if (parse_config_file(ls2cstring(m_config_file))) {
    err_log("Fail to parse %s", ls2cstring(m_config_file));
  } else {
    use_work_conf_file_ = true;
  }

  propget_wan_modem_type();


  return 0;
}

int LogConfig::parse_mipilog_line(const uint8_t* buf, MipiLogList& mipi_log) {
  const uint8_t* t;
  LogString type;
  LogString channel;
  LogString freq;
  size_t tlen;
  info_log("parse mipi log");
  t = get_token(buf, tlen);
  if (!t) {
    return -1;
  }
  if (tlen != 7 ||
          (memcmp(t, "serdes0", 7) && memcmp(t, "serdes1", 7) && memcmp(t, "serdes2", 7))) {
    err_log("type of mipi log is invalid");
    return -1;
  }

  str_assign(type, reinterpret_cast<const char*>(t), tlen);

  buf = t + tlen;
  t = get_token(buf, tlen);
  if (!t) {
    return -1;
  }
  bool chn_find {false};
  switch (tlen) {
  case 2:
    if (!memcmp(t, "nr", tlen) || !memcmp(t, "v3", tlen)) {
      chn_find = true;
    }
    break;
  case 4:
    if (!memcmp(t, "dbus", tlen) || !memcmp(t, "tpiu", tlen)) {
      chn_find = true;
    }
    break;
  case 5:
    if (!memcmp(t, "close", tlen)) {
      chn_find = true;
    }
    break;
  case 7:
    if (!memcmp(t, "disable", tlen)) {
      chn_find = true;
    }
    break;
  case 8:
    if (!memcmp(t, "training", tlen)) {
      chn_find = true;
    }
    break;
  default:
    break;
  }
  if (chn_find) {
    str_assign(channel, reinterpret_cast<const char*>(t), tlen);
  } else {
    err_log("channel of mipi log is invalid");
    return -1;
  }

  buf = t + tlen;
  t = get_token(buf, tlen);
  if (!t) {
    return -1;
  }

  bool freq_find {false};
  switch (tlen) {
  case 6:
    if (!memcmp(t, "500000", tlen)) {
      freq_find = true;
    }
    break;
  case 7:
    if (!memcmp(t, "1500000", tlen) || !memcmp(t, "2000000", tlen) ||
        !memcmp(t, "2500000", tlen)) {
      freq_find = true;
    }
    break;
  default:
    break;
  }
  if (freq_find) {
    str_assign(freq, reinterpret_cast<const char*>(t), tlen);
  } else {
    err_log("freq of mipi log is invalid");
    return -1;
  }
  MipiLogEntry* tmp_mipi_entry = new MipiLogEntry {
                type,
                channel,
                freq
              };

  for (auto c : m_mipilog) {
    MipiLogEntry* mipi_log = (MipiLogEntry*)c;
    if (!memcmp(ls2cstring(mipi_log->type), ls2cstring(type), 7)) {
      ll_remove(m_mipilog, c);
      delete c;
      break;
    }
  }
  m_mipilog.push_back(tmp_mipi_entry);
  return 0;
}

int LogConfig::parse_minidump_line(const uint8_t* buf, bool& en,
                                   bool& save_to_int) {
  const uint8_t* t;
  const uint8_t* locate;
  size_t tlen;

  t = get_token(buf, tlen);
  if (!t) {
    return -1;
  }

  if ((6 == tlen) && !memcmp(t, "enable", 6)) {
    en = true;
  } else {
    en = false;
  }

  buf = t + tlen;
  locate = get_token(buf, tlen);

  if (!locate) {
    return -1;
  }

  if ((8 == tlen) && !memcmp(locate, "internal", 8)) {
    save_to_int = true;
  } else {
    save_to_int = false;
  }

  return 0;
}

int LogConfig::parse_number(const uint8_t* buf, size_t& num) {
  unsigned long n;
  char* endp;

  n = strtoul(reinterpret_cast<const char*>(buf), &endp, 0);
  if ((ULONG_MAX == n && ERANGE == errno) || !isspace(*endp)) {
    return -1;
  }

  num = static_cast<size_t>(n);
  return 0;
}

int LogConfig::get_log_mode(const uint8_t* tok, size_t len,
                            LogMode& lm) {
  int ret {-1};

  switch (len) {
    case 2:
      if (!memcmp(tok, "on", 2)) {
        lm = LM_NORMAL;
        ret = 0;
      }
      break;
    case 3:
      if (!memcmp(tok, "evt", 3)) {
        lm = LM_EVENT;
        ret = 0;
      } else if (!memcmp(tok, "off", 3)) {
        lm = LM_OFF;
        ret = 0;
      }
      break;
    default:
      break;
  }

  return ret;
}

int LogConfig::parse_stream_line(const uint8_t* buf) {
  const uint8_t* t;
  size_t tlen;
  const uint8_t* pn;
  size_t nlen;

  // Get the modem name
  pn = get_token(buf, nlen);
  if (!pn) {
    return -1;
  }

  CpType cp_type = get_modem_type(pn, nlen);
  if (CT_UNKNOWN == cp_type) {
    // Ignore unknown CP
    return 0;
  }

  // Get enable state
  LogMode lm;

  buf = pn + nlen;
  t = get_token(buf, tlen);
  if (!t) {
    return -1;
  }

  if (get_log_mode(t, tlen, lm)) {  // Unknown log mode
    return 0;
  }

  // internal size
  buf = t + tlen;
  t = get_token(buf, tlen);
  if (!t) {
    return -1;
  }
  char* endp;
  unsigned long int_sz =
      strtoul(reinterpret_cast<const char*>(t), &endp, 0);
  if ((ULONG_MAX == int_sz && ERANGE == errno) ||
      (' ' != *endp && '\t' != *endp && '\r' != *endp && '\n' != *endp &&
       '\0' != *endp)) {
    return -1;
  }

  // external size
  buf = t + tlen;
  t = get_token(buf, tlen);
  if (!t) {
    return -1;
  }

  unsigned long ext_sz =
      strtoul(reinterpret_cast<const char*>(t), &endp, 0);
  if ((ULONG_MAX == ext_sz && ERANGE == errno) ||
      (' ' != *endp && '\t' != *endp && '\r' != *endp && '\n' != *endp &&
       '\0' != *endp)) {
    return -1;
  }

  // file size
  buf = t + tlen;
  t = get_token(buf, tlen);
  if (!t) {
    return -1;
  }

  unsigned long file_sz =
      strtoul(reinterpret_cast<const char*>(t), &endp, 0);
  if ((ULONG_MAX == file_sz && ERANGE == errno) ||
      (' ' != *endp && '\t' != *endp && '\r' != *endp && '\n' != *endp &&
       '\0' != *endp)) {
    return -1;
  }

  // Level
  buf = reinterpret_cast<const uint8_t*>(endp);
  t = get_token(buf, tlen);
  if (!t) {
    return -1;
  }
  unsigned long level = strtoul(reinterpret_cast<const char*>(t), &endp, 0);
  if ((ULONG_MAX == level && ERANGE == errno) ||
      (' ' != *endp && '\t' != *endp && '\r' != *endp && '\n' != *endp &&
       '\0' != *endp)) {
    return -1;
  }

  //Overwrite
  buf = reinterpret_cast<const uint8_t*>(endp);
  t = get_token(buf, tlen);
  if (!t) {
    err_log("fail to parse overwrite status");
    return -1;
  }

  bool ow = false;;
  if ((2 == tlen) && !memcmp(t, "on", 2)) {
    ow = true;
  }

  ConfigList::iterator it = find(m_config, cp_type);
  if (it != m_config.end()) {
    ConfigEntry* pe = *it;
    str_assign(pe->modem_name, reinterpret_cast<const char*>(pn), nlen);
    pe->mode = lm;
    pe->internal_limit = int_sz;
    if(!strcmp(version, "user")
     && !strcmp(oper_rst, "cmcc")) {
      pe->internal_limit = 1024;
      info_log("CMCC internal_limit =%d changed.",pe->internal_limit);
    }
    pe->external_limit = ext_sz;
    pe->file_size_limit = file_sz;
    pe->level = static_cast<int>(level);
    pe->overwrite = ow;
  } else {
    ConfigEntry* pe = new ConfigEntry{reinterpret_cast<const char*>(pn),
                                      nlen,
                                      cp_type,
                                      lm,
                                      int_sz,
                                      ext_sz,
                                      file_sz,
                                      static_cast<int>(level),
                                      ow};
    m_config.push_back(pe);
  }

  return 0;
}

int LogConfig::parse_storage_line(const uint8_t* buf) {
  int ret = -1;
  size_t tlen;
  const uint8_t* tok;

  tok = get_token(buf, tlen);
  if (!tok) {
    return ret;
  }

  if (8 == tlen) {
    if (!memcmp(tok, "internal", 8)) {
      ext_stor_active_ = false;
    } else {
      ext_stor_active_ = true;
    }

    ret = 0;
  }

  return ret;
}

int LogConfig::parse_iq_line(const uint8_t* buf) {
  size_t tlen;
  const uint8_t* tok;

  // Get the modem name
  tok = get_token(buf, tlen);
  if (!tok) {
    return -1;
  }
  CpType cp_type = get_modem_type(tok, tlen);
  if (CT_UNKNOWN == cp_type) {
    // Ignore unknown CP
    err_log("invalid I/Q CP type");
    return 0;
  }

  bool gsm_iq = false;
  bool wcdma_iq = false;

  bool iq_got = false;
  bool iq_type_err = false;
  while (1) {
    buf = tok + tlen;
    tok = get_token(buf, tlen);
    if (!tok) {
      break;
    }

    IqType iqt = get_iq_type(tok, tlen);
    if (IT_UNKNOWN == iqt) {
      // Invalid I/Q type
      iq_type_err = true;
      break;
    }
    iq_got = true;
    if (IT_GSM == iqt) {
      gsm_iq = true;
    } else {
      wcdma_iq = true;
    }
  }

  if (iq_type_err) {
    err_log("invalid I/Q type");
    return 0;
  }
  if (!iq_got) {
    err_log("no I/Q type");
    return 0;
  }

  auto it = std::find_if(m_iq_config.begin(), m_iq_config.end(),
                         [&cp_type] (const IqConfig* iqc) {
                           return cp_type == iqc->cp_type;
                         });
  if (it != m_iq_config.end()) {
    auto pe = *it;
    pe->gsm_iq = gsm_iq;
    pe->wcdma_iq = wcdma_iq;
  } else {
    m_iq_config.push_back(new IqConfig{cp_type, gsm_iq, wcdma_iq});
  }

  return 0;
}

int LogConfig::parse_line(const uint8_t* buf) {
  // Search for the first token
  const uint8_t* t;
  size_t tlen;
  int err = -1;

  t = get_token(buf, tlen);
  if (!t || '#' == *t) {
    return 0;
  }

  // What line?
  buf += tlen;
  switch (tlen) {
    case 2:
      if (!memcmp(t, "iq", 2)) {
        err = parse_iq_line(buf);
      }
      break;
    case 6:
      if (!memcmp(t, "stream", 6)) {
        err = parse_stream_line(buf);
      }
      break;
    case 7:
      if (!memcmp(t, "storage", 7)) {
        err = parse_storage_line(buf);
      } else if (!memcmp(t, "mipilog", 7)) {
        err = parse_mipilog_line(buf, m_mipilog);
      }
      break;
    case 8:
      if (!memcmp(t, "minidump", 8)) {
        err = parse_minidump_line(buf, m_enable_md, m_md_save_to_int);
      }
      break;
#ifdef SUPPORT_AGDSP
    case 14:
      if (!memcmp(t, "agdsp_log_dest", 14)) {
        err = parse_agdsp_log_dest(buf, m_agdsp_log_dest);
      } else if (!memcmp(t, "agdsp_pcm_dump", 14)) {
        err = parse_on_off(buf, m_agdsp_pcm_dump);
      }
      break;
#endif
   default:
      break;
  }

  return err;
}

CpType LogConfig::get_modem_type(const uint8_t* name, size_t len) {
  CpType type = CT_UNKNOWN;

  switch (len) {
    case 5:
#ifdef SUPPORT_AGDSP
      if (!memcmp(name, "agdsp", 5)) {
        type = CT_AGDSP;
      } else
#endif
      if (!memcmp(name, "pm_sh", 5)) {
        type = CT_PM_SH;
      }
      break;
    case 6:
      if (!memcmp(name, "cp_wcn", 6)) {
        type = CT_WCN;
      }
      break;
    case 7:
      if (!memcmp(name, "cp_gnss", 7)) {
        type = CT_GNSS;
      }
      break;
    case 8:
      if (!memcmp(name, "cp_wcdma", 8)) {
        type = CT_WCDMA;
      } else if (!memcmp(name, "cp_3mode", 8)) {
        type = CT_3MODE;
      } else if (!memcmp(name, "cp_4mode", 8)) {
        type = CT_4MODE;
      } else if (!memcmp(name, "cp_5mode", 8)) {
        type = CT_5MODE;
      }
      break;
    case 9:
      if (!memcmp(name, "cp_orcaap", 9)) {
        type = CT_ORCA_MINIAP;
      } else if (!memcmp(name, "cp_orcadp", 9)) {
        type = CT_ORCA_DP;
      }
      break;
    case 11:
      if (!memcmp(name, "cp_td-scdma", 11)) {
        type = CT_TD;
      }
      break;
    default:
      break;
  }

  return type;
}

const char* LogConfig::log_mode_to_string(LogMode mode) {
  const char* ms;

  switch (mode) {
    case LM_OFF:
      ms = "off";
      break;
    case LM_NORMAL:
      ms = "on";
      break;
    default:
      ms = "evt";
      break;
  }

  return ms;
}

void LogConfig::reset() {
    const char* config =  ls2cstring(m_config_file);
    int res = remove(config);
    if (res  != 0){
        info_log("remove %s failed,error is %s", config, strerror(errno));
    }
    kill(getpid(), SIGKILL);
    usleep(300 * 1000);
}

int LogConfig::save() {
  FILE* pf = fopen(ls2cstring(m_config_file), "w+t");

  if (!pf) {
    return -1;
  }

  // storage choice
  fprintf(pf, "storage %s\n", ext_stor_active_ ? "external" : "internal");

  // Mini dump
  if (m_md_save_to_int) {
    fprintf(pf, "minidump %s\t%s\n", m_enable_md ? "enable" : "disable",
            "internal");
  } else {
    fprintf(pf, "minidump %s\t%s\n", m_enable_md ? "enable" : "disable",
            "external");
  }
  //mipi log
  for (auto c : m_mipilog) {
    fprintf(pf, "mipilog %s\t%s\t%s\n", ls2cstring(c->type),
                       ls2cstring(c->channel), ls2cstring(c->freq));
  }
  // I/Q config
  LogList<IqConfig*>::iterator it;

  fprintf(pf, "\n");
  for (it = m_iq_config.begin(); it != m_iq_config.end(); ++it) {
    IqConfig* iq = (*it);
    fprintf(pf, "iq %s ", cp_type_to_name(iq->cp_type));
    if (iq->gsm_iq) {
      fprintf(pf, "GSM ");
    }
    if (iq->wcdma_iq) {
      fprintf(pf, "WCDMA");
    }
    fprintf(pf, "\n");
  }

  fprintf(pf, "\n");
  fprintf(pf, "#Type\tName\t\tState\tInternal Size\tExternal Size\tFile Size\tLevel\tOverwrite\n");

  for (ConfigIter it = m_config.begin(); it != m_config.end(); ++it) {
    ConfigEntry* pe = *it;
    fprintf(pf, "stream\t%s\t%s\t%u\t%u\t%u\t%u\t%s\n",
            ls2cstring(pe->modem_name),
            log_mode_to_string(pe->mode),
            static_cast<unsigned>(pe->internal_limit),
            static_cast<unsigned>(pe->external_limit),
            static_cast<unsigned>(pe->file_size_limit),
            pe->level,
            pe->overwrite ? "on" : "off");
  }

  fprintf(pf, "\n");

#ifdef SUPPORT_AGDSP
  // AG-DSP log settings
  const char* agdsp_log_dest;

  switch (m_agdsp_log_dest) {
    case AGDSP_LOG_DEST_OFF:
      agdsp_log_dest = "off";
      break;
    case AGDSP_LOG_DEST_UART:
      agdsp_log_dest = "uart";
      break;
    default: //case AGDSP_LOG_DEST_AP:
      agdsp_log_dest = "ap";
      break;
  }
  fprintf(pf, "agdsp_log_dest %s\n", agdsp_log_dest);

  fprintf(pf, "agdsp_pcm_dump %s\n", m_agdsp_pcm_dump ? "on" : "off");
#endif

  fclose(pf);

  m_dirty = false;

  return 0;
}

void LogConfig::get_cp_storage_limit(CpType ct,
                                     StorageManager::MediaType mt,
                                     size_t& sz) const {
  for (auto it = m_config.begin();
       it != m_config.end(); ++it) {
    ConfigEntry* p = *it;
    if (p->type == ct) {
      if (StorageManager::MT_INT_STOR == mt) {
        sz = p->internal_limit;
      } else if (StorageManager::MT_EXT_STOR == mt) {
        sz = p->external_limit;
      } else {
        err_log("Not supported Media Type");
      }
      break;
    }
  }
}

void LogConfig::set_cp_storage_limit(CpType ct,
                                     StorageManager::MediaType mt, size_t sz) {
  for (ConfigList::iterator it = m_config.begin();
       it != m_config.end(); ++it) {
    ConfigEntry* p = *it;
    if (p->type == ct) {
      if (StorageManager::MT_INT_STOR == mt) {
        m_dirty = true;
        p->internal_limit = sz;
      } else if (StorageManager::MT_EXT_STOR == mt) {
        m_dirty = true;
        p->external_limit = sz;
      } else {
        err_log("Not supported Media Type");
      }
      break;
    }
  }
}

void LogConfig::set_cp_log_file_size(CpType ct, size_t sz) {
  for (ConfigList::iterator it = m_config.begin();
       it != m_config.end(); ++it) {
    ConfigEntry* p = *it;
    if (p->type == ct) {
      p->file_size_limit = sz;
      m_dirty = true;
      break;
    }
  }
}

void LogConfig::get_cp_log_file_size(CpType ct, size_t& sz) const {
  for (auto it = m_config.begin();
       it != m_config.end(); ++it) {
    ConfigEntry* p = *it;
    if (p->type == ct) {
      sz = p->file_size_limit;
      break;
    }
  }
}

void LogConfig::set_cp_log_overwrite(CpType ct, bool enabled) {
  auto it = std::find_if(m_config.begin(), m_config.end(),
                         [&ct](const ConfigEntry* config) {
                           return config->type == ct; }
                        );

  if (it != m_config.end()) {
    ConfigEntry* p = *it;
    if (p->overwrite != enabled) {
      p->overwrite = enabled;
      m_dirty = true;
    }
  }
}

void LogConfig::get_cp_log_overwrite(CpType ct, bool& enabled) const {
  auto it = std::find_if(m_config.begin(), m_config.end(),
                         [&ct](const ConfigEntry* config) {
                           return config->type == ct; }
                        );

  if (it != m_config.end()) {
    ConfigEntry* p = *it;
    enabled = p->overwrite;
  }
}

void LogConfig::enable_log(CpType cp, bool en /*= true*/) {
  LogMode lm {en ? LM_NORMAL : LM_OFF};

  for (ConfigList::iterator it = m_config.begin(); it != m_config.end(); ++it) {
    ConfigEntry* p = *it;
    if (p->type == cp) {
      if (p->mode != lm) {
        p->mode = lm;
        m_dirty = true;
      }
      break;
    }
  }
}

void LogConfig::set_log_mode(CpType cp, LogConfig::LogMode mode) {
  for (auto ce: m_config) {
    if (ce->type == cp) {
      if (ce->mode != mode) {
        ce->mode = mode;
        m_dirty = true;
      }
      break;
    }
  }
}

void LogConfig::enable_md(bool en /*= true*/) {
  if (m_enable_md != en) {
    m_enable_md = en;
    m_dirty = true;
  }
}

void LogConfig::enable_iq(CpType cpt, IqType iqt) {
  LogList<IqConfig*>::iterator it = find_iq(m_iq_config, cpt);
  IqConfig* iq;

  if (m_iq_config.end() == it) {
    iq = new IqConfig;
    iq->cp_type = cpt;
    iq->gsm_iq = false;
    iq->wcdma_iq = false;
    m_iq_config.push_back(iq);
  } else {
    iq = *it;
  }
  if (IT_GSM == iqt && !iq->gsm_iq) {
    iq->gsm_iq = true;
    m_dirty = true;
  } else if (IT_WCDMA == iqt && !iq->wcdma_iq) {
    iq->wcdma_iq = true;
    m_dirty = true;
  }
}

void LogConfig::disable_iq(CpType cpt, IqType iqt) {
  LogList<IqConfig*>::iterator it = find_iq(m_iq_config, cpt);
  IqConfig* iq;

  if (m_iq_config.end() == it) {
    return;
  }

  iq = *it;
  if (IT_ALL == iqt) {
    m_iq_config.erase(it);
    delete iq;
    m_dirty = true;
  } else if (IT_GSM == iqt && iq->gsm_iq) {
    iq->gsm_iq = false;
    m_dirty = true;
  } else if (IT_WCDMA == iqt && iq->wcdma_iq) {
    iq->wcdma_iq = false;
    m_dirty = true;
  }
}

bool LogConfig::wcdma_iq_enabled(CpType cpt) const {
  LogList<IqConfig*>::const_iterator it = find_iq(m_iq_config, cpt);
  bool ret = false;

  if (m_iq_config.end() != it && (*it)->wcdma_iq) {
    ret = true;
  }

  return ret;
}

LogConfig::ConfigList::iterator LogConfig::find(ConfigList& clist, CpType t) {
  ConfigList::iterator it;

  for (it = clist.begin(); it != clist.end(); ++it) {
    ConfigEntry* p = *it;
    if (p->type == t) {
      break;
    }
  }

  return it;
}

int get_dev_paths(CpType t, bool& same, LogString& log_path,
                  LogString& diag_path) {
  char prop_val[PROPERTY_VALUE_MAX];
  int err = -1;

  // Get path from property
  switch (t) {
    case CT_WCDMA:
      property_get(MODEM_DIAG_PROPERTY, prop_val, "");
      if (prop_val[0]) {
        char log_prop[PROPERTY_VALUE_MAX];
        property_get(MODEM_LOG_PROPERTY, log_prop, "");
        if (log_prop[0] && strcmp(prop_val, log_prop)) {
          same = false;
          log_path = log_prop;
          diag_path = prop_val;
        } else {
          same = true;
          log_path = prop_val;
        }
        err = 0;
      }
      break;
    case CT_TD:
    case CT_3MODE:
    case CT_4MODE:
    case CT_5MODE:
      property_get(MODEM_LOG_PROPERTY, prop_val, "");
      if (prop_val[0]) {
        log_path = prop_val;
        property_get(MODEM_DIAG_PROPERTY, prop_val, "");
        if (prop_val[0]) {
          same = false;
          diag_path = prop_val;
          err = 0;
        } else {
          log_path.clear();
        }
      }
      break;
    case CT_WCN:
      property_get(MODEM_WCN_DIAG_PROPERTY, prop_val, "");
      if (prop_val[0]) {
        same = true;
        log_path = prop_val;
        err = 0;
      }
      break;
    case CT_GNSS:
      property_get(MODEM_GNSS_DIAG_PROPERTY, prop_val, "");
      if (prop_val[0]) {
        same = true;
        log_path = prop_val;
        err = 0;
      }
      break;
    case CT_AGDSP:
      property_get(AGDSP_LOG_PROPERTY, prop_val, "");
      if (prop_val[0]) {
        same = true;
        log_path = prop_val;
        err = 0;
      }
      break;
    case CT_PM_SH:
      property_get(PM_SENSORHUB_LOG_PROPERTY, prop_val, "");
      if (prop_val[0]) {
        same = true;
        log_path = prop_val;
        err = 0;
      }
      break;
    case CT_ORCA_MINIAP:
      property_get(MODEM_ORCA_AP_LOG_PROPERTY, prop_val, "");
      if (prop_val[0]) {
        same = true;
        log_path = prop_val;
        err = 0;
      }
      break;
    case CT_ORCA_DP:
      property_get(MODEM_ORCA_DP_LOG_PROPERTY, prop_val, "");
      if (prop_val[0]) {
        same = true;
        log_path = prop_val;
        err = 0;
      }
      break;
    default:  // Unknown
      break;
  }

  return err;
}

const char* LogConfig::cp_type_to_name(CpType t) {
  const char* cp_name;

  if (CT_WANMODEM == t) {
    t = get_wan_modem_type();
  }

  switch (t) {
    case CT_WCDMA:
      cp_name = "cp_wcdma";
      break;
    case CT_TD:
      cp_name = "cp_td-scdma";
      break;
    case CT_3MODE:
      cp_name = "cp_3mode";
      break;
    case CT_4MODE:
      cp_name = "cp_4mode";
      break;
    case CT_5MODE:
      cp_name = "cp_5mode";
      break;
    case CT_WCN:
      cp_name = "cp_wcn";
      break;
    case CT_GNSS:
      cp_name = "cp_gnss";
      break;
    case CT_AGDSP:
      cp_name = "agdsp";
      break;
    case CT_PM_SH:
      cp_name = "pm_sh";
      break;
    case CT_ORCA_MINIAP:
      cp_name = "orcaap";
      break;
    case CT_ORCA_DP:
      cp_name = "orcadp";
      break;
    default:
      cp_name = "(unknown)";
      break;
  }

  return cp_name;
}

IqType LogConfig::get_iq_type(const uint8_t* iq, size_t len) {
  IqType iqt = IT_UNKNOWN;

  if (3 == len && !memcmp(iq, "GSM", 3)) {
    iqt = IT_GSM;
  } else if (5 == len && !memcmp(iq, "WCDMA", 5)) {
    iqt = IT_WCDMA;
  }

  return iqt;
}

LogList<LogConfig::IqConfig*>::iterator LogConfig::find_iq(
    LogList<LogConfig::IqConfig*>& iq_list, CpType t) {
  LogList<IqConfig*>::iterator it;

  for (it = iq_list.begin(); it != iq_list.end(); ++it) {
    if ((*it)->cp_type == t) {
      break;
    }
  }

  return it;
}

LogList<LogConfig::IqConfig*>::const_iterator LogConfig::find_iq(
    const LogList<LogConfig::IqConfig*>& iq_list, CpType t) {
  LogList<IqConfig*>::const_iterator it;

  for (it = iq_list.begin(); it != iq_list.end(); ++it) {
    if ((*it)->cp_type == t) {
      break;
    }
  }

  return it;
}

int LogConfig::parse_on_off(const uint8_t* buf, bool& on_off) {
  const uint8_t* t;
  size_t tlen;

  t = get_token(buf, tlen);
  if (!t) {
    return -1;
  }

  int ret = 0;

  if (2 == tlen && !memcmp(t, "on", 2)) {
    on_off = true;
  } else if (3 == tlen && !memcmp(t, "off", 3)) {
    on_off = false;
  } else {
    ret = -1;
  }

  return ret;
}
