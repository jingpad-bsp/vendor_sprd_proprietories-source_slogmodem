/*
 *  modem_utils.cpp - modem utils function.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-4-1 Elon li
 *  Initial version.
 */

#include <unistd.h>
#include <fcntl.h>

#include "cp_log_cmn.h"
#include "modem_ioctl.h"
#include "modem_utils.h"

int get_load_info(const char* path, modem_load_info* load_info) {
  int ret;

  ret = modem_iocmd(MODEM_READ_LOCK_CMD, path, nullptr);
  ret += modem_iocmd(MODEM_GET_LOAD_INFO_CMD, path, load_info);
  ret += modem_iocmd(MODEM_READ_UNLOCK_CMD, path, nullptr);

  return ret;
}

int set_dump_region(const char* path, int region) {
  int ret;

  ret = modem_iocmd(MODEM_WRITE_LOCK_CMD, path, nullptr);
  ret += modem_iocmd(MODEM_SET_READ_REGION_CMD, path, &region);
  ret += modem_iocmd(MODEM_WRITE_UNLOCK_CMD, path, nullptr);

  return ret;
}

int modem_iocmd(unsigned int cmd, const char* path, void* arg) {
  int ret = -1;
  int fd;

  fd = open(path, O_RDWR);
  if (fd < 0) {
    err_log("open mem path %s error", path);
    return -1;
  }

  ret = ioctl(fd, cmd, (unsigned long)arg);
  if (ret) {
    err_log("ioctl error");
  }

  close(fd);
  return ret;
}
