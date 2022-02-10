/*
 *  modem_state_hidl_server.cpp - The modem state socket server main function.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2019-06-24 Gloria.He
 *  Initial version.
 */

#include <ctype.h>
#include <inttypes.h>
#include <sys/types.h>
#include "modem_time_sync_hidl_server.h"

ModemTimesyncHIDLServer::ModemTimesyncHIDLServer(sp<IConnectControl>& service,const char* server_name)
    :SocketHIDLServer(-1, -1, server_name, service) {}

void *ModemTimesyncHIDLServer::thread_modem_time_check(void *arg) {
    ((ModemTimesyncHIDLServer *)arg)->process();
    return NULL;
}

void ModemTimesyncHIDLServer::start() {
  int err;
  pthread_t tid;

  SocketHIDLServer::init();

  /* start thread to check status of modem */
  err = pthread_create(&tid, NULL, thread_modem_time_check, this);
  if (err ) {
    ALOGE("warning: pthread create for cp-check fail: %s\n", strerror(err));
  }
  pthread_detach(tid);
}

