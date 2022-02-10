/*
 *  misc.h - Public function definition.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-2-13 Elon Li
 *  Initial version.
*/
#ifndef _MISC_H_
#define _MISC_H_

#include <cutils/log.h>
#include <errno.h>

#define LOG_TAG "MODEMLSV"
#define MAX_RESPONSE 32
#define MAX_COMMAND_BYTES 128

#define info_log(fmt, arg...) ALOGE("%s: "fmt "\n", __func__, ##arg)
#define err_log(fmt, arg...) \
 ALOGE("%s: "fmt"[%d(%s)]\n", __func__,  ##arg, errno, strerror(errno))

struct DataBuffer {
  uint8_t* buffer;
  size_t buf_size;
  size_t data_start;
  size_t data_len;
};

enum CmdList {
  CMD_UNKNOWN = -1,
  CMD_ARMLOG,
  CMD_CAPLOG,
  CMD_ARMPCMLOG,
  CMD_ETB,
  CMD_DSPLOG,
  CMD_LOGLEVEL,
  CMD_EVENTTRIGGER,
  CMD_DSPPCMLOG,
  CMD_MINIAPLOG,
  CMD_ORCADP,
  CMD_MAX
};

bool is_data_mounted();
int is_gsi_version();
int data_sync(uint8_t* name, int value);
int find(uint8_t* value);
enum CmdList parse_cmd(const uint8_t *p, size_t len);
int parse_first_number(const uint8_t* data, size_t len);
void strupper(const uint8_t* str, uint8_t* res);
const uint8_t* get_token1(const uint8_t* buf, size_t* tlen);
const uint8_t* get_token(const uint8_t* data, size_t len, size_t* tlen);

const uint8_t* get_sign(const uint8_t* buf, size_t* len);
const uint8_t* search_end(const uint8_t* req, size_t len);

#endif // !MISC_H_
