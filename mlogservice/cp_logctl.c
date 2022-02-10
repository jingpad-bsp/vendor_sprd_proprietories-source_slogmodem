/*
 *  cp_logctl.c - Handling AT commands.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-2-13 Elon Li
 *  Initial version.
*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cutils/properties.h>
#include "at_cmd.h"
#include "config.h"
#include "cp_logctl.h"
#include "misc.h"
#include "proc_cmd.h"
#include "socket_hdl.h"

void close_at(void) {
  close_at_channel();
}

void open_at(void) {
  open_at_channel();
}

void check_at(const int num) {
  const uint8_t* cmd = conf[num].at_check;
  uint8_t req[MAX_NAME] = {0};
  uint8_t at_res[MAX_AT_RESPONSE] = {0};
  int r_parse = -1;

  if (cmd) {
    r_parse = send_at_parse_result(cmd, at_res, strlen(cmd));
  }

  if (-1 != conf[num].mode) {
    if (r_parse != conf[num].mode || NULL == cmd) {
      strupper(conf[num].name, req);
      if (num == CMD_LOGLEVEL && conf[CMD_ARMLOG].mode != 1) {
        info_log("arm log is close, slogmodem will not set loglevel");
        return;
      }
      proc_req(req, conf[num].mode, strlen(conf[num].name), -1);
    } else if (r_parse == conf[num].mode) {
      info_log("%s nv is same as DB",conf[num].name);
      proc_req(req, conf[num].mode, strlen(conf[num].name), -1);
    }
  } else {
    info_log("%s has invaild parameter,need't process",conf[num].name);
  }
}

int proc_etb(const int value, const int socket) {
  uint8_t at_res[MAX_AT_RESPONSE] = {0};
  uint8_t cmd[MAX_COMMAND_BYTES] = {0};
  uint8_t atvalue[4] = "0,0";

  if (value == 1) {
    memcpy(atvalue, "1,0", 4);
  }

  snprintf(cmd, sizeof(cmd), "%s%s%c", AT_SETETB, atvalue, '\r');
  int  r_parse = send_at_parse_result(cmd, at_res, strlen(cmd));
  if (socket >= 0) {  //it means the client trigger this function, not boot
    if (r_parse == 1) {
      send_response(socket, REC_SUCCESS);
    } else {
      send_response(socket, REC_FAILURE);
      return 0;
    }
  }

  return data_sync("etb", value);
}

int proc_arm(const int value, const int socket) {
  uint8_t atres[MAX_AT_RESPONSE] = {0};
  uint8_t cmd[MAX_COMMAND_BYTES] = {0};

  snprintf(cmd, sizeof(cmd), "%s%d%c", AT_SETARMLOG, value, '\r');
  int r_parse = send_at_parse_result(cmd, atres, strlen(cmd));
  if (socket >= 0) {
    if (r_parse == 1) {
      send_response(socket, REC_SUCCESS);
    } else {
      send_response(socket, REC_FAILURE);
    }
  }

  return data_sync("arm", value);
}

int proc_cap(const int value,const int socket) {
  uint8_t atres[MAX_AT_RESPONSE] = {0};
  uint8_t cmd[MAX_COMMAND_BYTES] = {0};

  snprintf(cmd, sizeof(cmd), "%s%d%c", AT_SETCAPLOG, value,'\r');
  int r_parse = send_at_parse_result(cmd, atres, strlen(cmd));
  if (socket >= 0) {
    if (r_parse == 1) {
      send_response(socket, REC_SUCCESS);
    } else {
      send_response(socket, REC_FAILURE);
    }
  }

  return data_sync("cap", value);
}

int proc_dsp(const int value,const int socket) {
  uint8_t atres[MAX_AT_RESPONSE] = {0};
  uint8_t cmd[MAX_COMMAND_BYTES] = {0};

  snprintf(cmd, sizeof(cmd), "%s%d%c", AT_SETDSPLOG, value, '\r');
  int r_parse = send_at_parse_result(cmd, atres, strlen(cmd));
  if (socket >= 0) {
    if (r_parse == 1) {
      send_response(socket, REC_SUCCESS);
    } else {
      send_response(socket, REC_FAILURE);
    }
  }

  return data_sync("dsp", value);
}

int proc_evt(const int value, const int socket) {
  uint8_t atres[MAX_AT_RESPONSE] = {0};
  uint8_t cmd[MAX_COMMAND_BYTES] = {0};

  if (value == 1) {
    snprintf(cmd, sizeof(cmd), "%s%d%c", AT_SETARMLOG, 2, '\r');
    int r_parse = send_at_parse_result(cmd, atres, strlen(cmd));
    if (socket >= 0) {
      if (r_parse == 1) {
        send_response(socket, REC_SUCCESS);
      } else {
        send_response(socket, REC_FAILURE);
      }
    }
  } else {
    info_log("log_mode: do nothing");
    if (socket >= 0) {
      send_response(socket, REC_SUCCESS);
    }
  }

  return data_sync("event", value);
}

int proc_loglevel(const int value, const int socket) {
  uint8_t atres[MAX_AT_RESPONSE] = {0};
  uint8_t cmd[MAX_COMMAND_BYTES] = {0};

  if (value == 3) {
    snprintf(cmd, sizeof(cmd), "%s%s%c", AT_SETLOGLEVEL, LOG_DEBUG_CMD, '\r');
  } else if (value == 0) {
    snprintf(cmd, sizeof(cmd), "%s%s%c", AT_SETLOGLEVEL, LOG_USER_CMD, '\r');
  } else if (value == 1) {
    snprintf(cmd, sizeof(cmd), "%s%s%c", AT_SETLOGLEVEL, LOG_LEVEL_1, '\r');
  } else if (value == 2) {
    snprintf(cmd, sizeof(cmd), "%s%s%c", AT_SETLOGLEVEL, LOG_LEVEL_2, '\r');
  } else {
    info_log("log_mode: do nothing");
    if (socket >= 0) {
      send_response(socket, REC_SUCCESS);
    }
    return 0;
  }
  int r_parse = send_at_parse_result(cmd, atres, strlen(cmd));
  if (socket >= 0) {
    if (r_parse == 1) {
      send_response(socket, REC_SUCCESS);
    } else {
      send_response(socket, REC_FAILURE);
    }
  }

  return data_sync("loglevel",value);
}

int proc_armpcm(const int value, const int socket) {
  uint8_t atres[MAX_AT_RESPONSE] = {0};
  uint8_t cmd[MAX_COMMAND_BYTES] = {0};

  snprintf(cmd, sizeof(cmd), "%s%d%s%c", AT_SETARMPCMDUMP, value, ",1,1,15", '\r');
  int r_parse = send_at_parse_result(cmd, atres, strlen(cmd));
  if (socket >= 0) {
    if (r_parse == 1) {
      send_response(socket, REC_SUCCESS);
    } else {
      send_response(socket, REC_FAILURE);
    }
  }

  return data_sync("armpcm",value);
}

int proc_dsppcm(const int value, const int socket) {
  uint8_t atres[MAX_AT_RESPONSE] = {0};
  uint8_t cmd[MAX_COMMAND_BYTES] = {0};

  if (1 == value) {
    snprintf(cmd, sizeof(cmd), "%s%s%c", AT_SETDSPPCMDUMP, "65535,0,0,4096", '\r');
  } else if (0 == value) {
    snprintf(cmd, sizeof(cmd), "%s%s%c", AT_SETDSPPCMDUMP, "65535,0,0,0", '\r');
  }  else {
    info_log("log_mode: do nothing");
    if (socket >= 0) {
      send_response(socket, REC_SUCCESS);
    }
    return 0;
  }
  int r_parse = send_at_parse_result(cmd, atres, strlen(cmd));
  if (socket >= 0) {
    if (r_parse == 1) {
      send_response(socket, REC_SUCCESS);
    } else {
      send_response(socket, REC_FAILURE);
    }
  }

  return data_sync("dsppcm",value);
}

int proc_miniaplog(const int value, const int socket) {
  char cmd[MAX_COMMAND_BYTES] = {0};
  char res[MAX_RESPONSE] = {0};
  snprintf(cmd, sizeof(cmd), "SET_MINIAP_LOG %s\n", ((value == 1) ? "ON" : "OFF"));
  sersendCmd(cmd, res, "slogmodem");
  if (socket >= 0) {
    write(socket, res, MAX_RESPONSE);//because slogmodem already follow this protocol,so cut-Through
  }
  return data_sync("orcaap", value);
}

int proc_orcadplog(const int value, const int socket) {
  char cmd[MAX_COMMAND_BYTES] = {0};
  char res[MAX_RESPONSE] = {0};
  snprintf(cmd, sizeof(cmd), "SET_ORCADP_LOG %s\n", ((value == 1) ? "ON" : "OFF"));
  info_log("%s", cmd);
  sersendCmd(cmd, res, "slogmodem");
  if (socket >= 0) {
    write(socket, res, MAX_RESPONSE);//because slogmodem already follow this protocol,so cut-Through
  }
  return data_sync("orcadp", value);
}

