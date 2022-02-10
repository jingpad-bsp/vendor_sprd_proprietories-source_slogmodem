/*
 *  client_hdl.cpp - The base class implementation for file descriptor
 *                   handler.
 *
 *  Copyright (C) 2015-2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-2-16 Zhang Ziyi
 *  Initial version.
 *
 *  2015-6-5 Zhang Ziyi
 *  CP dump notification added.
 *
 *  2015-7-22 Zhang Ziyi
 *  I/Q commands added.
 *
 *  2017-5-16 Zhang Ziyi
 *  CP dump notification changed to the new
 *  LogPipeHandler/register_dump_{start,end} API;
 *  COLLECT_LOG command added.
 *
 *  2017-5-26 Zhang Ziyi
 *  Add ENABLE_EVT_LOG command.
 *
 *  2017-8-9 Zhang Ziyi
 *  Port SAVE_LAST_LOG command from sprdroid7.0_trunk branch.
 */

#include <cstring>
#include <poll.h>
#include <unistd.h>

#include "client_hdl.h"
#include "client_mgr.h"
#include "client_req.h"
#include "log_ctrl.h"
#include "log_pipe_hdl.h"
#include "parse_utils.h"
#include "req_err.h"
#include "trans_disable_log.h"
#include "trans_enable_log.h"
#include "trans_last_log.h"
#include "trans_log_col.h"
#include "trans_save_sleep_log.h"
#include "trans_start_evt_log.h"
#include "trans_wcn_last_log.h"
#include "wan_modem_log.h"

ClientHandler::ClientHandler(int sock, LogController* ctrl,
                             Multiplexer* multiplexer, ClientManager* mgr)
    : DataProcessHandler(sock, ctrl, multiplexer, CLIENT_BUF_SIZE),
      m_mgr(mgr),
      m_trans_type{CTT_UNKNOWN},
      m_state{CTS_IDLE},
      m_cp{nullptr},
      cur_trans_{nullptr} {
  for (auto& subs: dump_subs_) {
    subs.handler = this;
    subs.cp_log = nullptr;
  }
}

ClientHandler::~ClientHandler() {
  // Cancel transactions
  if (CTS_IDLE != m_state) {
    cancel_trans();
  }

  for (auto& subs: dump_subs_) {
    LogPipeHandler* cp_log = subs.cp_log;
    if (cp_log) {
      cp_log->unregister_dump_start(&subs);
      cp_log->unregister_dump_end(&subs);
      subs.cp_log = nullptr;
    }
  }

  if (cur_trans_) {
    cur_trans_->cancel();
    delete cur_trans_;
  }
}

const uint8_t* ClientHandler::search_end(const uint8_t* req, size_t len) {
  const uint8_t* endp = req + len;

  while (req < endp) {
    if ('\0' == *req || '\n' == *req) {
      break;
    }
    ++req;
  }

  if (req == endp) {
    req = 0;
  }

  return req;
}

int ClientHandler::process_data() {
  const uint8_t* start = m_buffer.buffer;
  const uint8_t* end = m_buffer.buffer + m_buffer.data_len;
  size_t rlen = m_buffer.data_len;
  info_log("process_data start = %s, rlen=%d",start, rlen);

  while (start < end) {
    const uint8_t* p1 = search_end(start, rlen);
    if (!p1) {  // Not complete request
      err_log("Not complete request, p1=%p.", p1);
      show_memory_info(m_buffer.buffer, m_buffer.buf_size);
      break;
    }
    process_req(start, p1 - start);
    start = p1 + 1;
    rlen = end - start;
  }

  if (rlen && start != m_buffer.buffer) {
    memset(m_buffer.buffer, '\0', m_buffer.buf_size);
    m_buffer.data_len = 0;
    m_buffer.data_start = 0;
  }
  m_buffer.data_len = rlen;

  return 0;
}

