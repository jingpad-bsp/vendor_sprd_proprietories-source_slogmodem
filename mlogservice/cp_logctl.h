/*
 *  cp_logctl.h - Handling AT commands definition.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-2-13 Elon Li
 *  Initial version.
*/
#ifndef _CP_LOGCTL_H_
#define _CP_LOGCTL_H_

#define MINI_AP_KERNEL_LOG_PROP "persist.vendor.miniap.log"

void close_at(void);
void open_at(void);
void check_at(const int num);
int proc_etb(const int value, const int socket);
int proc_arm(const int value, const int socket);
int proc_cap(const int value, const int socket);
int proc_dsp(const int value, const int socket);
int proc_evt(const int value, const int socket);
int proc_loglevel(const int value, const int socket);
int proc_armpcm(const int value, const int socket);
int proc_dsppcm(const int value, const int socket);
int proc_miniaplog(const int value, const int socket);
int proc_orcadplog(const int value, const int socket);


#endif // !CP_LOGCTL_H_
