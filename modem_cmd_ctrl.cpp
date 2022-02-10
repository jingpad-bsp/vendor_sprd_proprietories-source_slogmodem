/*
 *  modem_cmd_ctrl.cpp - The MODEM command controller.
 *
 *  Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2019-9-20 Pizer.Fan
 *  Initial version.
 */

#include <climits>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "modem_cmd_ctrl.h"
#include "multiplexer.h"
#include "timer_mgr.h"

ModemCmdController::ModemCmdController(LogController* ctrl,
                                     Multiplexer* multiplexer)
    :DeviceFileHandler{4096, ctrl, multiplexer},
     agdsp_log_dest_suspended_{false},
     agdsp_log_dest_{LogConfig::AGDSP_LOG_DEST_AP},
     agdsp_pcm_dump_suspended_{false},
     agdsp_pcm_dump_{true} {
}

ModemCmdController::~ModemCmdController() {
}

int ModemCmdController::try_open() {
  /* already open? */
  if (fd() >= 0) {
    return fd();
  }

  if (open() >= 0) {
    multiplexer()->register_fd(this, POLLIN);
    info_log("%s opened", ls2cstring(path()));
  } else {
    multiplexer()->timer_mgr().add_timer(300, reopen_cmd_dev, this);
    err_log("open %s error", ls2cstring(path()));
  }

  return fd();
}

void ModemCmdController::do_suspend_actions() {
  /* have suspended actions? */
  if(is_suspend_set_agdsp_pcm_dump()) {
    resume_set_agdsp_pcm_dump();
  }
  if(is_suspend_set_agdsp_log_dest()) {
    resume_set_agdsp_log_dest();
  }
  if (is_suspend_set_miniap_log_status()) {
    resume_set_miniap_log_status();
  }
  if (is_suspend_set_orcadp_log_status()) {
    resume_set_orcadp_log_status();
  }
  if (is_suspend_set_mipi_log_info()) {
    resume_set_mipi_log_info();
  }
}

void ModemCmdController::reopen_cmd_dev(void* param) {
  ModemCmdController* cmd_ctrl = static_cast<ModemCmdController*>(param);
  if(cmd_ctrl->try_open() >= 0) {
    cmd_ctrl->do_suspend_actions();
  }
}

bool ModemCmdController::process_data(DataBuffer& buf) {
  /* drop it */
  buf.data_start = buf.data_len = 0;
  return false;
}

int ModemCmdController::set_agdsp_log_dest(LogConfig::AgDspLogDestination dest) {
  if (fd() < 0) {
    suspend_set_agdsp_log_dest(dest);
    err_log("ModemCmdController: suspend set_agdsp_log_dest: %d, wait for opening: %s", 
      dest, ls2cstring(path()));
    return -1;
  }

  LogString cmd_str;
  cmd_str += "SET_AGDSP_LOG_OUTPUT";
  
  switch(dest) {
    case LogConfig::AGDSP_LOG_DEST_OFF:
      cmd_str += " OFF";
      break;
      
    case LogConfig::AGDSP_LOG_DEST_UART:
      cmd_str += " UART";
      break;

    case LogConfig::AGDSP_LOG_DEST_AP:
      cmd_str += " USB";
      break;
      
    default:
      err_log("unknown dest: %d", dest);
      return -1;
  }
  
  cmd_str += "\n";
  ssize_t nw = ::write(fd(), ls2cstring(cmd_str), cmd_str.length());
  if ((ssize_t)cmd_str.length() != nw) {
    err_log("ModemCmdController: set_agdsp_log_dest failure, ret=%d", nw);
    return -1;
  } else {
    info_log("ModemCmdController: set_agdsp_log_dest: %d", (int)dest);
    return 0;
  }
}

void ModemCmdController::resume_set_agdsp_log_dest() {
  if (agdsp_log_dest_suspended_) {
    info_log("resume set_agdsp_log_dest: %d", agdsp_log_dest_);
    agdsp_log_dest_suspended_ = false;
    set_agdsp_log_dest(agdsp_log_dest_);
  }
}

int ModemCmdController::set_agdsp_pcm_dump(bool pcm) {
  if (fd() < 0) {
    suspend_set_agdsp_pcm_dump(pcm);
    err_log("suspend set_agdsp_pcm_dump: %d, wait for opening: %s",
      pcm, ls2cstring(path()));
    return -1;
  }

  LogString cmd_str;
  cmd_str += "SET_AGDSP_PCM_OUTPUT";
  cmd_str += pcm ? " ON" : " OFF";
  cmd_str += "\n";
  ssize_t nw = ::write(fd(), ls2cstring(cmd_str), cmd_str.length());
  if ((ssize_t)cmd_str.length() != nw) {
    err_log("ModemCmdController: set_agdsp_pcm_dump failure, ret=%d", nw);
    return -1;
  } else {
    info_log("ModemCmdController: set_agdsp_pcm_dump: %d", (int)pcm);
    return 0;
  }
}

