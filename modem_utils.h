/*
 *  modem_utils.h - The MODEM utils.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-4-1 Elon li
 *  Initial version.
 */

#ifndef _MODEM_UTILS_H
#define _MODEM_UTILS_H

static const unsigned long DUMP_ONCE_READ_SIZE = (32 * 1024 * 1024);
static const int MAX_REGION_NAME_LEN = 20;
static const int MAX_REGION_CNT = 20;

struct modem_region_info {
  uint64_t address;
  uint32_t size;
  char name[MAX_REGION_NAME_LEN + 1];
};

struct modem_load_info {
  uint32_t region_cnt;
  uint64_t modem_base;
  uint32_t modem_size;
  uint64_t all_base;
  uint32_t all_size;
  modem_region_info regions[MAX_REGION_CNT];
};

/*
 *  get_load_info - get region info.
 *
 *  Return 0 on success, some negative number on failure.
 */
int get_load_info(const char* path, modem_load_info* load_info);

/*
 *  set_dump_region - set dump region.
 *
 *  Return 0 on success, some negative number on failure.
 */
int set_dump_region(const char* path, int region);

int modem_iocmd(unsigned int cmd, const char* path, void* arg);

#endif  //!_MODEM_UTILS_H
