/*
 *  config.c - conf parsing.
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
#include "at_cmd.h"
#include "config.h"
#include "misc.h"
#include "socket_hdl.h"

uint8_t init_conf_file[100] = INIT_CONF_DIR;
uint8_t work_conf_file[100] = WORK_CONF_DIR;
struct conf_file conf[CMD_MAX] = { {"arm", 0, AT_GETARMLOG}, {"cap", 0, AT_GETCAPLOG}, {"armpcm", 0, NULL}, {"etb", 0, AT_GETETB},
                                   {"dsp",0, AT_GETDSPLOG}, {"loglevel", 0, AT_GETLOGLEVEL}, {"event", 0, AT_GETARMLOG}, {"dsppcm", -1, NULL},
                                   {"orcaap", 0, NULL}, {"orcadp", 0, NULL}};

int find_match_item(const uint8_t* buf, size_t name_len, int* index) {
  int ret = -1;
  for (int i = 0; i< CMD_MAX; i++) {
    size_t n = strlen(conf[i].name);
    if (n == name_len && !memcmp(buf, conf[i].name, name_len)) {
      *index = i;
      ret = 0;
      break;
    }
  }
  return ret;
}

int parse_dsp_dest(const uint8_t* buf, int* dest) {
  int ret = 0;
  if (!memcmp(buf, "ap", 2)) {
    *dest = 2;
  } else if (!memcmp(buf, "off", 3)) {
    *dest = 0;
  } else if (!memcmp(buf, "uart", 4)) {
    *dest = 1;
  } else {
    err_log("dsp value error %s", buf);
    ret = -1;
  }
  return ret;
}

int parse_bool_param(const uint8_t* buf, int* mode) {
  int ret = 0;
  if (!memcmp(buf, "on", 2)) {
    *mode = 1;
  } else if (!memcmp(buf, "off", 3)) {
    *mode = 0;
  } else {
    err_log("value error %s", buf);
    ret = -1;
  }
  return ret;
}

int parse_number(const uint8_t* buf, unsigned long* num) {
  char* endp;
  int ret = 0;
  unsigned long value = 0;
  value = strtoul(buf, &endp, 0);
  if ((ULONG_MAX == value && ERANGE == errno) ||
      (' ' != *endp && '\t' != *endp && '\r' != *endp &&
       '\n' != *endp && '\0' != *endp)) {
    err_log("loglevel value error %s", buf);
    ret = -1;
  }
  *num = value;
  return ret;
}

int parse_line(const uint8_t* buf, int line_num) {
  const uint8_t* t1 = NULL;
  const uint8_t* t2 = NULL;
  size_t tlen = 0;
  size_t name_len = 0;
  int index = 0;
  int ret = 0;

  t1 = get_token1(buf, &tlen);
  if (!t1) {
    return -1;
  }
  if ('#' == *t1) {
    return 0;
  }

  if (tlen > MAX_NAME - 1) {
    err_log("conf name too long, line num is %d", line_num);
    return -1;
  }
  name_len = tlen;
  ret = find_match_item(t1,name_len,&index);
  if (ret < 0) {
    err_log("no match item!");
    return -1;
  }

  buf = t1 + tlen;
  t2 = get_token1(buf, &tlen);
  if (!t2) {
    return -1;
  }

  int mode = 0;
  switch (name_len) {
    case 3:
      if (!memcmp(t1, "dsp", 3)) {
        parse_dsp_dest(t2,&mode);
        conf[index].mode = mode;
      } else if (!memcmp(t1, "arm", 3) || !memcmp(t1, "cap", 3)
                 || !memcmp(t1, "etb", 3)) {
        parse_bool_param(t2,&mode);
        conf[index].mode = mode;
      }
      break;
    case 5:
       if (!memcmp(t1, "event", 5)) {
         parse_bool_param(t2,&mode);
         conf[index].mode = mode;
      }
      break;
    case 6:
      if (!memcmp(t1, "armpcm", 6)) {
         parse_bool_param(t2,&mode);
         conf[index].mode = mode;
      } else if (!memcmp(t1, "dsppcm", 6)) {
        parse_bool_param(t2, &mode);
        conf[index].mode = mode;
      } else if (!memcmp(t1, "orcaap", 6)) {
        parse_bool_param(t2, &mode);
        conf[index].mode = mode;
      } else if (!memcmp(t1, "orcadp", 6)) {
        parse_bool_param(t2, &mode);
        conf[index].mode = mode;
      }
      break;
    case 8:
      if (!memcmp(t1, "loglevel", 8)) {
        unsigned long level = 0;
        parse_number(t2, &level);
        conf[index].mode = level;
      }
      break;
    default:
      err_log("name invalid");
      ret = -1;
      break;
  }
  info_log("parse name = %s, mode = %d, index = %d",
           conf[index].name, conf[index].mode, index);

  return ret;
}

int parse_config_file(const uint8_t* config_file_path) {
  info_log("config file is %s",config_file_path);
  FILE* pf = fopen(config_file_path,"rt");
  if (!pf) {
    return -1;
  }
  int line_num = 0;
  uint8_t buf[1024] = {0};

  while (true) {
    uint8_t* p = fgets(buf, 1024, pf);
    if (!p) {
      break;
    }
    int err = parse_line(buf, line_num);
    if (-1 == err) {
      err_log("invalid line:%d in %s", line_num, config_file_path);
    }
    ++line_num;
  }
  fclose(pf);
  return 0;
}

int read_config() {
  if (!access(work_conf_file, R_OK)) { //work file exist
    if (parse_config_file(work_conf_file)) {
      err_log("fail to parse %s", work_conf_file);
    } else {
      return 0;
    }
  }
  if (parse_config_file(init_conf_file)){
    err_log("fail to parse %s", init_conf_file);
    return -1;
  }
  return 0;
}

void set_config_files() {
  strcat(init_conf_file, "mlogservice.conf");
  strcat(work_conf_file, "mlogservice.conf");
}