void ClientHandler::process_req(const uint8_t* req, size_t len) {
  const uint8_t* req_start = req;
  size_t orig_len = len;
  size_t tok_len;
  const uint8_t* endp = req + len;
  const uint8_t* token = get_token(req, len, tok_len);

  if (!token) {
    // Empty line: ignore it.
    err_log("Empty line: ignore it.");
    show_memory_info(m_buffer.buffer, m_buffer.buf_size);
    return;
  }

  // What request?
  bool known_req = false;

  req = token + tok_len;
  len = endp - req;
  switch (tok_len) {
    case 5:
      if (!memcmp(token, "FLUSH", 5)) {
        proc_flush(req, len);
        known_req = true;
      }
      break;
    case 7:
      if (!memcmp(token, "slogctl", 7)) {
        proc_slogctl(req, len);
        known_req = true;
      }
      break;
    case 9:
      if (!memcmp(token, "COPY_FILE", 9)) {
        proc_copy_file(req, len);
        known_req = true;
      } else if (!memcmp(token, "ENABLE_IQ", 9)) {
        proc_enable_iq(req, len);
        known_req = true;
      } else if (!memcmp(token, "ENABLE_MD", 9)) {
        proc_enable_md(req, len);
        known_req = true;
      } else if (!memcmp(token, "MINI_DUMP", 9)) {
        proc_mini_dump(req, len);
        known_req = true;
      } else if (!memcmp(token, "SUBSCRIBE", 9)) {
        proc_subscribe(req, len);
        known_req = true;
      }
      break;
    case 10:
      if (!memcmp(token, "DISABLE_IQ", 10)) {
        proc_disable_iq(req, len);
        known_req = true;
      } else if (!memcmp(token, "ENABLE_LOG", 10)) {
        proc_enable_log(req, len);
        known_req = true;
      } else if (!memcmp(token, "DISABLE_MD", 10)) {
        proc_disable_md(req, len);
        known_req = true;
      }
      break;
    case 11:
      if (!memcmp(token, "COLLECT_LOG", 11)) {
        proc_collect_log(req, len);
        known_req = true;
      } else if (!memcmp(token, "DISABLE_LOG", 11)) {
        proc_disable_log(req, len);
        known_req = true;
      } else if (!memcmp(token, "UNSUBSCRIBE", 11)) {
        proc_unsubscribe(req, len);
        known_req = true;
      }
      break;
    case 13:
      if (!memcmp(token, "GET_LOG_STATE", 13)) {
        proc_get_log_state(req, len);
        known_req = true;
      } else if (!memcmp(token, "SAVE_LAST_LOG", 13)) {
        proc_save_last_log(req, len);
        known_req = true;
      }if (!memcmp(token, "RESET_SETTING", 13)) {
        info_log("reset all settings");
        known_req = true;
        controller()->config()->reset();
      }
      break;
    case 14:
      if (!memcmp(token, "ENABLE_EVT_LOG", 14)) {
        proc_enable_evt_log(req, len);
        known_req = true;
      } else if (!memcmp(token, "SAVE_SLEEP_LOG", 14)) {
        proc_sleep_log(req, len);
        known_req = true;
      } else if (!memcmp(token, "SET_MINIAP_LOG", 14)) {
//        proc_set_miniap_status(req, len);
        proc_set_orca_log(token, tok_len, req, len);
        known_req = true;
      } else if (!memcmp(token, "SET_ORCADP_LOG", 14)) {
        proc_set_orca_log(token, tok_len, req, len);
        known_req = true;
      }
      break;
    case 15:
      if (!memcmp(token, "GET_CP_LOG_SIZE", 15)) {
        proc_get_cp_log_size(req, len);
        known_req = true;
      } else if (!memcmp(token, "SET_CP_LOG_SIZE", 15)) {
        proc_set_cp_log_size(req, len);
        known_req = true;
      } else if (!memcmp(token, "GET_MD_STOR_POS", 15)) {
        proc_get_md_pos(req, len);
        known_req = true;
      } else if (!memcmp(token, "SET_MD_STOR_POS", 15)) {
        proc_set_md_pos(req, len);
        known_req = true;
      }
      break;
    case 17:
      if (!memcmp(token, "GET_LOG_OVERWRITE", 17)) {
        proc_get_log_overwrite(req, len);
        known_req = true;
      } else if (!memcmp(token, "GET_LOG_FILE_SIZE", 17)) {
        proc_get_log_file_size(req, len);
        known_req = true;
      } else if (!memcmp(token, "SET_LOG_FILE_SIZE", 17)) {
        proc_set_log_file_size(req, len);
        known_req = true;
      } else if (!memcmp(token, "SET_MIPI_LOG_INFO", 17)) {
        proc_set_mipi_log_info(req, len);
        known_req = true;
      } else if (!memcmp(token, "GET_MIPI_LOG_INFO", 17)) {
        proc_get_mipi_log_info(req, len);
        known_req = true;
      }
      break;
    case 18:
      if (!memcmp(token, "SET_STORAGE_CHOICE", 18)) {
        proc_set_storage_choice(req, len);
        known_req = true;
      } else if (!memcmp(token, "GET_STORAGE_CHOICE", 18)) {
        proc_get_storage_choice(req, len);
        known_req = true;
      }
      break;
    case 20:
      if (!memcmp(token, "ENABLE_LOG_OVERWRITE", 20)) {
        proc_set_log_overwrite(req, len, true);
        known_req = true;
      }
#ifdef SUPPORT_AGDSP
      else if (!memcmp(token, "GET_AGDSP_LOG_OUTPUT", 20)) {
        proc_get_agdsp_log_dest(req, len);
        known_req = true;
      } else if (!memcmp(token, "GET_AGDSP_PCM_OUTPUT", 20)) {
        proc_get_agdsp_pcm(req, len);
        known_req = true;
      } else if (!memcmp(token, "SET_AGDSP_LOG_OUTPUT", 20)) {
        proc_set_agdsp_log_dest(req, len);
        known_req = true;
      } else if (!memcmp(token, "SET_AGDSP_PCM_OUTPUT", 20)) {
        proc_set_agdsp_pcm(req, len);
        known_req = true;
      }
#endif
      break;
    case 21:
      if (!memcmp(token, "DISABLE_LOG_OVERWRITE", 21)) {
        proc_set_log_overwrite(req, len, false);
        known_req = true;
      }
      break;
    default:
      break;
  }

  if (!known_req) {
    LogString sd;

    str_assign(sd, reinterpret_cast<const char*>(req_start), orig_len);
    err_log("unknown request: %s", ls2cstring(sd));

    send_response(fd(), REC_UNKNOWN_REQ);
  }
}