void ModemCmdController::resume_set_agdsp_pcm_dump() {
  if (agdsp_pcm_dump_suspended_) {
    info_log("resume set_agdsp_pcm_dump: %d", agdsp_pcm_dump_);
    agdsp_pcm_dump_suspended_ = false;
    set_agdsp_pcm_dump(agdsp_pcm_dump_);
  }
}

int ModemCmdController::set_miniap_log_status(bool status) {
  if (fd() < 0) {
    suspend_set_miniap_log_status(status);
    err_log("suspend set_miniap_log_status: %d, wait for opening: %s",
            status, ls2cstring(path()));
    return -1;
  }

  LogString cmd_str;
  cmd_str += "SET_MINIAP_LOG";
  cmd_str += status ? " ON" : " OFF";
  cmd_str += "\n";

  ssize_t nw = ::write(fd(), ls2cstring(cmd_str), cmd_str.length());
  if ((ssize_t)cmd_str.length() != nw) {
    err_log("ModemCmdController: set_miniap_log_status failure, ret=%d", nw);
    return -1;
  } else {
    info_log("ModemCmdController: set_miniap_log_status: %s", ls2cstring(cmd_str));
    return 0;
  }
}

int ModemCmdController::set_etb_save() {
  LogString cmd_str;
  cmd_str += "GET_ETB";
  cmd_str += "\n";
  ssize_t nw = ::write(fd(), ls2cstring(cmd_str), cmd_str.length());
  if ((ssize_t)cmd_str.length() != nw) {
    err_log("ModemCmdController: set_etb_save failure, ret=%d", nw);
    return -1;
  } else {
    info_log("ModemCmdController: set_etb_save");
    return 0;
  }
}

void ModemCmdController::resume_set_miniap_log_status() {
  if (miniap_log_status_suspended_) {
    info_log("resume set_miniap_log_status: %d", miniap_log_status);
    miniap_log_status_suspended_ = false;
    set_miniap_log_status(miniap_log_status);
  }
}

int ModemCmdController::set_orcadp_log_status(bool status) {
  if (fd() < 0) {
    suspend_set_orcadp_log_status(status);
    err_log("suspend_set_orcadp_log_status: %d, wait for opening: %s",
            status, ls2cstring(path()));
    return -1;
  }

  LogString cmd_str;
  cmd_str += "SET_CM4";
  cmd_str += status ? " LOG_ON" : " LOG_OFF";
  cmd_str += "\n";

  ssize_t nw = ::write(fd(), ls2cstring(cmd_str), cmd_str.length());
  if ((ssize_t)cmd_str.length() != nw) {
    err_log("ModemCmdController: set_orcadp_log_status failure, ret=%d", nw);
    return -1;
  } else {
    info_log("ModemCmdController: set_orcadp_log_status: %s", ls2cstring(cmd_str));
    return 0;
  }
}

void ModemCmdController::resume_set_orcadp_log_status() {
  if (orcadp_log_status_suspended_) {
    info_log("resume set_orcadp_log_status: %d", orcadp_log_status);
    orcadp_log_status_suspended_ = false;
    set_orcadp_log_status(orcadp_log_status);
  }
}

int ModemCmdController::set_mipi_log_info(LogConfig::MipiLogList mipi_log_info) {
  if (fd() < 0) {
    suspend_set_mipi_log_info(mipi_log_info);
    err_log("suspend suspend_set_mipi_log_info, wait for opening: %s", ls2cstring(path()));
    return -1;
  }

  for (auto c : mipi_log_info) {
    LogString cmd_str;
    cmd_str += "SET_MIPI_LOG_INFO";
    cmd_str += " ";
    cmd_str += c->type;
    cmd_str += " ";
    cmd_str += c->channel;
    cmd_str += " ";
    cmd_str += c->freq;
    cmd_str += "\n";

    ssize_t nw = ::write(fd(), ls2cstring(cmd_str), cmd_str.length());
    if ((ssize_t)cmd_str.length() != nw) {
      err_log("ModemCmdController: set_mipi_log failure, ret=%d", nw);
      return -1;
    } else {
      info_log("ModemCmdController: set_mipi_log: %s",
                    ls2cstring(cmd_str));
    }
  }
  return 0;
}

void ModemCmdController::resume_set_mipi_log_info() {
  if (mipi_log_info_suspended_) {
    mipi_log_info_suspended_ = false;
    set_mipi_log_info(mipi_log_info_);
  }
}
