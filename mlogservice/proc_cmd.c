/*
 *  proc_cmd.c - resolve customer requests.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-2-13 Elon Li
 *  Initial version.
*/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "cp_logctl.h"
#include "misc.h"
#include "proc_cmd.h"
#include "socket_hdl.h"

int proc_req(const uint8_t* cmd, const int addvalue, const int len, int c_fd) {
  int dirty = 0;

  switch(len) {
    case 3:
      if (!memcmp(cmd, "ETB", 3)) {
        dirty = proc_etb(addvalue, c_fd);
      } else if (!memcmp(cmd, "ARM", 3)) {
        dirty = proc_arm(addvalue, c_fd);
      } else if (!memcmp(cmd, "DSP", 3)) {
        dirty = proc_dsp(addvalue, c_fd);
      } else if (!memcmp(cmd, "CAP", 3)) {
        dirty = proc_cap(addvalue, c_fd);
      }
      break;
    case 5:
      if (!memcmp(cmd, "EVENT", 5)) {
        dirty = proc_evt(addvalue, c_fd);
      }
      break;
    case 6:
      if (!memcmp(cmd, "ARMPCM", 6)) {
        dirty = proc_armpcm(addvalue, c_fd);
      } else if (!memcmp(cmd, "DSPPCM", 6)) {
        dirty = proc_dsppcm(addvalue, c_fd);
      } else if (!memcmp(cmd, "ORCAAP", 6)) {
        dirty = proc_miniaplog(addvalue, c_fd);
      } else if (!memcmp(cmd, "ORCADP", 6)) {
        dirty = proc_orcadplog(addvalue, c_fd);
      }
      break;
    case 8:
      if (!memcmp(cmd, "LOGLEVEL", 8)) {
        dirty = proc_loglevel(addvalue, c_fd);
      }
      break;
    default:
      send_response(c_fd, REC_INVAL_PARAM);
      break;
  }

  return dirty;
}

int proc_set(const uint8_t* req, size_t len, int c_fd) {
  uint8_t p1[MAX_COMMAND_BYTES] = {0};
  const uint8_t* tok;
  const uint8_t* endp = req + len;
  int value = -1;
  int dirty = 0;
  size_t tlen1;
  size_t tlen2;

  tok = get_token(req, len, &tlen1);
  info_log("modem_log_service set %s,c_fd is %d", (const char*)req, c_fd);
  if (!tok) {
    err_log("set no param");
    send_response(c_fd, REC_INVAL_PARAM);
    return -1;
  }
  memcpy(p1, tok, tlen1);

  req = tok + tlen1;
  len = endp - req;
  tok = get_token(req, len, &tlen2);
  if (!tok) {
   err_log("set no apram value");
   send_response(c_fd, REC_INVAL_PARAM);
   return -1;
  }

  switch(tlen2) {
    case 1:
      value = atoi(tok);
      break;
    case 2:
      if (!memcmp(tok, "ON", 2)) {
        value = 1;
      } else if (!memcmp(tok, "AP", 2)) {
        value = 2;
      }
      break;
    case 3:
      if (!memcmp(tok, "OFF", 3)) {
        value = 0;
      }
      break;
    case 4:
      if (!memcmp(tok, "UART", 4)) {
        value = 1;
      }
      break;
    case 5:
      if (!memcmp(tok, "EVENT", 5)) {
        value = 2;
      }
      break;
    default:
      break;
  }
  if (value != -1) {
    dirty = proc_req(p1, value, tlen1, c_fd);
  } else {
    send_response(c_fd, REC_INVAL_PARAM);
  }

  if (dirty > 0) {
    if(!write_work_file()) {
      return -1;
    }
  }

 return 0;
}

int proc_get(const uint8_t* req, size_t len, int c_fd) {
  uint8_t res[MAX_RESPONSE];
  const uint8_t* tok;
  size_t tlen;

  tok = get_token(req, len, &tlen);
  info_log("modem_log_service get %s,c_fd is %d", (const char*)req, c_fd);
  if (!tok) {
    send_response(c_fd, REC_UNKNOWN_REQ);
    return -1;
  }

  enum CmdList cmd = parse_cmd(tok,tlen);  //if tok haven't \n or \0, can't find
  if ( cmd == CMD_UNKNOWN) {
    send_response(c_fd, REC_INVAL_PARAM);
    return -1;
  }

  switch (cmd) {
    case CMD_DSPLOG:
      if (conf[cmd].mode == 0) {
        memcpy(res, "OK OFF\n", 8);
      } else if (conf[cmd].mode == 1) {
        memcpy(res, "OK UART\n", 9);
      } else {
        memcpy(res, "OK AP\n", 7);
      }
      break;
    case CMD_LOGLEVEL:
      snprintf(res, MAX_RESPONSE, "OK %d\n", conf[cmd].mode);
      break;
    default:
      if (conf[cmd].mode == 0) {
        memcpy(res, "OK OFF\n", 8);
      } else {
        memcpy(res, "OK ON\n", 7);
      }
      break;
  }

  ssize_t nwr = write(c_fd, res, strlen(res));
  if (nwr != strlen(res)) {
    err_log("send response error");
  }
  return 0;
}

int write_work_file() {
  uint8_t result[MAX_NAME] = {0};
  FILE *pf = fopen(work_conf_file, "w+t");

  if (!pf) {
    err_log("open work file error");
    return -1;
  }

  fprintf(pf, "#Name\tMode\n");
  for (int i = 0; i < CMD_MAX; i++) {
    switch (i) {
      case CMD_DSPLOG:
        if (conf[i].mode == 0) {
          memcpy(result, "off", 3);
        } else if (conf[i].mode == 1) {
          memcpy(result, "uart", 4);
        } else {
          memcpy(result, "ap", 2);
        }
        break;
      case CMD_LOGLEVEL:
        snprintf(result, sizeof(result), "%d", conf[i].mode);
        break;
      default:
        if (conf[i].mode == 0) {
          memcpy(result, "off", 3);
        } else {
          memcpy(result, "on", 2);
        }
        break;
    }
    fprintf(pf, "%s\t\t%s\n", conf[i].name, result);
    memset(result, 0, sizeof(result));
  }
  fclose(pf);

  return 0;
}

void boot_action(void) {
  open_at();
  for (int i = 0; i < CMD_MAX; i++) {
    check_at(i);
  }
  close_at();
}

void set_orca_log_status(const uint8_t* item_name) {
  int index;
  uint8_t req[MAX_NAME] = {0};
  strupper(item_name, req);
  int ret = find_match_item(item_name, 6, &index);
  proc_req(req, conf[index].mode, strlen(req), -1);
}

void set_orca_log(void) {
  set_orca_log_status("orcaap");
  set_orca_log_status("orcadp");
}
