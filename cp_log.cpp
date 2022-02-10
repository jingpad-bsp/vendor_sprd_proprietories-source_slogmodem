/*
 *  cp_log.cpp - The main function for the CP log and dump program.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-2-16 Zhang Ziyi
 *  Initial version.
 */

/*  This is the service program for CP log on T-card.
 *  Usage:
 *    slogmodem [–-test-ic <initial_conf>] [--test-conf <working_conf>]
 *
 *    –-test-ic <initial_conf>
 *    Specify test mode initial configuration file path.
 *    /system/etc/test_mode.conf is used in test mode (calibration or autotest)
 *    /system/etc/slog_mode.conf is used by default.
 *
 *    --test-conf <working_conf>
 *    Specify working configuration file in test mode (calibration mode or
 *    autotest mode). <working conf> shall only include the file name.
 */

#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "def_config.h"
#include "log_ctrl.h"
#include "log_config.h"

static LogController log_controller;
static LogConfig log_config;

static void join_to_cgroup_blockio(void) {
  char path[96];
  if ((access("/sys/fs/cgroup/blkio/other/tasks", 0) == 0) &&
      (access(CP_GROUP_EN, 0) == 0)) {
    snprintf(path, sizeof path,
             "echo %lu > /sys/fs/cgroup/blkio/other/tasks",
             static_cast<unsigned long>(getpid()));
    info_log( "slogmodem join to cgroup: [%s]\n", path);
    system(path);
  }
}

static void set_config_files(SystemRunMode rm, LogConfig& log_config) {
  LogString init_conf{INIT_CONF_DIR};
  LogString work_conf{WORK_CONF_DIR};
  const char* file_name{};

  switch (rm) {
    case SRM_NORMAL:
      file_name = kNormalConfName;
      break;
    case SRM_AUTOTEST:
      file_name = kAutoTestConfName;
      break;
    case SRM_CALIBRATION:
      file_name = kCaliConfName;
      break;
    default:  // case SRM_FACTORY_TEST:
      file_name = kFactoryConfName;
      break;
  }

  init_conf += file_name;
  work_conf += file_name;

  log_config.set_conf_files(init_conf, work_conf);
}

int main() {
  info_log("slogmodem start ID=%u/%u, GID=%u/%u",
           static_cast<unsigned>(getuid()), static_cast<unsigned>(geteuid()),
           static_cast<unsigned>(getgid()), static_cast<unsigned>(getegid()));

  if (is_vendor_version()) {
    char device[PROPERTY_VALUE_MAX];
    get_sys_gsi_flag(device);
    if (strstr(device, "generic") == NULL) {
       info_log("vendor process run on %s,sleep...", device);
       while (1) {
         sleep(120);
       }
    }
  }

  SystemRunMode rm{SRM_NORMAL};

  if (get_sys_run_mode(rm)) {
    err_log("can not get system run mode");
    return 3;
  }

  // slogmodem shall do nothing in charger mode or recovery mode.
  if (SRM_RECOVERY == rm || SRM_CHARGER == rm) {
    for (;;) {
      sleep(100000000);
    }
  }

  // Set config files
  set_config_files(rm, log_config);

  // Wait until /data partition is mounted.
  while (!is_data_mounted()) {
    if(SRM_FACTORY_TEST == rm) {
      info_log("SRM_FACTORY_TEST enter...");
      break;
    }
    sleep(1);
  }

  umask(0);

  int err = log_config.read_config();
  if (err < 0) {
    return 1;
  }

  // Ignore SIGPIPE to avoid to be killed by the kernel
  // when writing to a socket which is closed by the peer.
  struct sigaction siga;

  memset(&siga, 0, sizeof siga);
  siga.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &siga, 0);

  if (log_controller.init(&log_config) < 0) {
    return 2;
  }
  join_to_cgroup_blockio();
  return log_controller.run();
}