void ClientHandler::proc_copy_file(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  size_t tlen;
  const uint8_t* endp = req + len;

  tok = get_token(req, len, tlen);
  if (!tok) {
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  CpType t = get_cp_type(tok, tlen);
  if (CT_UNKNOWN == t) {
    err_log("COPY_FILE with invalid CP type.");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  req = tok + tlen;
  len = endp - req;
  tok = get_token(req, len, tlen);
  if (!tok) {
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  LogFile::LogType copy_file_type = get_file_type(tok, tlen);
  if (LogFile::LT_UNKNOWN == copy_file_type) {
    err_log("COPY_FILE with invalid file type.");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  req = tok + tlen;
  len = endp - req;

  LogVector<LogString> src_files;

  while (tok = get_token(req, len, tlen)) {
    LogString src_file_path;
    str_assign(src_file_path, reinterpret_cast<const char*>(tok), tlen);
    src_files.push_back(src_file_path);

    req = tok + tlen;
    len = endp - req;
  }

  if (!src_files.size()) {
    err_log("COPY_FILE command misses file path.");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  int ret = controller()->copy_cp_files(t, LogFile::LT_DUMP, src_files);

  if (LCR_SUCCESS != ret) {
    err_log("COPY_FILE error %d", ret);
  }

  send_response(fd(), trans_result_to_req_result(ret));
}

void ClientHandler::proc_flush(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  size_t tlen;

  tok = get_token(req, len, tlen);
  if (tok) {
    send_response(fd(), REC_UNKNOWN_REQ);
    return;
  }

  info_log("flush cp logs to disk.");
  int ret = controller()->flush();

  ResponseErrorCode code;
  if (!ret) {
    code = REC_SUCCESS;
  } else {
    code = REC_FAILURE;
  }
  send_response(fd(), code);
}

void ClientHandler::proc_slogctl(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  size_t tlen;

  tok = get_token(req, len, tlen);
  if (!tok) {
    send_response(fd(), REC_UNKNOWN_REQ);
    return;
  }

  if (5 == tlen && !memcmp(tok, "clear", 5)) {
    info_log("remove CP log");
    // Delete all logs
    int ret = controller()->clear_log();
    info_log("LogController::clear_log() result =%d", ret);
    ret = send_response(fd(), trans_result_to_req_result(ret));
    if (ret) {
      err_log("proc_slogctl send reponse error");
    }
  } else {
    send_response(fd(), REC_UNKNOWN_REQ);
  }
}

void ClientHandler::proc_enable_log(const uint8_t* req, size_t len) {
  LogString sd;

  str_assign(sd, reinterpret_cast<const char*>(req), len);
  info_log("ENABLE_LOG %s", ls2cstring(sd));

  // Parse MODEM types
  ModemSet ms;
  int err = parse_modem_set(req, len, ms);

  if (err || !ms.num) {
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  TransEnableLog* en_log;
  ResponseErrorCode res;

  err = controller()->enable_log(ms.modems, ms.num,
                                 trans_result, this,
                                 en_log);
  switch (err) {
    case Transaction::TRANS_E_SUCCESS:
      res = trans_r_to_resp_code(en_log->result());
      delete en_log;
      send_response(fd(), res);
      break;
    case Transaction::TRANS_E_STARTED:
      del_events(POLLIN);
      m_trans_type = CTT_ENABLE_LOG;
      m_state = CTS_EXECUTING;
      cur_trans_ = en_log;
      break;
    default:  // Failed to start event log
      send_response(fd(), REC_FAILURE);
      break;
  }
}

void ClientHandler::proc_disable_log(const uint8_t* req, size_t len) {
  LogString sd;

  str_assign(sd, reinterpret_cast<const char*>(req), len);
  info_log("DISABLE_LOG %s", ls2cstring(sd));

  // Parse MODEM types
  ModemSet ms;
  int err = parse_modem_set(req, len, ms);

  if (err || !ms.num) {
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  TransDisableLog* dis_log;
  ResponseErrorCode res;

  err = controller()->disable_log(ms.modems, ms.num,
                                  trans_result, this,
                                  dis_log);
  switch (err) {
    case Transaction::TRANS_E_SUCCESS:
      res = trans_r_to_resp_code(dis_log->result());
      delete dis_log;
      send_response(fd(), res);
      break;
    case Transaction::TRANS_E_STARTED:
      del_events(POLLIN);
      m_trans_type = CTT_DISABLE_LOG;
      m_state = CTS_EXECUTING;
      cur_trans_ = dis_log;
      break;
    default:  // Failed to start event log
      send_response(fd(), REC_FAILURE);
      break;
  }
}

void ClientHandler::proc_enable_md(const uint8_t* req, size_t len) {
  size_t i;

  for (i = 0; i < len; ++i) {
    uint8_t c = req[i];
    if (' ' != c && '\t' != c) {
      break;
    }
  }
  if (i < len) {
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  int err = controller()->enable_md();
  ResponseErrorCode ec = REC_SUCCESS;
  if (err) {
    ec = REC_FAILURE;
  }

  send_response(fd(), ec);
}

void ClientHandler::proc_disable_md(const uint8_t* req, size_t len) {
  size_t i;

  for (i = 0; i < len; ++i) {
    uint8_t c = req[i];
    if (' ' != c && '\t' != c) {
      break;
    }
  }
  if (i < len) {
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  int err = controller()->disable_md();
  ResponseErrorCode ec = REC_SUCCESS;
  if (err) {
    ec = REC_FAILURE;
  }

  send_response(fd(), ec);
}

void ClientHandler::proc_mini_dump(const uint8_t* req, size_t len) {
  size_t i;

  for (i = 0; i < len; ++i) {
    uint8_t c = req[i];
    if (' ' != c && '\t' != c) {
      break;
    }
  }
  if (i < len) {
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  int err = controller()->mini_dump();
  ResponseErrorCode ec = REC_SUCCESS;
  if (err) {
    ec = REC_FAILURE;
  }

  send_response(fd(), ec);
}

void ClientHandler::process_conn_closed() {
  // Clear current transaction result notification
  if (CTS_IDLE != m_state) {
    cancel_trans();
    m_state = CTS_IDLE;
    m_cp = 0;
  }

  // Inform ClientManager the connection is closed
  m_mgr->process_client_disconn(this);
}

void ClientHandler::process_conn_error(int /*err*/) {
  ClientHandler::process_conn_closed();
}

void ClientHandler::proc_set_log_file_size(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  const uint8_t* endp = req + len;
  size_t tlen;

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("SET_LOG_FILE_SIZE no param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  CpType cpt = get_cp_type(tok, tlen);
  if (CT_UNKNOWN == cpt) {
    err_log("SET_LOG_FILE_SIZE invalid CP type");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  LogString cp_name;
  str_assign(cp_name, reinterpret_cast<char*>(const_cast<uint8_t*>(tok)), tlen);

  req = tok + tlen;
  len = endp - req;

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("SET_LOG_FILE_SIZE invalid parameter");

    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  unsigned val;

  if (parse_number(tok, tlen, val)) {
    err_log("SET_LOG_FILE_SIZE invalid size");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  } else if (0 == val) {
    err_log("SET_LOG_FILE_SIZE invalid size value: 0");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  info_log("SET_LOG_FILE_SIZE %s %u", ls2cstring(cp_name), val);

  req = tok + tlen;
  len = endp - req;
  if (len && get_token(req, len, tlen)) {
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  controller()->set_log_file_size(cpt, val);
  send_response(fd(), REC_SUCCESS);
}

void ClientHandler::proc_get_storage_choice(const uint8_t* req, size_t len) {
  size_t tlen;

  const uint8_t* tok = get_token(req, len, tlen);
  if (tok) {
    err_log("GET_STORAGE_CHOICE with extral param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  bool ext_stor = controller()->use_external_storage();
  char rsp[64];
  int rsp_len = snprintf(rsp, sizeof rsp, "OK %s\n",
                         ext_stor ? "EXTERNAL" : "INTERNAL");
  write(fd(), rsp, rsp_len);
  info_log("GET_STORAGE_CHOICE %s.", ext_stor ? "EXTERNAL" : "INTERNAL");
}

void ClientHandler::proc_set_storage_choice(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  size_t tlen;
  bool use_external_stor = false;
  const uint8_t* endp = req + len;

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("SET_STORAGE_CHOICE invalid param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  if (8 == tlen && !memcmp(tok, "INTERNAL", 8)) {
    use_external_stor = false;
  } else if (8 == tlen && !memcmp(tok, "EXTERNAL", 8)) {
    use_external_stor = true;
  } else {
    LogString stor;
    str_assign(stor, reinterpret_cast<const char*>(tok), tlen);
    err_log("SET_STORAGE_CHOICE unknown media type %s", ls2cstring(stor));
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  req = tok + tlen;
  len = endp - req;
  tok = get_token(req, len, tlen);
  if (tok) {
    err_log("SET_STORAGE_CHOICE with extral param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  info_log("SET_STORAGE_CHOICE %s",
           use_external_stor ? "EXTERNAL" : "INTERNAL");

  int err = controller()->active_external_storage(use_external_stor);

  if (err != LCR_SUCCESS) {
    err_log("SET_STORAGE_CHOICE %s fail, err code %d",
            use_external_stor ? "INTERNAL" : "EXTERNAL", err);
    send_response(fd(), trans_result_to_req_result(err));
  } else {
    send_response(fd(), REC_SUCCESS);
  }
}

void ClientHandler::proc_set_md_pos(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  size_t tlen;
  bool md_int_stor = false;

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("SET_MD_STOR_POS invalid param");

    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  if (8 == tlen && !memcmp(tok, "INTERNAL", 8)) {
    md_int_stor = true;
  } else if (8 == tlen && !memcmp(tok, "EXTERNAL", 8)) {
    md_int_stor = false;
  } else {
    LogString str_media;

    str_assign(str_media, reinterpret_cast<const char*>(tok), tlen);
    err_log("SET_MD_STOR_POS unknown media type %s", ls2cstring(str_media));

    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  info_log("SET_MD_STOR_POS: %s.", md_int_stor ? "INTERNAL" : "EXTERNAL" );
  controller()->set_md_int_stor(md_int_stor);
  send_response(fd(), REC_SUCCESS);
}

void ClientHandler::proc_get_log_file_size(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  size_t tlen;

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("GET_LOG_FILE_SIZE no param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  CpType cpt = get_cp_type(tok, tlen);
  if (CT_UNKNOWN == cpt) {
    err_log("GET_LOG_FILE_SIZE invalid CP type");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  LogString cp_name;
  str_assign(cp_name, reinterpret_cast<char*>(const_cast<uint8_t*>(tok)), tlen);

  size_t sz;
  int err = controller()->get_log_file_size(cpt, sz);

  if (err != LCR_SUCCESS) {
    err_log("GET_LOG_FILE_SIZE %s fail, err code %d", ls2cstring(cp_name), err);
    send_response(fd(), trans_result_to_req_result(err));
  } else {
    char rsp[64];
    int rsp_len = snprintf(rsp, sizeof rsp, "OK %u\n",
                           static_cast<unsigned>(sz));
    write(fd(), rsp, rsp_len);
  }
}

void ClientHandler::proc_set_cp_log_size(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  const uint8_t* endp = req + len;
  size_t tlen;

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("SET_CP_LOG_SIZE no param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  CpType cpt = get_cp_type(tok, tlen);
  if (CT_UNKNOWN == cpt) {
    err_log("SET_CP_LOG_SIZE invalid CP type");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  LogString cp_name;
  str_assign(cp_name, reinterpret_cast<char*>(const_cast<uint8_t*>(tok)), tlen);

  req = tok + tlen;
  len = endp - req;
  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("SET_CP_LOG_SIZE no media type parameter");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  StorageManager::MediaType mt = StorageManager::MT_STOR_END;

  if (8 == tlen) {
    if (!memcmp(tok, "internal", tlen)) {
      mt = StorageManager::MT_INT_STOR;
    } else if (!memcmp(tok, "external", tlen)) {
      mt = StorageManager::MT_EXT_STOR;
    }
  }

  if (mt == StorageManager::MT_STOR_END) {
    err_log("SET_CP_LOG_SIZE wrong media type parameter");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  req = tok + tlen;
  len = endp - req;
  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("SET_CP_LOG_SIZE no max size parameter");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  unsigned val = 0;

  if (parse_number(tok, tlen, val)) {
    err_log("SET_CP_LOG_SIZE invalid size");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  info_log("SET_CP_LOG_SIZE %s %s %u",
           ls2cstring(cp_name),
           StorageManager::MT_INT_STOR == mt ? "internal" : "external",
           val);

  req = tok + tlen;
  len = endp - req;
  if (len && get_token(req, len, tlen)) {
    err_log("SET_CP_LOG_SIZE invalid param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  int err = controller()->set_cp_log_size(cpt, mt, static_cast<size_t>(val));
  if (err != LCR_SUCCESS) {
    send_response(fd(), trans_result_to_req_result(err));
  } else {
    send_response(fd(), REC_SUCCESS);
  }
}

void ClientHandler::proc_get_cp_log_size(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  const uint8_t* endp = req + len;
  size_t tlen;

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("GET_CP_LOG_SIZE no param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  CpType cpt = get_cp_type(tok, tlen);
  if (CT_UNKNOWN == cpt) {
    err_log("GET_CP_LOG_SIZE invalid CP type");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  LogString cp_name;
  str_assign(cp_name, reinterpret_cast<char*>(const_cast<uint8_t*>(tok)), tlen);

  req = tok + tlen;
  len = endp - req;
  tok =  get_token(req, len, tlen);
  if (!tok) {
    err_log("GET_CP_LOG_SIZE invalid param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  StorageManager::MediaType mt = StorageManager::MT_STOR_END;

  if (8 == tlen) {
    if (!memcmp(tok, "internal", tlen)) {
      mt = StorageManager::MT_INT_STOR;
    } else if (!memcmp(tok, "external", tlen)) {
      mt = StorageManager::MT_EXT_STOR;
    }
  }

  if (mt == StorageManager::MT_STOR_END) {
    err_log("GET_CP_LOG_SIZE wrong media type parameter");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  size_t sz = 0;
  int err = controller()->get_cp_log_size(cpt, mt, sz);
  if (err != LCR_SUCCESS) {
    send_response(fd(), trans_result_to_req_result(err));
  } else {
    char rsp[64];
    int rsp_len = snprintf(rsp, sizeof rsp, "OK %u\n", static_cast<unsigned>(sz));
    write(fd(), rsp, rsp_len);
    info_log("GET_CP_LOG_SIZE %s %u", ls2cstring(cp_name), static_cast<unsigned>(sz));
  }
}

void ClientHandler::proc_get_log_overwrite(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  const uint8_t* endp = req + len;
  size_t tlen;

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("GET_LOG_OVERWRITE no param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  CpType cpt = get_cp_type(tok, tlen);
  if (CT_UNKNOWN == cpt) {
    err_log("GET_LOG_OVERWRITE invalid CP type");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  LogString cp_name;
  str_assign(cp_name, reinterpret_cast<char*>(const_cast<uint8_t*>(tok)), tlen);

  req = tok + tlen;
  len = endp - req;
  tok =  get_token(req, len, tlen);
  if (tok) {
    err_log("GET_LOG_OVERWRITE invalid param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  bool ov = false;
  int err = controller()->get_log_overwrite(cpt, ov);
  if (err != LCR_SUCCESS) {
    err_log("GET_LOG_OVERWRITE %s fail, err code %d", ls2cstring(cp_name), err);
    send_response(fd(), trans_result_to_req_result(err));
  } else {
    char rsp[64];
    int rsp_len = snprintf(rsp, sizeof rsp, "OK %s\n", ov ? "ENABLE" : "DISABLE");
    write(fd(), rsp, rsp_len);
  }
}

void ClientHandler::proc_set_log_overwrite(const uint8_t* req, size_t len, bool en) {
  const uint8_t* tok;
  const uint8_t* endp = req + len;
  size_t tlen;

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("%s no param", en ? "ENABLE_LOG_OVERWRITE" : "DISABLE_LOG_OVERWRITE");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  CpType cpt = get_cp_type(tok, tlen);
  if (CT_UNKNOWN == cpt) {
    err_log("%s invalid CP type", en ? "ENABLE_LOG_OVERWRITE" : "DISABLE_LOG_OVERWRITE");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  LogString cp_name;
  str_assign(cp_name, reinterpret_cast<char*>(const_cast<uint8_t*>(tok)), tlen);

  req = tok + tlen;
  len = endp - req;
  tok =  get_token(req, len, tlen);
  if (tok) {
    err_log("%s %s invalid param",
            en ? "ENABLE_LOG_OVERWRITE" : "DISABLE_LOG_OVERWRITE",
            ls2cstring(cp_name));
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  int err = controller()->set_log_overwrite(cpt, en);
  send_response(fd(), trans_result_to_req_result(err));
}

void ClientHandler::proc_get_md_pos(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  size_t tlen;

  tok = get_token(req, len, tlen);
  if (tok) {
    err_log("GET_MD_STOR_POS invalid param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  bool md_int_stor = controller()->get_md_int_stor();
  char rsp[64];
  int rsp_len = snprintf(rsp, sizeof rsp, "OK %s\n",
                         md_int_stor ? "INTERNAL" : "EXTERNAL");
  write(fd(), rsp, rsp_len);
  info_log("GET_MD_STOR_POS %s.", md_int_stor ? "INTERNAL" : "EXTERNAL");
}

void ClientHandler::proc_subscribe(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  const uint8_t* endp = req + len;
  size_t tlen;
  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("SUBSCRIBE no param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  CpType cpt = get_cp_type(tok, tlen);
  if (CT_UNKNOWN == cpt) {
    err_log("SUBSCRIBE invalid CP type");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  req = tok + tlen;
  len = endp - req;
  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("SUBSCRIBE no <event>");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  if (4 != tlen || memcmp(tok, "DUMP", 4)) {
    err_log("SUBSCRIBE invalid <event>");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  req = tok + tlen;
  len = endp - req;
  if (len && get_token(req, len, tlen)) {
    err_log("SUBSCRIBE more params than expected");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  if (dump_subs_[cpt].cp_log) {  // Already subscribed
    send_response(fd(), REC_SUCCESS);
    return;
  }

  LogPipeHandler* cp_log = controller()->get_cp(cpt);
  if (!cp_log) {
    err_log("SUBSCRIBE nonexistent CP %d", cpt);
    send_response(fd(), REC_CP_NONEXISTENT);
    return;
  }

  dump_subs_[cpt].cp_log = cp_log;
  cp_log->register_dump_start(dump_subs_ + cpt, dump_start_notify);
  cp_log->register_dump_end(dump_subs_ + cpt, dump_end_notify);

  send_response(fd(), REC_SUCCESS);
}

int ClientHandler::send_dump_notify(int fd, CpType cpt, CpEvent evt) {
  uint8_t buf[128];
  size_t len;

  if (CE_DUMP_START == evt) {
    memcpy(buf, "CP_DUMP_START ", 14);
    len = 14;
  } else {
    memcpy(buf, "CP_DUMP_END ", 12);
    len = 12;
  }

  size_t rlen = sizeof buf - len;
  size_t tlen;
  int ret = -1;
  if (!put_cp_type(buf + len, rlen, cpt, tlen)) {
    len += tlen;
    rlen -= tlen;
    if (rlen) {
      buf[len] = '\n';
      ++len;
      ssize_t wlen = write(fd, buf, len);
      if (static_cast<size_t>(wlen) == len) {
        ret = 0;
      }
    }
  }

  return ret;
}

void ClientHandler::proc_unsubscribe(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  const uint8_t* endp = req + len;
  size_t tlen;

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("UNSUBSCRIBE no param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  CpType cpt = get_cp_type(tok, tlen);
  if (CT_UNKNOWN == cpt) {
    err_log("UNSUBSCRIBE invalid CP type");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  req = tok + tlen;
  len = endp - req;
  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("UNSUBSCRIBE no <event>");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  if (4 != tlen || memcmp(tok, "DUMP", 4)) {
    err_log("UNSUBSCRIBE invalid <event>");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  req = tok + tlen;
  len = endp - req;
  if (len && get_token(req, len, tlen)) {
    err_log("UNSUBSCRIBE more params than expected");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  LogPipeHandler* cp_log = dump_subs_[cpt].cp_log;
  if (cp_log) {
    DumpEntry* de = dump_subs_ + cpt;
    cp_log->unregister_dump_start(de);
    cp_log->unregister_dump_end(de);
    dump_subs_[cpt].cp_log = nullptr;
  }

  send_response(fd(), REC_SUCCESS);
}

void ClientHandler::proc_sleep_log(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  const uint8_t* endp = req + len;
  size_t tlen;

  info_log("SAVE_SLEEP_LOG");

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("SAVE_SLEEP_LOG no param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  CpType cpt = get_cp_type(tok, tlen);
  if (CT_UNKNOWN == cpt) {
    err_log("SAVE_SLEEP_LOG invalid CP type");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  if (cpt < CT_WCDMA || cpt > CT_5MODE) {
    err_log("SAVE_SLEEP_LOG invalid CP type %d", cpt);
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  req = tok + tlen;
  len = endp - req;
  tok = get_token(req, len, tlen);
  if (tok) {
    err_log("SAVE_SLEEP_LOG extra parameter");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  // Start the sleep log transaction
  WanModemLogHandler* cp = static_cast<WanModemLogHandler*>
      (controller()->get_cp(cpt));
  if (!cp) {
    err_log("SAVE_SLEEP_LOG unknown CP %d", cpt);
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  TransSaveSleepLog* sleep_log;
  ResponseErrorCode res;
  int err = cp->save_sleep_log(this, trans_result, sleep_log);
  switch (err) {
    case Transaction::TRANS_E_STARTED:
      del_events(POLLIN);
      m_trans_type = CTT_SAVE_SLEEP_LOG;
      m_state = CTS_EXECUTING;
      cur_trans_ = sleep_log;
      break;
    case Transaction::TRANS_E_SUCCESS:
      res = trans_r_to_resp_code(sleep_log->result());
      delete sleep_log;
      send_response(fd(), res);
      break;
    default:  // Failed to save sleep log
      send_response(fd(), REC_FAILURE);
      break;
  }
}

void ClientHandler::notify_trans_result(int result) {
  if (CTS_EXECUTING == m_state) {
    switch (m_trans_type) {
      case CTT_SAVE_SLEEP_LOG:
        proc_sleep_log_result(result);
        break;
      case CTT_SAVE_RINGBUF:
        proc_ringbuf_result(result);
        break;
      default:
        err_log("Unknown transaction %d result %d", m_trans_type, result);
        break;
    }
  } else {
    err_log("Non CTS_EXECUTING state notification %d", result);
  }
}

void ClientHandler::proc_sleep_log_result(int result) {
  info_log("SAVE_SLEEP_LOG result %d", result);

  m_state = CTS_IDLE;
  m_cp = 0;

  add_events(POLLIN);
  send_response(fd(), trans_result_to_req_result(result));
}

void ClientHandler::proc_ringbuf_result(int result) {
  info_log("SAVE_RINGBUF result %d", result);

  m_state = CTS_IDLE;
  m_cp = 0;

  add_events(POLLIN);
  send_response(fd(), trans_result_to_req_result(result));
}

ResponseErrorCode ClientHandler::trans_result_to_req_result(int result) {
  ResponseErrorCode res;

  switch (result) {
    case LCR_SUCCESS:
      res = REC_SUCCESS;
      break;
    case LCR_IN_TRANSACTION:
      res = REC_IN_TRANSACTION;
      break;
    case LCR_LOG_DISABLED:
      res = REC_LOG_DISABLED;
      break;
    case LCR_SLEEP_LOG_NOT_SUPPORTED:
      res = REC_SLEEP_LOG_NOT_SUPPORTED;
      break;
    case LCR_RINGBUF_NOT_SUPPORTED:
      res = REC_RINGBUF_NOT_SUPPORTED;
      break;
    case LCR_CP_ASSERTED:
      res = REC_CP_ASSERTED;
      break;
    case LCR_CP_NONEXISTENT:
      res = REC_CP_NONEXISTENT;
      break;
    case LCR_PARAM_INVALID:
      res = REC_INVAL_PARAM;
      break;
    case LCR_NO_AGDSP:
      res = REC_NO_AGDSP;
      break;
    default:
      res = REC_FAILURE;
      break;
  }

  return res;
}

void ClientHandler::proc_enable_iq(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  const uint8_t* endp = req + len;
  size_t tlen;

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("ENABLE_IQ no param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  CpType cpt = get_cp_type(tok, tlen);
  if (CT_UNKNOWN == cpt) {
    err_log("ENABLE_IQ invalid CP type");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  req = tok + tlen;
  len = endp - req;
  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("ENABLE_IQ no I/Q type");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  IqType it;

  it = LogConfig::get_iq_type(tok, tlen);
  if (IT_WCDMA != it) {
    err_log("ENABLE_IQ unknown I/Q type");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  // Enable I/Q saving
  int err = controller()->enable_wcdma_iq(cpt);
  if (LCR_SUCCESS != err) {
    err_log("enable_iq error %d", err);
  }
  send_response(fd(), trans_result_to_req_result(err));
}

void ClientHandler::proc_disable_iq(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  const uint8_t* endp = req + len;
  size_t tlen;

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("DISABLE_IQ no param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  CpType cpt = get_cp_type(tok, tlen);
  if (CT_UNKNOWN == cpt) {
    err_log("DISABLE_IQ invalid CP type");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  IqType it;

  req = tok + tlen;
  len = endp - req;
  tok = get_token(req, len, tlen);
  if (tok) {
    it = LogConfig::get_iq_type(tok, tlen);
    if (IT_WCDMA != it) {
      err_log("DISABLE_IQ unknown I/Q type");
      send_response(fd(), REC_INVAL_PARAM);
      return;
    }
  } else {
    it = IT_ALL;
  }

  // Disable I/Q log
  int err = controller()->disable_wcdma_iq(cpt);
  if (LCR_SUCCESS != err) {
    err_log("disable_iq error %d", err);
  }
  send_response(fd(), trans_result_to_req_result(err));
}

void ClientHandler::proc_get_log_state(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  size_t tlen;

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("GET_LOG_STATE no param");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  CpType ct = get_cp_type(tok, tlen);
  if (CT_UNKNOWN == ct) {
    err_log("GET_LOG_STATE invalid CP type");
    send_response(fd(), REC_UNKNOWN_CP_TYPE);
    return;
  }

  LogConfig::LogMode lm = controller()->get_log_state(ct);
  send_log_state_response(fd(), lm);
}

void ClientHandler::dump_start_notify(void* client) {
  DumpEntry* de = static_cast<DumpEntry*>(client);
  ClientHandler* conn = de->handler;
  CpType ct = static_cast<CpType>(de - conn->dump_subs_);

  send_dump_notify(conn->fd(), ct, CE_DUMP_START);
}

void ClientHandler::dump_end_notify(void* client) {
  DumpEntry* de = static_cast<DumpEntry*>(client);
  ClientHandler* conn = de->handler;
  CpType ct = static_cast<CpType>(de - conn->dump_subs_);

  send_dump_notify(conn->fd(), ct, CE_DUMP_END);
}

ResponseErrorCode ClientHandler::trans_r_to_resp_code(int res) {
  ResponseErrorCode resp;

  switch (res) {
    case Transaction::TRANS_R_SUCCESS:
      resp = REC_SUCCESS;
      break;
    case Transaction::TRANS_R_NONEXISTENT_CP:
      resp = REC_CP_NONEXISTENT;
      break;
    case Transaction::TRANS_R_LOG_NOT_ENABLED:
      resp = REC_LOG_DISABLED;
      break;
    case Transaction::TRANS_R_CP_ASSERTED:
      resp = REC_CP_ASSERTED;
      break;
    case Transaction::TRANS_R_SLEEP_LOG_NOT_SUPPORTED:
      resp = REC_SLEEP_LOG_NOT_SUPPORTED;
      break;
    default:  //Transaction::TRANS_R_FAILURE:
      resp = REC_FAILURE;
      break;
  }

  return resp;
}

void ClientHandler::proc_collect_log(const uint8_t* req, size_t len) {
  LogString sd;

  str_assign(sd, reinterpret_cast<const char*>(req), len);
  info_log("COLLECT_LOG %s", ls2cstring(sd));

  // Parse MODEM types
  ModemSet ms;
  int err = parse_modem_set(req, len, ms);

  if (err) {
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  // Request the transaction to LogController
  TransLogCollect* log_col;
  ResponseErrorCode res;

  err = controller()->start_log_collect(ms, trans_result, this, log_col);
  switch (err) {
    case Transaction::TRANS_E_STARTED:
      del_events(POLLIN);
      m_trans_type = CTT_COLLECT_LOG;
      m_state = CTS_EXECUTING;
      cur_trans_ = log_col;
      break;
    case Transaction::TRANS_E_SUCCESS:
      res = trans_r_to_resp_code(log_col->result());
      delete log_col;
      send_response(fd(), res);
      break;
    default:  // Failed to start log collection
      send_response(fd(), REC_FAILURE);
      break;
  }
}

void ClientHandler::trans_result(void* client, Transaction* trans) {
  ClientHandler* conn = static_cast<ClientHandler*>(client);

  if (CTS_EXECUTING == conn->m_state) {
    conn->m_trans_type = CTT_UNKNOWN;
    conn->m_state = CTS_IDLE;

    info_log("transaction result %d", trans->result());

    ResponseErrorCode res = trans_r_to_resp_code(trans->result());

    delete trans;
    conn->cur_trans_ = nullptr;

    send_response(conn->fd(), res);
    conn->add_events(POLLIN);
  } else {
    err_log("receive transaction result when no trans");
  }
}

void ClientHandler::proc_enable_evt_log(const uint8_t* req, size_t len) {
  LogString sd;

  str_assign(sd, reinterpret_cast<const char*>(req), len);
  info_log("ENABLE_EVT_LOG %s", ls2cstring(sd));

  const uint8_t* endp = req + len;
  const uint8_t* tok;
  size_t tlen;

  tok = get_token(req, len, tlen);
  if (!tok) {
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  // Only MODEM support event triggered log
  CpType ct = get_cp_type(tok, tlen);
  if (ct < CT_WCDMA || ct > CT_5MODE) {
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  req = tok + tlen;
  len = endp - req;
  if (get_token(req, len, tlen)) {
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  TransStartEventLog* start_evt;
  ResponseErrorCode res;
  int err = controller()->start_event_log(ct, trans_result, this,
                                          start_evt);
  switch (err) {
    case Transaction::TRANS_E_STARTED:
      del_events(POLLIN);
      m_trans_type = CTT_START_EVT_LOG;
      m_state = CTS_EXECUTING;
      cur_trans_ = start_evt;
      break;
    case Transaction::TRANS_E_SUCCESS:
      res = trans_r_to_resp_code(start_evt->result());
      delete start_evt;
      send_response(fd(), res);
      break;
    default:  // Failed to start event log
      send_response(fd(), REC_FAILURE);
      break;
  }
}

void ClientHandler::cancel_trans() {
  if (cur_trans_) {
    if (Transaction::GLOBAL == cur_trans_->category()) {
      controller()->cancel_trans(cur_trans_);
    } else {
      m_cp->cancel_trans(cur_trans_);
      m_cp = nullptr;
    }
    delete cur_trans_;
    cur_trans_ = nullptr;
    m_state = CTS_IDLE;
    m_trans_type = CTT_UNKNOWN;
  }
}

int ClientHandler::send_log_state_response(int conn,
                                           LogConfig::LogMode mode) {
  char resp[32];
  const char* mode_name;

  switch (mode) {
    case LogConfig::LM_NORMAL:
      mode_name = "ON";
      break;
    case LogConfig::LM_OFF:
      mode_name = "OFF";
      break;
    default:
      mode_name = "EVENT";
      break;
  }

  int len = snprintf(resp, 32, "OK %s\n", mode_name);
  ssize_t n = write(conn, resp, len);

  return n == len ? 0 : -1;
}

void ClientHandler::proc_save_last_log(const uint8_t* req, size_t len) {
  const uint8_t* tok;
  size_t tlen;
  const uint8_t* endp = req + len;

  tok = get_token(req, len, tlen);
  if (!tok) {
    err_log("SAVE_LAST_LOG with no cp type");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  size_t tail;
  if (get_token(tok + tlen, endp - tok - tlen, tail)) {
    err_log("SAVE_LAST_LOG with extra parameter");
    send_response(fd(), REC_INVAL_PARAM);
    return;
  }

  if (5 == tlen && !memcmp(tok, "MODEM", 5)) {
    WanModemLogHandler* cp = static_cast<WanModemLogHandler*>
        (controller()->get_cp(LogConfig::get_wan_modem_type()));

    if (!cp) {  // Subsystem does not exist.
      err_log("SAVE_LAST_LOG no wan modem is supported");
      send_response(fd(), REC_CP_NONEXISTENT);
    } else {
      TransModemLastLog* last_log{};
      int ret = cp->save_last_log(this, trans_result, last_log);
      ResponseErrorCode res{REC_SUCCESS};

      switch (ret) {
        case Transaction::TRANS_E_SUCCESS:
          res = trans_r_to_resp_code(last_log->result());
          delete last_log;
          send_response(fd(), res);
          break;
        case Transaction::TRANS_E_STARTED:
          del_events(POLLIN);
          m_trans_type = CTT_MODEM_LAST_LOG;
          m_state = CTS_EXECUTING;
          cur_trans_ = last_log;
          break;
        default:  // Failed to start event log
          send_response(fd(), REC_FAILURE);
          break;
      }
    }
  } else if (3 == tlen && !memcmp(tok, "WCN", 3)) {
    TransWcnLastLog* last_log;
    int ret = controller()->save_wcn_last_log(trans_result,
                                              this, last_log);
    ResponseErrorCode res{REC_SUCCESS};

    switch (ret) {
      case Transaction::TRANS_E_SUCCESS:
        res = trans_r_to_resp_code(last_log->result());
        delete last_log;
        send_response(fd(), res);
        break;
      case Transaction::TRANS_E_STARTED:
        del_events(POLLIN);
        m_trans_type = CTT_WCN_LAST_LOG;
        m_state = CTS_EXECUTING;
        cur_trans_ = last_log;
        break;
      default:  // Failed to start event log
        send_response(fd(), REC_FAILURE);
        break;
    }
  } else {  // Unknown subsystem
    LogString param;
    str_assign(param, reinterpret_cast<const char*>(req), len);

    err_log("SAVE_LAST_LOG with invalid CP type %s", ls2cstring(param));
    send_response(fd(), REC_INVAL_PARAM);
  }
}

//debug
void ClientHandler::show_memory_info(uint8_t* mem, size_t len) {
  info_log("show_memory_info.");
  int i = 0;
  for(i = 0; i < len; i += 8) {
    info_log("%p : %x\t%x\t%x\t%x\t%x\t%x\t%x\t%x", &mem[i],
        mem[i], *(mem + i + 1), *(mem + i + 2), *(mem + i + 3),
        *(mem + i + 4), *(mem + i + 5), *(mem + i + 6), *(mem + i + 7));
  }
}

