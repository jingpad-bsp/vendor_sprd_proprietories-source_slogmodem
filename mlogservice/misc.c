/*
 *  misc.c - Public function.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-2-13 Elon Li
 *  Initial version.
*/
#include <ctype.h>
#include <cutils/properties.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "misc.h"
#include "socket_hdl.h"

bool is_data_mounted() {
  return 0 == access(WORK_CONF_DIR,W_OK | R_OK | X_OK);
}

int is_gsi_version() {
  uint8_t version[PROPERTY_VALUE_MAX] = {0};

  property_get("ro.product.device", version, "NULL");
  info_log("version is %s",version);
  if (!strcmp(version,"generic_arm_a") ||
      !strcmp(version,"generic_arm64_a") ||
      !strcmp(version,"generic_x86_64_a")) {
    return 1;
  }
  return 0;
}

int data_sync(uint8_t* name, int value) {
  for (int i = 0; i < CMD_MAX; i++) {
    if (!strcmp(conf[i].name, name)) {  //matching name
      if (conf[i].mode != value) {  //dirty
        info_log("%s is dirty, will change to %d", conf[i].name, value);
        conf[i].mode = value;
        return 1;
      } else {
        return 0;
      }
    }
  }
  return -1;
}

enum CmdList parse_cmd(const uint8_t *cmd, size_t len) {
  enum CmdList t = CMD_UNKNOWN;

  switch(len) {
    case 3:
      if (!memcmp(cmd, "ETB", 3)) {
        t = CMD_ETB;
      } else if (!memcmp(cmd, "ARM", 3)) {
        t = CMD_ARMLOG;
      } else if (!memcmp(cmd, "DSP", 3)) {
        t = CMD_DSPLOG;
      } else if (!memcmp(cmd, "CAP", 3)) {
        t = CMD_CAPLOG;
      }
      break;
    case 5:
      if (!memcmp(cmd, "EVENT", 5)) {
        t = CMD_EVENTTRIGGER;
      }
      break;
    case 6:
      if (!memcmp(cmd, "ARMPCM", 6)) {
        t = CMD_ARMPCMLOG;
      }
      break;
    case 8:
      if (!memcmp(cmd, "LOGLEVEL", 8)) {
        t = CMD_LOGLEVEL;
      }
      break;
    default:
      break;
  }

  return t;
}

int parse_first_number(const uint8_t* data, size_t len) {
  const uint8_t* endp = data + len;

  while (data < endp) {
    int n = *data;
    if (n >= '0' && n <= '9') {
      return n-48;
    }
    data++;
  }

  return -1;
}

void strupper(const uint8_t* str, uint8_t* res) {
  for (; *str!='\0'; str++) {
    *res++ = toupper(*str);
  }
}

const uint8_t* get_token1(const uint8_t* buf, size_t* tlen) {
  // Search for the first non-blank
  while (*buf) {
    uint8_t c = *buf;
    if (' ' != c && '\t' != c && '\r' != c && '\n' != c) {
      break;
    }
    ++buf;
  }
  if (!*buf) {
    return 0;
  }

  // Search for the first blank
  const uint8_t* p = buf + 1;
  while (*p) {
    uint8_t c = *p;
    if (' ' == c || '\t' == c || '\r' == c || '\n' == c) {
      break;
    }
    ++p;
  }

  *tlen = p - buf;
  return buf;
}

const uint8_t* get_token(const uint8_t* data, size_t len, size_t* tlen) {
  // Search for the first non-blank
  const uint8_t* endp = data + len;

  while (data < endp) {
    uint8_t c = *data;
    if (' ' != c && '\t' != c && '\r' != c && '\n' != c) {
      break;
    }
    ++data;
  }

  if (data == endp) {
    return 0;
  }

  const uint8_t* p1 = data + 1;
  while (p1 < endp) {
    uint8_t c = *p1;
    if (' ' == c || '\t' == c || '\r' == c || '\n' == c || '\0' == c) {
      break;
    }
    ++p1;
  }

  *tlen = p1 - data;

  return data;
}

const uint8_t* get_sign(const uint8_t* buf, size_t* len) {
  while (*buf) {
    uint8_t c = *buf;
    if ('+' == c) {
      break;
    }
    ++buf;
  }
  if (!*buf) {
    return 0;
  }

  const uint8_t* p = buf + 1;
  while (*p) {
    uint8_t c = *p;
    if ('\r' == c) {
      break;
    }
    ++p;
  }
  *len = p - buf;
  return buf;
}

const uint8_t* search_end(const uint8_t* req, size_t len) {
  const uint8_t* endp = req + len;

  while(req < endp) {
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
