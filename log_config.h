/*
 *  log_config.h - The configuration class declaration.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-2-16 Zhang Ziyi
 *  Initial version.
 */
#ifndef LOG_CONFIG_H_
#define LOG_CONFIG_H_

#include <cstdint>
#ifdef HOST_TEST_
#include "prop_test.h"
#else
#include <cutils/properties.h>
#endif

#include "def_config.h"
#include "cp_log_cmn.h"
#include "stor_mgr.h"

class LogConfig {
 public:
  enum LogMode {
    LM_OFF,
    LM_NORMAL,
    LM_EVENT
  };

  enum AgDspLogDestination {
    AGDSP_LOG_DEST_OFF,
    AGDSP_LOG_DEST_UART,
    AGDSP_LOG_DEST_AP
  };

  enum SaveDirType { CPLOG_DIR, SLOG_DIR, YLOG_DIR };

  struct MipiLogEntry {
    LogString type;
    LogString channel;
    LogString freq;
  };

  typedef LogList<MipiLogEntry*> MipiLogList;

  struct ConfigEntry {
    LogString modem_name;
    CpType type;
    LogMode mode;
    size_t internal_limit;
    size_t external_limit;
    size_t file_size_limit;
    int level;
    bool overwrite;

    ConfigEntry(const char* modem, size_t len, CpType t, LogMode lm,
                size_t internal, size_t external,
                size_t file_size, int lvl, bool ovwt)
        : modem_name(modem, len), type{t}, mode{lm},
          internal_limit{internal},
          external_limit{external},
          file_size_limit{file_size}, level{lvl}, overwrite{ovwt} {}
  };

  typedef LogList<ConfigEntry*> ConfigList;
  typedef LogList<ConfigEntry*>::iterator ConfigIter;
  typedef LogList<ConfigEntry*>::const_iterator ConstConfigIter;

  LogConfig();
  LogConfig(const LogConfig&) = delete;
  ~LogConfig();

  LogConfig& operator = (const LogConfig&) = delete;

  void set_conf_files(const LogString& init_conf, const LogString& work_conf);

  const LogString& get_mount_point() const { return m_mount_point; }
  const LogString& get_internal_path() const { return m_internal_path; }

  /*
   * read_config - read config from specifed file.
   *
   * Return Value:
   *   If the function succeeds, return 0; otherwise return -1.
   */
  int read_config();

  bool dirty() const { return m_dirty; }

  /*
   * save - save config to the config file.
   *
   * Return Value:
   *   If the function succeeds, return 0; otherwise return -1.
   */
  int save();
  void reset();
  const ConfigList& get_conf() const { return m_config; }

  bool md_enabled() const { return m_enable_md; }

  bool md_save_to_int() const {
    return m_md_save_to_int;
  }

  bool int_stor_active() const { return true; }
  bool ext_stor_active() const { return ext_stor_active_; }

  void set_external_stor(bool use) {
    if (ext_stor_active_ != use) {
      ext_stor_active_ = use;
      m_dirty = true;
    }
  }

  void set_md_int_stor(bool md_int_stor) {
    if (m_md_save_to_int != md_int_stor) {
      m_md_save_to_int = md_int_stor;
      m_dirty = true;
    }
  }

  void set_cp_storage_limit(CpType ct,
                            StorageManager::MediaType mt,
                            size_t sz);
  void get_cp_storage_limit(CpType ct,
                            StorageManager::MediaType mt,
                            size_t& sz) const;

  void set_cp_log_file_size(CpType ct, size_t sz);
  void get_cp_log_file_size(CpType ct, size_t& sz) const;
  void set_cp_log_overwrite(CpType ct, bool enabled);
  void get_cp_log_overwrite(CpType ct, bool& enabled) const;

  void enable_log(CpType cp, bool en = true);
  void set_log_mode(CpType cp, LogConfig::LogMode mode);
  void enable_md(bool en = true);

