/*
 *  modem_ioctl.h - The MODEM ioctl commands.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-4-1 Elon li
 *  Initial version.
 */

#ifndef _MODEM_IOCTL_H
#define _MODEM_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ORCA modem io cmd */
#define MODEM_MAGIC 'M'

#define MODEM_READ_LOCK_CMD _IO(MODEM_MAGIC, 0x1)
#define MODEM_READ_UNLOCK_CMD _IO(MODEM_MAGIC, 0x2)

#define MODEM_WRITE_LOCK_CMD _IO(MODEM_MAGIC, 0x3)
#define MODEM_WRITE_UNLOCK_CMD _IO(MODEM_MAGIC, 0x4)

#define MODEM_GET_LOAD_INFO_CMD _IOR(MODEM_MAGIC, 0x5, modem_load_info)

#define MODEM_SET_READ_REGION_CMD _IOR(MODEM_MAGIC, 0x7, int)

#ifdef __cplusplus
}
#endif

#endif  //!_MODEM_IOCTL_H
