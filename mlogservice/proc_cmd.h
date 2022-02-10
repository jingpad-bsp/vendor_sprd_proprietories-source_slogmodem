/*
 *  proc_cmd.h - resolve customer requests definition.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-2-13 Elon Li
 *  Initial version.
*/
#ifndef _PROC_CMD_H_
#define _PROC_CMD_H_

int proc_set(const uint8_t* req, size_t len, int c_fd);
int proc_get(const uint8_t* req, size_t len, int c_fd);
int proc_req(const uint8_t* cmd, const int addvalue, const int len, int c_fd);
int write_work_file();
void boot_action(void);
void set_orca_log_status(const uint8_t* item_name);
void set_orca_log(void);

#endif // !PROC_CMD_H_
