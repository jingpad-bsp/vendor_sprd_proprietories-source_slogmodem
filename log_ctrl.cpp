/*
 *  log_ctrl.cpp - The log controller class implementation.
 *
 *  Copyright (C) 2015-2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2015-2-16 Zhang Ziyi
 *  Initial version.
 *
 *  2017-6-6 Zhang Ziyi
 *  Transaction requests added.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HOST_TEST_
#include "prop_test.h"
#else
#include <cutils/properties.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>

#ifdef SUPPORT_AGDSP
#include "agdsp_log.h"
#endif
#include "pm_sensorhub_log.h"
#include "cp_dir.h"
#include "cp_log_cmn.h"
#include "cp_stor.h"
#include "def_config.h"
#include "ext_gnss_log.h"
#ifdef EXTERNAL_WCN
#include "ext_wcn_log.h"
#else
#include "int_wcn_log.h"
#endif
#include "log_ctrl.h"
#include "req_err.h"
#include "trans_disable_log.h"
#include "trans_enable_log.h"
#include "trans_log_col.h"
#include "trans_wcn_last_log.h"
#include "wan_modem_log.h"
#include "orca_miniap_log.h"
#include "orca_dp_log.h"

LogController::LogController()
    : m_config{nullptr},
      m_cli_mgr{nullptr},
      m_modem_state{nullptr},
      m_wcn_state{nullptr},
      f_rate_statistic_{-1},
      convey_workshop_{nullptr} {}
LogController::~LogController() {
  delete m_wcn_state;
  delete m_modem_state;
  clear_ptr_container(m_log_pipes);
  delete m_cli_mgr;
#ifdef SUPPORT_RATE_STATISTIC
  close();
#endif
  delete convey_workshop_;
}

LogPipeHandler* LogController::create_handler(const LogConfig::ConfigEntry* e) {
  // First get log device files
  bool same;
  LogString log_path;
  LogString diag_path;
  int err = get_dev_paths(e->type, same, log_path, diag_path);

  if (err) {
    err_log("Can not get log/diag path for %s", ls2cstring(e->modem_name));
    return nullptr;
  }

  LogPipeHandler* log_pipe = nullptr;
  CpType cp_type = e->type;
  const char* proc_mem = ""; //Coverity ID:214695

  switch (cp_type) {
#ifdef SUPPORT_WCN
    case CT_WCN:
  #ifdef EXTERNAL_WCN
      log_pipe = new ExtWcnLogHandler(this, &m_multiplexer,
                                      e, m_stor_mgr, CLASS_CONNECTIVITY);
  #else
      log_pipe = new IntWcnLogHandler(this, &m_multiplexer, e, m_stor_mgr,
                                      "/proc/cpwcn/mem", CLASS_CONNECTIVITY);
  #endif
      break;
#endif
    case CT_GNSS:
      log_pipe = new ExtGnssLogHandler(this, &m_multiplexer, e,
                                       m_stor_mgr, CLASS_CONNECTIVITY);
      break;
#ifdef SUPPORT_AGDSP
    case CT_AGDSP:
      log_pipe = new AgDspLogHandler(this, &m_multiplexer, e, m_stor_mgr,
                                     CLASS_AUDIO);
      break;
#endif
    case CT_PM_SH:
      log_pipe = new PmSensorhubLogHandler(this, &m_multiplexer, e, m_stor_mgr,
                                           CLASS_SENSOR);
      break;
    case CT_TD:
      cp_type = CT_WANMODEM;
      proc_mem = "/proc/cpt/mem";
      break;
    case CT_WCDMA:
      cp_type = CT_WANMODEM;
      proc_mem = "/proc/cpw/mem";
      break;
    case CT_3MODE:
    case CT_4MODE:
    case CT_5MODE:
      cp_type = CT_WANMODEM;
      proc_mem = "/proc/cptl/mem";
      break;
    case CT_ORCA_MINIAP:
    case CT_ORCA_DP: {
      auto it = find_log_handler(m_log_pipes, m_config->get_wan_modem_type());
      WanModemLogHandler* mlog;
      if (it == m_log_pipes.end()) {
        mlog = nullptr;
      } else {
        mlog = static_cast<WanModemLogHandler*>(*it);
      }
      if (cp_type == CT_ORCA_MINIAP) {
        log_pipe = new OrcaMiniapLogHandler(this, &m_multiplexer, e, m_stor_mgr,
                                            CLASS_MODEM, mlog);
      } else {
        log_pipe = new OrcaDpLogHandler(this, &m_multiplexer, e, m_stor_mgr,
                                         CLASS_MODEM, mlog);
      }
      break;
    }
    default:
      break;
  }

  if ((cp_type == CT_WANMODEM) && (e->type == LogConfig::get_wan_modem_type())) {
#ifdef HOST_TEST_
    proc_mem = "/data/local/tmp/mem";
#endif
    log_pipe = new WanModemLogHandler(this, &m_multiplexer, e, m_stor_mgr,
                                      proc_mem, CLASS_MODEM);

    WanModemLogHandler* wan_cp = static_cast<WanModemLogHandler*>(log_pipe);
    // If WCDMA I/Q is enabled, set it.
    if (m_config->wcdma_iq_enabled(e->type)) {
      wan_cp->enable_wcdma_iq();
    }

#ifdef POWER_ON_PERIOD
    wan_cp->set_poweron_period(POWER_ON_PERIOD);
#endif
    wan_cp->set_md_save(m_config->md_enabled());
    wan_cp->set_md_internal(m_config->md_save_to_int());
#ifdef SUPPORT_AGDSP
    wan_cp->set_agdsp_pcm_dump(m_config->agdsp_pcm_dump_enabled());
    wan_cp->set_agdsp_log_dest(m_config->agdsp_log_dest());
#endif
  }

  if (log_pipe) {
    char version[PROPERTY_VALUE_MAX];
    property_get("ro.build.type", version, "");
    //User version and log configuration has not changed, limit log storage space.
    if (!strcmp(version, "user") && !config()->use_work_conf_file()) {
      log_pipe->reset_external_max_size();
    }
    if (same) {
      info_log("CP %s dev %s", ls2cstring(e->modem_name),
               ls2cstring(log_path));
      log_pipe->set_log_diag_dev_path(log_path);
    } else {
      info_log("CP %s dev %s %s", ls2cstring(e->modem_name),
               ls2cstring(log_path), ls2cstring(diag_path));
      log_pipe->set_log_diag_dev_path(log_path, diag_path);
    }

    if (CT_WANMODEM == cp_type) {  // Cellular MODEM
      if (LogConfig::LM_OFF != e->mode) {
        err = log_pipe->start(e->mode);
        if (err < 0) {
          err_log("start %s log mode %d failed",
                  ls2cstring(e->modem_name), e->mode);
        }
      }
    } else {  // Non cellular MODEM
      if (LogConfig::LM_NORMAL == e->mode) {
        err = log_pipe->start();
        if (err < 0) {
          err_log("start %s log failed", ls2cstring(e->modem_name));
        }
      }
    }
  }

  return log_pipe;
}

int LogController::init(LogConfig* config) {
  m_config = config;

  // Initialize the storage manager
  int err = m_stor_mgr.init(m_config->get_internal_path(),
                            m_config->get_mount_point(),
                            m_config->int_stor_active(),
                            m_config->ext_stor_active(),
                            this, &m_multiplexer);
  if (err < 0) {
    err_log("init storage manager error");
    return -1;
  }

  convey_workshop_ = new ConveyWorkshop(this, &m_stor_mgr,
                                        &m_multiplexer);
  convey_workshop_->init();

  // Create LogPipeHandlers according to settings in config
  // And add them to m_log_pipes
  const LogConfig::ConfigList& conf_list = config->get_conf();
  for (auto it = conf_list.begin(); it != conf_list.end(); ++it) {
    LogConfig::ConfigEntry* pentry = *it;
    LogPipeHandler* handler = create_handler(pentry);
    if (handler) {
      m_log_pipes.push_back(handler);
#ifdef SUPPORT_AGDSP
      // Initialize AG-DSP log
      if (CT_AGDSP == handler->type() &&
          !str_empty(handler->log_dev_path())) {
        info_log("init AG-DSP log output: %d, %s",
                 static_cast<int>(config->agdsp_log_dest()),
                 config->agdsp_pcm_dump_enabled() ? "ON" : "OFF");
        AgDspLogHandler* agdsp = static_cast<AgDspLogHandler*>(handler);
        if (agdsp->init_log_output(config->agdsp_log_dest(),
                                   config->agdsp_pcm_dump_enabled())) {
          err_log("AG-DSP log init error");
        }
      }
#endif
    }
  }

  // Server socket for clients (Engineering mode)
  m_cli_mgr = new ClientManager(this, &m_multiplexer, 5);
  if (m_cli_mgr->init(SLOG_MODEM_SERVER_SOCK_NAME) < 0) {
    delete m_cli_mgr;
    m_cli_mgr = nullptr;
  }

  // Connection to modemd
  m_modem_state = init_state_handler(MODEM_SOCKET_NAME);

#ifdef SUPPORT_WCN
  // Connection to wcn state reporter (wcnd/download)
  m_wcn_state = init_wcn_state_handler(WCN_SOCKET_NAME, WCN_ASSERT_MESSAGE);
#endif

#ifdef SUPPORT_RATE_STATISTIC
  if (open_ratefile()) {
    err_log("init rate_statistic file error");
  }
#endif

  return 0;
}

ModemStateHandler* LogController::init_state_handler(const char* serv_name) {
  auto it = find_log_handler(m_log_pipes, m_config->get_wan_modem_type());
  if (it == m_log_pipes.end()) {
    return nullptr;
  }
  WanModemLogHandler* mlog = static_cast<WanModemLogHandler*>(*it);
  ModemStateHandler* p = new ModemStateHandler{this,
                                               &m_multiplexer,
                                               serv_name,
                                               mlog};

  int err = p->init();
  if (err < 0) {
    delete p;
    p = nullptr;
    err_log("connect to %s error", serv_name);
  }

  return p;
}

WcndStateHandler* LogController::init_wcn_state_handler(const char* serv_name,
                                                        const char* assert_msg) {
  WcndStateHandler* p =
      new WcndStateHandler(this, &m_multiplexer, serv_name, assert_msg);

  int err = p->init();
  if (err < 0) {
    delete p;
    p = nullptr;
    err_log("connect to %s error", serv_name);
  }

  return p;
}

int LogController::run() { return m_multiplexer.run(); }

void LogController::process_cp_evt(CpType type, CpStateHandler::CpEvent evt,
                        LogString assert_info, size_t assert_info_len) {
  LogList<LogPipeHandler*>::iterator it;

  if (CT_WANMODEM == type) {
    it = find_log_handler(m_log_pipes, m_config->get_wan_modem_type());
  } else {
    it = find_log_handler(m_log_pipes, type);
  }

  if (it == m_log_pipes.end()) {
    err_log("unknown MODEM type %d", type);
    return;
  }

  LogPipeHandler* p = *it;

  if (evt == CpStateHandler::CE_ALIVE) {
    p->process_alive();
  } else if (evt == CpStateHandler::CE_RESET) {
    p->process_reset();
  } else if ((evt == CpStateHandler::CE_ASSERT) ||
             (evt == CpStateHandler::CE_ASSERT_DP) ||
             (evt == CpStateHandler::CE_BLOCKED) ||
             (evt == CpStateHandler::CE_PANIC)) {
    time_t t;
    struct tm lt;
    t = time(0);
    if (static_cast<time_t>(-1) == t || !localtime_r(&t, &lt)) {
      err_log("fail to get system time");
    } else {
      p->set_assert_info(assert_info, assert_info_len);
      p->process_assert(lt, evt);
    }
  } else {
    err_log("unknown event %d", static_cast<int>(evt));
  }
}

int LogController::save_sipc_dump(CpStorage* stor, const struct tm& lt) {
  char fn[64];
  int year = lt.tm_year + 1900;
  int mon = lt.tm_mon + 1;
  int ret = -1;

  // /d/sipc/smsg
  snprintf(fn, sizeof fn, "smsg_%04d%02d%02d-%02d%02d%02d.log",
           year, mon, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
  int err = 0;

  auto f = stor->create_file(LogString(fn), LogFile::LT_SIPC).lock();
  if (f) {
    ret = f->copy(DEBUG_SMSG_PATH);
    f->close();
    if (ret) {
      err_log("save SIPC smsg info failed");
      f->dir()->remove(f);
      err = -1;
    }
  }

  // /d/sipc/sbuf
  snprintf(fn, sizeof fn, "sbuf_%04d%02d%02d-%02d%02d%02d.log",
           year, mon, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
  f = stor->create_file(LogString(fn), LogFile::LT_SIPC).lock();
  if (f) {
    ret = f->copy(DEBUG_SBUF_PATH);
    f->close();
    if (ret) {
      err_log("save SIPC sbuf info failed");
      f->dir()->remove(f);
      err = -1;
    }
  }

  // /d/sipc/sblock
  snprintf(fn, sizeof fn, "sblock_%04d%02d%02d-%02d%02d%02d.log",
           year, mon, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
  f = stor->create_file(LogString(fn), LogFile::LT_SIPC).lock();
  if (f) {
    ret = f->copy(DEBUG_SBLOCK_PATH);
    f->close();
    if (ret) {
      err_log("save SIPC sblock info failed");
      f->dir()->remove(f);
      err = -1;
    }
  }

  // /d/sipc/mbox
  snprintf(fn, sizeof fn, "mailbox_%04d%02d%02d-%02d%02d%02d.log",
           year, mon, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
  f = stor->create_file(LogString(fn), LogFile::LT_SIPC).lock();
  if (f) {
    ret = f->copy(DEBUG_MAILBOX_PATH);
    f->close();
    if (ret) {
      err_log("save SIPC mailbox info failed");
      f->dir()->remove(f);
      err = -1;
    }
  }

  return err;
}

int LogController::save_etb(CpStorage* stor, const struct tm& lt) {
  int ret = access(ETB_FILE_PATH, R_OK);
  if (-1 == ret) {
    if (ENOENT != errno) {
      err_log("open %s error", ETB_FILE_PATH);
    }
    return -1;
  }

  if (stor->check_quota()) {
    return -1;
  }

  char fn[64];
  int year = lt.tm_year + 1900;
  int mon = lt.tm_mon + 1;

  snprintf(fn, sizeof fn, "etb_%04d%02d%02d-%02d%02d%02d.log",
           year, mon, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);

  ret = -1;
  auto f = stor->create_file(LogString(fn), LogFile::LT_UNKNOWN).lock();
  if (f) {
    ret = f->copy(ETB_FILE_PATH);
    f->close();
    if (ret) {  // Copy failed
      f->dir()->remove(f);
      err_log("save ETB info failed");
    } else if (!f->size()) {  // ETB info file is empty. Not error.
      f->dir()->remove(f);
      info_log("ETB info size is 0");
    }
  }

  return ret;
}

LogList<LogPipeHandler*>::iterator LogController::find_log_handler(
    LogList<LogPipeHandler*>& handlers, CpType t) {
  LogList<LogPipeHandler*>::iterator it;

  for (it = handlers.begin(); it != handlers.end(); ++it) {
    if (t == (*it)->type()) {
      break;
    }
  }

  return it;
}

LogList<LogPipeHandler*>::const_iterator LogController::find_log_handler(
    const LogList<LogPipeHandler*>& handlers, CpType t) {
  LogList<LogPipeHandler*>::const_iterator it;

  for (it = handlers.begin(); it != handlers.end(); ++it) {
    if (t == (*it)->type()) {
      break;
    }
  }

  return it;
}

LogList<LogPipeHandler*>::iterator LogController::find_log_handler(
    LogList<LogPipeHandler*>& handlers, CpClass cls) {
  LogList<LogPipeHandler*>::iterator it;

  for (it = handlers.begin(); it != handlers.end(); ++it) {
    if (cls == (*it)->cp_class()) {
      break;
    }
  }

  return it;
}

LogList<LogPipeHandler*>::const_iterator LogController::find_log_handler(
    const LogList<LogPipeHandler*>& handlers, CpClass cls) {
  LogList<LogPipeHandler*>::const_iterator it;

  for (it = handlers.begin(); it != handlers.end(); ++it) {
    if (cls == (*it)->cp_class()) {
      break;
    }
  }

  return it;
}

int LogController::enable_log(const CpType* cps, size_t num,
                              Transaction::ResultCallback cb,
                              void* client,
                              TransEnableLog*& trans) {
  TransEnableLog* en_log = new TransEnableLog{this};
  en_log->set_client(client, cb);

  size_t i;

  for (i = 0; i < num; ++i) {
    LogList<LogPipeHandler*>::iterator it =
        find_log_handler(m_log_pipes, cps[i]);
    if (it != m_log_pipes.end()) {
      LogPipeHandler* p = *it;
      if (!str_empty(p->log_dev_path())) {
        if (LogConfig::LM_NORMAL != p->log_mode()) {
          en_log->add(p);
        }
      }
    }
  }

  int ret {Transaction::TRANS_E_ERROR};

  if (!number()) {  // Start now.
    ret = en_log->execute();
    switch (ret) {
      case Transaction::TRANS_E_SUCCESS:
        trans = en_log;
        break;
      case Transaction::TRANS_E_STARTED:
        enqueue(en_log);
        trans = en_log;
        break;
      default:  // Error
        delete en_log;
        break;
    }
  } else {  // Enqueue
    enqueue(en_log);
    trans = en_log;
    ret = Transaction::TRANS_E_STARTED;
  }

  return ret;
}

int LogController::disable_log(const CpType* cps, size_t num,
                               Transaction::ResultCallback cb,
                               void* client,
                               TransDisableLog*& trans) {
  TransDisableLog* dis_log = new TransDisableLog{this};
  dis_log->set_client(client, cb);

  size_t i;

  for (i = 0; i < num; ++i) {
    LogList<LogPipeHandler*>::iterator it =
        find_log_handler(m_log_pipes, cps[i]);
    if (it != m_log_pipes.end()) {
      LogPipeHandler* p = *it;
      if (LogConfig::LM_OFF != p->log_mode()) {
        dis_log->add(p);
      }
    }
  }

  int ret {Transaction::TRANS_E_ERROR};

  if (!number()) {  // Start now.
    ret = dis_log->execute();
    switch (ret) {
      case Transaction::TRANS_E_SUCCESS:
        trans = dis_log;
        break;
      case Transaction::TRANS_E_STARTED:
        enqueue(dis_log);
        trans = dis_log;
        break;
      default:  // Error
        delete dis_log;
        break;
    }
  } else {  // Enqueue
    enqueue(dis_log);
    trans = dis_log;
    ret = Transaction::TRANS_E_STARTED;
  }

  return ret;
}

LogConfig::LogMode LogController::get_log_state(CpType ct) const {
  LogConfig::LogMode lm = LogConfig::LM_OFF;
  auto it = find_log_handler(m_log_pipes, ct);

  if (it != m_log_pipes.end()) {
    lm = (*it)->log_mode();
  }

  return lm;
}

int LogController::enable_md() {
  if (!m_config->md_enabled()) {
    WanModemLogHandler* cp =
      static_cast<WanModemLogHandler*>(get_cp(m_config->get_wan_modem_type()));
    if (!cp) {
      return LCR_CP_NONEXISTENT;
    }

    cp->set_md_save(true);
    m_config->enable_md();
    if (m_config->save()) {
      err_log("save config file failed");
    }
  }

  return LCR_SUCCESS;
}

int LogController::disable_md() {
  if (m_config->md_enabled()) {
    WanModemLogHandler* cp =
      static_cast<WanModemLogHandler*>(get_cp(m_config->get_wan_modem_type()));
    if (!cp) {
      return LCR_CP_NONEXISTENT;
    }

    cp->set_md_save(false);
    m_config->enable_md(false);
    if (m_config->save()) {
      err_log("save config file failed");
    }
  }

  return LCR_SUCCESS;
}

int LogController::mini_dump() {
  time_t t = time(0);
  struct tm lt;

  if (static_cast<time_t>(-1) == t || !localtime_r(&t, &lt)) {
    return -1;
  }
  // TODO: use m_stor_mgr to save file
  return -1;
}

size_t LogController::get_log_file_size(CpType ct, size_t& size) const {
  int ret = LCR_SUCCESS;

  auto it = find_log_handler(m_log_pipes, ct);

  if (it != m_log_pipes.end()) {
    size = static_cast<size_t>(((*it)->get_max_log_file_size()) >> 20);
  } else {
    ret = LCR_CP_NONEXISTENT;
  }

  return ret;
}

int LogController::set_log_file_size(CpType ct, size_t len) {
  int ret = LCR_SUCCESS;

  auto it = find_log_handler(m_log_pipes, ct);

  if (it != m_log_pipes.end()) {
    (*it)->set_max_log_file_size(static_cast<uint64_t>(len) << 20);
    m_config->set_cp_log_file_size(ct, len);
    if (m_config->save()) {
      err_log("save config file failed");
    }
  } else {
    err_log("Not supported cp type");
    ret = LCR_CP_NONEXISTENT;
  }

  return ret;
}

int LogController::set_log_overwrite(CpType ct, bool en) {
  int ret = LCR_SUCCESS;

  auto it = find_log_handler(m_log_pipes, ct);
  if (it != m_log_pipes.end()) {
    (*it)->set_overwrite(en);
    m_config->set_cp_log_overwrite(ct, en);
    if (m_config->dirty()) {
      if (m_config->save()) {
        err_log("save config file failed");
      }
    }
  } else {
    err_log("Not supported cp type");
    ret = LCR_CP_NONEXISTENT;
  }

  return 0;
}

bool LogController::use_external_storage() {
  return m_config->ext_stor_active();
}

int LogController::active_external_storage(bool active) {
  if (active != m_config->ext_stor_active()) {
    m_stor_mgr.enable_media(StorageManager::MT_EXT_STOR, active);
    m_config->set_external_stor(active);
    if (m_config->dirty()) {
      if (m_config->save()) {
        err_log("save config file failed");
      }
    }
  }

  return LCR_SUCCESS;
}

bool LogController::get_md_int_stor() const {
  return m_config->md_save_to_int();
}

int LogController::set_md_int_stor(bool md_int_stor) {
  if (md_int_stor != m_config->md_save_to_int()) {
    WanModemLogHandler* cp =
      static_cast<WanModemLogHandler*>(get_cp(m_config->get_wan_modem_type()));
    if (!cp) {
      return LCR_CP_NONEXISTENT;
    }

    cp->set_md_internal(md_int_stor);
    m_config->set_md_int_stor(md_int_stor);

    if (m_config->dirty()) {
      if (m_config->save()) {
        err_log("save config file failed");
      }
    }
  }

  return LCR_SUCCESS;
}

int LogController::get_cp_log_size(CpType ct,
    StorageManager::MediaType mt, size_t& sz) const {
  int ret {LCR_SUCCESS};

  auto it = find_log_handler(m_log_pipes, ct);

  if (it != m_log_pipes.end()) {
    sz = ((*it)->log_capacity(mt)) >> 20;
  } else {
    ret = LCR_CP_NONEXISTENT;
  }

  return ret;
}

int LogController::set_cp_log_size(CpType ct,
                                   StorageManager::MediaType mt, size_t sz) {
  int ret = LCR_SUCCESS;

  auto it = find_log_handler(m_log_pipes, ct);

  if (it != m_log_pipes.end()) {
    (*it)->set_log_capacity(mt, static_cast<uint64_t>(sz) << 20);
    m_config->set_cp_storage_limit(ct, mt, sz);
    if (m_config->save()) {
      err_log("save config file failed");
    }
  } else {
    ret = LCR_CP_NONEXISTENT;
  }

  return ret;
}

int LogController::get_log_overwrite(CpType ct, bool& en) const {
  int ret = LCR_SUCCESS;
  auto it = find_log_handler(m_log_pipes, ct);

  if (it != m_log_pipes.end()) {
    m_config->get_cp_log_overwrite(ct, en);
  } else {
    ret = LCR_CP_NONEXISTENT;
  }

  return ret;
}

int LogController::clear_log() {
  LogList<LogPipeHandler*>::iterator it;

  for (it = m_log_pipes.begin(); it != m_log_pipes.end(); ++it) {
    if ((*it)->in_transaction()) {
      return LCR_IN_TRANSACTION;
    }
  }

  // Now we can pause log
  for (it = m_log_pipes.begin(); it != m_log_pipes.end(); ++it) {
    LogPipeHandler* cp = *it;

    if (cp->enabled()) {
      cp->pause();
      info_log("clear_log(): pause log,cp_type = %d", cp->type());
    }
  }

  convey_workshop_->clear();
  info_log("clear_log(): workshop clear");
  m_stor_mgr.clear();
  info_log("clear_log(): stor_mgr clear");

  // Now we can resume log
  for (it = m_log_pipes.begin(); it != m_log_pipes.end(); ++it) {
    LogPipeHandler* cp = *it;

    if (cp->enabled()) {
      cp->resume();
      info_log("clear_log(): resume log,cp_type = %d", cp->type());
    }
  }

  return LCR_SUCCESS;
}

LogPipeHandler* LogController::get_cp(CpType type) {
  LogList<LogPipeHandler*>::iterator it = find_log_handler(m_log_pipes, type);
  LogPipeHandler* p = 0;
  if (it != m_log_pipes.end()) {
    p = (*it);
  }
  return p;
}

LogPipeHandler* LogController::get_generic_cp(CpType type) {
  if (CT_WANMODEM == type) {
    type = m_config->get_wan_modem_type();
  }

  auto it = find_log_handler(m_log_pipes, type);
  LogPipeHandler* cp{nullptr};
  if (it != m_log_pipes.end()) {
    cp = *it;
  }
  return cp;
}

int LogController::enable_wcdma_iq(CpType cpt) {
  if ((cpt == CT_WCN) || (cpt == CT_PM_SH) ||
      (cpt == CT_GNSS) || (cpt == CT_AGDSP)) {
    err_log("enable WCDMA I/Q on wrong CP %d", cpt);
    return LCR_ERROR;
  }

  WanModemLogHandler* cp = static_cast<WanModemLogHandler*>(get_cp(cpt));
  if (!cp) {
    return LCR_CP_NONEXISTENT;
  }

  int err = cp->enable_wcdma_iq();
  if (LCR_SUCCESS == err) {
    m_config->enable_iq(cpt, IT_WCDMA);
    m_config->save();
  }

  return err;
}

int LogController::disable_wcdma_iq(CpType cpt) {
  if ((cpt == CT_WCN) || (cpt == CT_PM_SH) ||
      (cpt == CT_GNSS) || (cpt == CT_AGDSP)) {
    err_log("disable WCDMA I/Q on wrong CP %d", cpt);
    return LCR_ERROR;
  }

  WanModemLogHandler* cp = static_cast<WanModemLogHandler*>(get_cp(cpt));
  if (!cp) {
    return LCR_CP_NONEXISTENT;
  }

  int err = cp->disable_wcdma_iq();
  if (LCR_SUCCESS == err) {
    m_config->disable_iq(cpt, IT_WCDMA);
    m_config->save();
  }

  return err;
}

int LogController::copy_cp_files(CpType cpt, LogFile::LogType file_type,
                                 const LogVector<LogString>& src_files) {
  int ret = LCR_CP_NONEXISTENT;
  LogPipeHandler* cp = get_cp(cpt);

  if (cp) {
    ret = cp->copy_files(file_type, src_files);
  }

  return ret;
}

int LogController::flush() {
  LogList<LogPipeHandler*>::iterator it;

  for (it = m_log_pipes.begin(); it != m_log_pipes.end(); ++it) {
    LogPipeHandler* cp = *it;
    if (cp->enabled()) {
      cp->flush();
    }
  }

  //Debug
#if 0
  it = find_log_handler(m_log_pipes, LogConfig::get_wan_modem_type());
  if (it != m_log_pipes.end()) {
    WanModemLogHandler* modem = static_cast<WanModemLogHandler*>(*it);
    WanModemLogHandler::modem_event_notify(modem, 0, 1024);
  }
#endif

  return LCR_SUCCESS;
}

int LogController::start_log_collect(const ModemSet& cps,
                                     Transaction::ResultCallback cb,
                                     void* client,
                                     TransLogCollect*& trans) {
  TransLogCollect* log_col = new TransLogCollect{this, cps};

  log_col->set_client(client, cb);
  int ret = log_col->execute();
  switch (ret) {
    case Transaction::TRANS_E_STARTED:
      enqueue(log_col);
      trans = log_col;
      break;
    case Transaction::TRANS_E_SUCCESS:
      trans = log_col;
      break;
    default:  // TRANS_E_ERROR
      delete log_col;
      break;
  }

  return ret;
}

int LogController::start_event_log(CpType ct,
                                   Transaction::ResultCallback cb,
                                   void* client,
                                   TransStartEventLog*& trans) {
  LogList<LogPipeHandler*>::iterator it =
      find_log_handler(m_log_pipes, ct);
  int ret {Transaction::TRANS_E_ERROR};

  if (it != m_log_pipes.end()) {
    WanModemLogHandler* modem = static_cast<WanModemLogHandler*>(*it);
    ret = modem->start_event_log(client, cb, trans);
  }

  return ret;
}

int LogController::save_wcn_last_log(Transaction::ResultCallback cb,
                                     void* client,
                                     TransWcnLastLog*& trans) {
  LogList<LogPipeHandler*>::iterator it =
      find_log_handler(m_log_pipes, CT_WCN);
  int ret {Transaction::TRANS_E_ERROR};

  if (it != m_log_pipes.end()) {
    LogPipeHandler* wcn = *it;
    TransWcnLastLog* last_log{new TransWcnLastLog{this, wcn}};
    last_log->set_client(client, cb);

    if (!number()) {  // Start now.
      ret = last_log->execute();
      switch (ret) {
        case Transaction::TRANS_E_SUCCESS:
          trans = last_log;
          break;
        case Transaction::TRANS_E_STARTED:
          enqueue(last_log);
          trans = last_log;
          break;
        default:  // Error
          delete last_log;
          break;
      }
    } else {  // Enqueue
      enqueue(last_log);
      trans = last_log;
      ret = Transaction::TRANS_E_STARTED;
    }
  }

  return ret;
}

int LogController::open_ratefile() {
  struct stat statbuff;
  LogString test_file_path = LogString(DATA_PATH);
  test_file_path += "/ylog/rate_statistic.txt";
  const char * test_file = ls2cstring(test_file_path);

  f_rate_statistic_ = open(test_file, O_RDWR | O_CREAT | O_APPEND,
                           S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (f_rate_statistic_ < 0) {
    err_log("creat rate_statistic file error");
    return -1;
  } else {
    info_log("creat rate_statistic file ok %d", f_rate_statistic_);
  }
  if (-1 == stat(test_file, &statbuff)) {
    err_log("stat wrong");
    return -1;
  } else {
    if (statbuff.st_size >= 614400) {
      ftruncate(f_rate_statistic_,0);
    }
  }

  return 0;
}

int LogController::close() {
  int ret = 0;

  if (f_rate_statistic_ >= 0) {
    ret = ::close(f_rate_statistic_);
    f_rate_statistic_ = -1;
  }

  return ret;
}
