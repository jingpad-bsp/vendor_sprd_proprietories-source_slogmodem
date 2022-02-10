/*
 *  modem_state_hidl_server.h - The modem state socket server main function.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2019-06-24 Gloria.He
 *  Initial version.
 */
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include "modem_state_hidl_server.h"

ModemStateHIDLServer::ModemStateHIDLServer(sp<IConnectControl>& service,const char* server_name)
    :SocketHIDLServer(-1, -1, server_name, service) {}

void *ModemStateHIDLServer::thread_modem_state_check(void *arg) {
    ((ModemStateHIDLServer *)arg)->process();
    return NULL;
}

void ModemStateHIDLServer::start() {
  int err;
  pthread_t tid;

  SocketHIDLServer::init();

  /* start thread to check status of modem */
  err = pthread_create(&tid, NULL, thread_modem_state_check, this);
  if (err ) {
    ALOGE("warning: pthread create for cp-check fail: %s\n", strerror(err));
  }
  pthread_detach(tid);
}

