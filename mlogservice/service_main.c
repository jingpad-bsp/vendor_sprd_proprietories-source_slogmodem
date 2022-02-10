/*
 *  service_main.c - main fuction.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-2-13 Elon Li
 *  Initial version.
*/
#include <cutils/sockets.h>
#include <cutils/properties.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "config.h"
#include "cp_logctl.h"
#include "client_hdl.h"
#include "misc.h"
#include "proc_cmd.h"
#include "socket_hdl.h"

pthread_mutex_t mutex;

int main() {
  struct sigaction siga;

  info_log("modem_log_service start ID=%u/%u, GID=%u/%u",
           getuid(), geteuid(), getgid(), getegid());

  set_config_files();

  while (!is_data_mounted()) {
    sleep(1);
  }

  umask(0);

  memset(&siga, 0, sizeof siga);
  siga.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &siga, 0);

  int err = pthread_mutex_init(&mutex, NULL);
  if (err) {
    err_log("pthread_mutex_init error %d", err);
    return 1;
  }

  err = read_config();
  if (err < 0) {
    return 4;
  }

  set_orca_log();
  boot_action();
  client_process();

  pthread_mutex_destroy(&mutex);
  return 0;
}