  const MipiLogList& get_mipi_log_info() {return m_mipilog;}
  void set_mipi_log_info(MipiLogList mipi_log) {
    m_mipilog = mipi_log;
    m_dirty = true;
  }

  void enable_iq(CpType cpt, IqType it);
  /*  disable_iq - disable I/Q saving.
   *  @cpt: CP type
   *  @it: I/Q type. IT_ALL for all I/Q types.
   *
   */
  void disable_iq(CpType cpt, IqType it);

  bool wcdma_iq_enabled(CpType cpt) const;

  bool use_work_conf_file() const {
    return use_work_conf_file_;
  }

#ifdef SUPPORT_AGDSP
  // AG-DSP options
  AgDspLogDestination agdsp_log_dest() const {
    return m_agdsp_log_dest;
  }
  void set_agdsp_log_dest(AgDspLogDestination dest);
  bool agdsp_pcm_dump_enabled() const {
    return m_agdsp_pcm_dump;
  }
  void enable_agdsp_pcm_dump(bool enable);
#endif

  static CpType get_wan_modem_type() { return s_wan_modem_type; }

  static IqType get_iq_type(const uint8_t* iq, size_t len);
  static int parse_enable_disable(const uint8_t* buf, bool& en);
  static int parse_number(const uint8_t* buf, size_t& num);
  static const char* cp_type_to_name(CpType t);

 private:
  struct IqConfig {
    CpType cp_type;
    bool gsm_iq;
    bool wcdma_iq;
  };

  bool m_dirty;
  // Internal storage's path preferred
  LogString m_internal_path;
  // External storage's mount point
  LogString m_mount_point;
  LogString m_config_file;
  ConfigList m_config;
  bool m_enable_md;
  // true, if minidump storage position is internal
  bool m_md_save_to_int;
  // I/Q config
  LogList<IqConfig*> m_iq_config;
#ifdef SUPPORT_AGDSP
  // AG-DSP log settings
  AgDspLogDestination m_agdsp_log_dest;
  bool m_agdsp_pcm_dump;
#endif
  LogString m_initial_config_file;
  // true, if external storage use is active
  bool ext_stor_active_;

  bool use_work_conf_file_;

  MipiLogList m_mipilog;

  // Cellular MODEM type
  static CpType s_wan_modem_type;

  char version[PROPERTY_VALUE_MAX];
  char oper_rst[PROPERTY_VALUE_MAX];

  int parse_cmd_line(int argc, char** argv);
  int parse_config_file(const char* config_file_path);

  int parse_line(const uint8_t* buf);
  int parse_stream_line(const uint8_t* buf);
  int parse_iq_line(const uint8_t* buf);
  int parse_minidump_line(const uint8_t* buf, bool& en,
                          bool& save_to_int);
  int parse_mipilog_line(const uint8_t* buf, MipiLogList& mipi_log);
  int parse_storage_line(const uint8_t* buf);


  void change_modem_log_dest(unsigned num);
  void change_wcn_log_dest(unsigned num);

  static void propget_wan_modem_type();
  static CpType get_modem_type(const uint8_t* name, size_t len);
  static ConfigList::iterator find(ConfigList& clist, CpType t);
  static LogList<IqConfig*>::iterator find_iq(LogList<IqConfig*>& iq_list,
                                              CpType t);
  static LogList<IqConfig*>::const_iterator find_iq(
      const LogList<IqConfig*>& iq_list, CpType t);

#ifdef SUPPORT_AGDSP
  static int parse_agdsp_log_dest(const uint8_t* buf,
                                  AgDspLogDestination& dest);
#endif
  static int parse_on_off(const uint8_t* buf, bool& on_off);
  static int get_log_mode(const uint8_t* tok, size_t len,
                          LogMode& lm);
  static const char* log_mode_to_string(LogMode mode);
};

int get_dev_paths(CpType t, bool& same, LogString& log_path,
                  LogString& diag_path);

#endif  // !LOG_CONFIG_H_
