/*
 *  client_hdl.h - Client data process definition.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-2-13 Elon Li
 *  Initial version.
*/
#ifndef _CLIENT_HDL_H_
#define _CLIENT_HDL_H_

#define LS_DATA_LENGTH 256

int client_process(void);
void* ls_socket_rw_thread(void* c_fd);

#endif // !CLIENT_HDL_H_