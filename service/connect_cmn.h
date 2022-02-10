/*
 *  connect_cmn.h - Common functions declarations.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2019-6-24 Gloria He
 *  Initial version.
 */
#ifndef _CONNECT_CMN_H_
#define _CONNECT_CMN_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <errno.h>
#include <cutils/log.h>

#define SOCKET_NAME_HIDL_MODEMD "hidl_modemd"
#define SOCKET_NAME_HIDL_WCND "hidl_wcnd"
#define SOCKET_NAME_HIDL_CP_TIME_SYNC "hidl_cp_time_sync_server"


#define info_log(fmt, arg...) ALOGE("%s: " fmt "\n", __func__, ##arg)
#define err_log(fmt, arg...) \
  ALOGE("%s: " fmt " [%d(%s)]\n", __func__, ##arg, errno, strerror(errno))


struct time_info {
  uint32_t sys_cnt;
  uint32_t uptime;
}__attribute__((__packed__));

struct poll_pair_entry {
    int fd;
    int client;
};


#endif  // !_CONNECT_CMN_H_
