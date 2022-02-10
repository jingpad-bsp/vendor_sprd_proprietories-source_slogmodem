/*
 *  cplogctl.cpp - utility program to control slogmodem.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2016-02-17 Yan Zhihang
 *  Initial version.
 *
 *  2016-7-4 Zhang Ziyi
 *  More commands added.
 *  Source files moved into cplogctl directory.
 *  Use libc++ instead of libutils.
 */

/*  Usage: cplogctl <command> [<arg1> [<arg2> ...]]
 *
 *  <command>
 *    clear  (no arguments)
 *    collect [<subsys1> [<subsys2> ...]]
 *      <subsysn> may be one of 5mode, wcn. If no <subsys> is defined,
 *      collect logs from all subsystems.
 *    enable <subsys1> [<subsys2> ...]
 *    enevt <subsys>
 *    disable <subsys1> [<subsys2> ...]
 *      <subsys> may only be 5mode or wcdma. <subsysn> may be 5mode,
 *      wcn, gnss, pmsh or agdsp.
 *    flush  (no arguments)
 *    getstor  (no arguments)
 *    getcpcapacity <subsys> <storage>
 *      <subsys> may be 5mode, wcn, gnss, pmsh or agdsp.
 *      <storage> maybe internal, external
 *    getfilesize <subsys>
 *      <subsys> may be 5mode, wcn, gnss, pmsh or agdsp.
 *    getoverwrite <subsys> (no arguments)
 *      <subsys> may be 5mode, wcn, gnss, pmsh or agdsp.
 *    lastlog <subsys>
 *      <subsys> may be 5mode or wcn.
 *    setaglog <output>
 *      <output> may be off, uart or ap.
 *    savelastlog
 *      save last modem log.
 *    setagpcm <enable>
 *      <enable> may be on or off.
 *    setcpcapacity <subsys> <storage> <size>
 *      <subsys> may be 5mode, wcn, gnss, pmsh or agdsp.
 *      <storage> maybe internal, external
 *      <size> should be positive integer in megabyte(MB). If <size> is 0,
 *      use all free space
 *    setfilesize <subsys> <size>
 *      <subsys> may be 5mode, wcn, gnss, pmsh or agdsp.
 *      <size> should be positive integer in megabyte(MB).
 *    setoverwrite <subsys> <command>
 *      <subsys> may be 5mode, wcn, gnss, pmsh or agdsp.
 *      <command> maybe enable, disable.
 *    setstor <storage>
 *      <storage> maybe internal, external.
 *    startevt
 *      start MODEM event log.
 *    state <subsys>
 *
 *  Exit code: 0 - success, 1 - fail.
 */

#include <algorithm>
#include <signal.h>
#include <stdio.h>

#include "clear_req.h"
#include "collect_req.h"
#include "cplogctl_cmn.h"
#include "en_evt_req.h"
#include "flush_req.h"
#include "get_cp_max_size.h"
#include "get_log_file_size.h"
#include "get_mipi_log.h"
#include "get_preferred_storage.h"
#include "last_log_req.h"
#include "on_off_req.h"
#include "overwrite_req.h"
#include "query_state_req.h"
#include "save_last_log_req.h"
#include "set_ag_log_dest.h"
#include "set_ag_pcm.h"
#include "set_cp_max_size.h"
#include "set_log_file_size.h"
#include "set_mini_ap.h"
#include "set_orca_dp.h"
#include "set_mipi_log.h"
#include "set_preferred_storage.h"
#include "start_evt_req.h"

/*  usage() - usage for modem log control
 */
void usage() {
  fprintf(stderr,
          "Usage: cplogctl <command> [<arg1> [<arg2> ...]]\n\n"
          "<command>\n"
          "  clear  (no arguments)\n"
          "    clear all logs.\n"
          "\n"
          "  collect [<subsys1> [<subsys2> ...]]\n"
          "    <subsysn> may be one of 5mode, wcn. If no <subsys> is defined,\n"
          "    collect logs from all subsystems.\n"
          "\n"
          "  enable <subsys1> [<subsys2> ...]\n"
          "    save logs to SD for specified subsystems\n"
          "\n"
          "  enevt <subsys>\n"
          "    Turn on event log. <subsys> can be one of 5mode and wcdma.\n"
          "\n"
          "  disable <subsys1> [<subsys2> ...]\n"
          "    stop saving logs to SD for specified subsystems\n"
          "    <subsysn> may be 5mode, wcn, gnss, pmsh or agdsp.\n"
          "\n"
          "  flush  (no arguments)\n"
          "    flush all buffered logs.\n"
          "\n"
          "  getcpcapacity <subsys> <storage>\n"
          "    get max size of specified subsystem for log saving.\n"
          "    <subsys> may be 5mode, wcn, gnss, pmsh or agdsp.\n"
          "    <storage> maybe internal, external\n"
          "    query result format:\n"
          "      <size> MB\n"
          "\n"
          "  getfilesize <subsys>\n"
          "    get max size of a single log file of specified subsystem.\n"
          "    <subsys> may be 5mode, wcn, gnss, pmsh or agdsp.\n"
          "    query result format:\n"
          "      <size> MB\n"
          "\n"
          "  getoverwrite <subsys>\n"
          "    query log files' overwrite state\n"
          "    <subsys> may be 5mode, wcn, gnss, pmsh or agdsp.\n"
          "    the result format is:\n"
          "      overwrite:<state> (e.g., overwrite:enabled)\n"
          "\n"
          "  getstor  (no arguments)\n"
          "    get the storage of the highest priority that used.\n"
          "    <storage> maybe internal, external\n"
          "    the result format is:\n"
          "      <storage> (internal or external) \n"
          "\n"
          "  lastlog <subsys>\n"
          "    save the last log of the subsys.\n"
          "    <subsys> may be 5mode or wcn.\n"
          "\n"
          "  savelastlog  (no arguments)\n"
          "    save modem last log"
          "  setaglog <output>\n"
          "    set AG-DSP log output method.\n"
          "    <output> may be off, uart or ap.\n"
          "\n"
          "  setagpcm <enable>\n"
          "    enable/disable AG-DSP PCM dump.\n"
          "    <enable> may be on or off.\n"
          "\n"
          "  setcpcapacity <subsys> <storage> <size>\n"
          "    set max size of specified subsystem for log saving.\n"
          "    <subsys> may be 5mode, wcn, gnss, pmsh or agdsp.\n"
          "    <storage> maybe internal, external\n"
          "    <size> should be any positive integer in megabyte(MB).\n"
          "    If <size> is 0, use all free space.\n"
          "\n"
          "  setfilesize <subsys> <size>\n"
          "    set max size of a single log file of specified subsystem.\n"
          "    <subsys> may be 5mode, wcn, gnss, pmsh or agdsp.\n"
          "    <size> should be positive integer in megabyte(MB).\n"
          "\n"
          "  setoverwrite <subsys> <command>\n"
          "    enable/disable log files overwrite.\n"
          "    <subsys> may be 5mode, wcn, gnss, pmsh or agdsp.\n"
          "    <command> may be enable, disable.\n"
          "\n"
          "  setstor <storage>\n"
          "    set the storage of highest priority to be used.\n"
          "    <storage> maybe internal, external\n"
          "\n"
          "  startevt\n"
          "    start MODEM event log.\n"
          "\n"
          "  state <subsys>\n"
          "    query log state for specified subsystem.\n"
          "    query result format:\n"
          "      <subsys>:<state> (e.g. 5mode:on, wcn:off)\n"
          "\n"
          "Exit code: 0 - success, 1 - fail.\n"
          "\n");
}

int parse_subsys(char** argv, int argc, LogVector<CpType>& types) {
  // <subsysn> may be 5mode, wcn, gnss, pmsh or agdsp.
  bool valid = true;

  for (int i = 0; i < argc; ++i) {
    CpType type;

    if (!strcmp(argv[i], "5mode")) {
      type = CT_5MODE;
    } else if (!strcmp(argv[i], "wcn")) {
      type = CT_WCN;
    } else if (!strcmp(argv[i], "gnss")) {
      type = CT_GNSS;
    } else if (!strcmp(argv[i], "pmsh")) {
      type = CT_PM_SH;
    } else if (!strcmp(argv[i], "agdsp")) {
      type = CT_AGDSP;
    } else if (!strcmp(argv[i], "orcaap")) {
      type = CT_ORCA_MINIAP;
    } else if (!strcmp(argv[i], "orcadp")) {
      type = CT_ORCA_DP;
    } else {
      valid = false;
      types.clear();
      fprintf(stderr, "Unknown subsystem: %s\n", argv[i]);
      break;
    }

    if (types.end() == std::find(types.begin(), types.end(), type)) {
      types.push_back(type);
    }
  }

  return valid ? 0 : -1;
}

SlogmRequest* proc_set_file_size(char** argv, int argc) {
  if (2 != argc) {
    fprintf(stderr, "invalid parameters\n");
    usage();
    return nullptr;
  }

  LogVector<CpType> types;

  if (parse_subsys(argv, 1, types)) {
    fprintf(stderr, "wrong cp name provided\n");
    return nullptr;
  }

  SetLogFileSize* set_size{nullptr};
  unsigned long size;
  const char* endp;

  if (non_negative_number(argv[1], size, endp)) {
    if (spaces_only(endp)) {
      set_size = new SetLogFileSize{types[0], static_cast<unsigned>(size)};
    } else {
      fprintf(stderr, "wrong size number provided\n");
    }
  }

  if (!set_size) {
    fprintf(stderr, "invalid parameter %s\n", argv[1]);
    usage();
  }

  return set_size;
}

SlogmRequest* proc_set_cp_max_size(char** argv, int argc) {
  if (3 != argc) {
    fprintf(stderr, "invalid parameters\n");
    usage();
    return nullptr;
  }

  LogVector<CpType> types;

  if (parse_subsys(argv, 1, types)) {
    fprintf(stderr, "wrong cp name provided\n");
    return nullptr;
  }

  MediaType mt;
  if (!strcmp(argv[1], "internal")) {
    mt = MT_INTERNAL;
  } else if (!strcmp(argv[1], "external")) {
    mt = MT_EXTERNAL;
  } else {
    fprintf(stderr, "invalid media storage type %s\n", argv[1]);
    return nullptr;
  }

  SetCpMaxSize* set_cp{nullptr};
  unsigned long size;
  const char* endp;

  if (non_negative_number(argv[2], size, endp)) {
    if (spaces_only(endp)) {
      set_cp = new SetCpMaxSize{types[0], mt, static_cast<unsigned>(size)};
    } else {
      fprintf(stderr, "wrong size number provided\n");
    }
  }

  if (!set_cp) {
    fprintf(stderr, "invalid size %s\n", argv[1]);
    usage();
  }

  return set_cp;
}

SlogmRequest* proc_get_cp_max_size(char** argv, int argc) {
  if (2 != argc) {
    fprintf(stderr, "invalid parameters\n");
    usage();
    return nullptr;
  }

  LogVector<CpType> types;

  if (parse_subsys(argv, 1, types)) {
    fprintf(stderr, "wrong cp name provided\n");
    return nullptr;
  }

  MediaType mt;
  if (!strcmp(argv[1], "internal")) {
    mt = MT_INTERNAL;
  } else if (!strcmp(argv[1], "external")) {
    mt = MT_EXTERNAL;
  } else {
    fprintf(stderr, "invalid media storage type %s\n", argv[1]);
    return nullptr;
  }

  return new GetCpMaxSize{types[0], mt};
}

SlogmRequest* proc_enable_disable(char** argv, int argc, bool enable) {
  LogVector<CpType> types;

  if (parse_subsys(argv, argc, types)) {
    return nullptr;
  }

  if (!argc) {
    fprintf(stderr, "No subsystem defined\n");
    return nullptr;
  }

  return new OnOffRequest{enable, types};
}

SlogmRequest* proc_get_log_file_size(char** argv, int argc) {
  if (1 != argc) {
    fprintf(stderr, "Invalid command argument\n");
    return nullptr;
  }

  LogVector<CpType> types;

  if (parse_subsys(argv, argc, types)) {
    fprintf(stderr, "wrong cp name provided\n");
    return nullptr;
  }

  return new GetLogFileSize{types[0]};
}

SlogmRequest* proc_enable_evt_log(char** argv, int argc) {
  if (1 != argc) {
    fprintf(stderr, "Invalid command argument\n");
    return nullptr;
  }

  CpType subsys;
  EnableEventLogRequest* evt_req {nullptr};

  if (!strcmp(argv[0], "5mode")) {
    evt_req = new EnableEventLogRequest{CT_5MODE};
    subsys = CT_5MODE;
  } else if (!strcmp(argv[0], "wcdma")) {
    evt_req = new EnableEventLogRequest{CT_WCDMA};
  } else {
    fprintf(stderr, "Invalid subsystem %s\n", argv[0]);
  }

  return evt_req;
}

SlogmRequest* proc_get_overwrite_state(char** argv, int argc) {
  if (1 != argc) {
    fprintf(stderr, "invalid parameters\n");
    usage();
    return nullptr;
  }

  LogVector<CpType> types;

  if (parse_subsys(argv, argc, types)) {
    fprintf(stderr, "wrong cp name provided\n");
    return nullptr;
  }

  return new OverwriteRequest{types[0], OverwriteRequest::OCT_QUERY};
}

SlogmRequest* proc_set_preferred_storage(char** argv, int argc) {
  if (1 != argc) {
    fprintf(stderr, "invalid parameters\n");
    usage();
    return nullptr;
  }

  MediaType mt;
  if (!strcmp(argv[0], "internal")) {
    mt = MT_INTERNAL;
  } else if (!strcmp(argv[0], "external")) {
    mt = MT_EXTERNAL;
  } else {
    fprintf(stderr, "invalid media storage type %s\n", argv[0]);
    return nullptr;
  }

  return new SetPreferredStorage(mt);
}

SlogmRequest* proc_set_miniap_log(char** argv, int argc) {
  if (1 != argc) {
    fprintf(stderr, "invalid parameters\n");
    usage();
    return nullptr;
  }

  bool state;
  if (!strcmp(argv[0], "on")) {
    state = true;
  } else if (!strcmp(argv[0], "off")) {
    state = false;
  } else {
    fprintf(stderr, "invalid miniap state %s\n", argv[0]);
    return nullptr;
  }

  return new SetMiniAp(state);
}

SlogmRequest* proc_set_orcadp_log(char** argv, int argc) {
  if (1 != argc) {
    fprintf(stderr, "invalid parameters\n");
    usage();
    return nullptr;
  }

  bool state;
  if (!strcmp(argv[0], "on")) {
    state = true;
  } else if (!strcmp(argv[0], "off")) {
    state = false;
  } else {
    fprintf(stderr, "invalid orcadp state %s\n", argv[0]);
    return nullptr;
  }

  return new SetOrcaDp(state);
}

SlogmRequest* proc_set_mipi_log_info(char** argv, int argc) {
  if (3 != argc) {
    fprintf(stderr, "invalid parameters\n");
    usage();
    return nullptr;
  }
  err_log("argv[0]: %s", argv[0]);
  LogString type;
  str_assign(type, argv[0], strlen(argv[0]));
  LogString channel;
  str_assign(channel, argv[1], strlen(argv[1]));
  LogString freq;
  str_assign(freq, argv[2], strlen(argv[2]));

  return new SetMipiLog(type, channel, freq);
}

SlogmRequest* proc_get_mipi_log_info(char** argv, int argc) {
  if (1 != argc) {
    fprintf(stderr, "invalid parameters\n");
    usage();
    return nullptr;
  }
  info_log("argv[0]: %s", argv[0]);
  LogString type;
  str_assign(type, argv[0], strlen(argv[0]));

  return new GetMipiLog(type);
}

SlogmRequest* proc_overwrite(char** argv, int argc) {
  if (2 != argc) {
    fprintf(stderr, "invalid parameters\n");
    usage();
    return nullptr;
  }

  LogVector<CpType> types;

  if (parse_subsys(argv, 1, types)) {
    return nullptr;
  }

  if (!strcmp(argv[1], "enable")) {
    return new OverwriteRequest{types[0], OverwriteRequest::OCT_ENABLE};
  } else if (!strcmp(argv[1], "disable")) {
    return new OverwriteRequest{types[0], OverwriteRequest::OCT_DISABLE};
  } else {
    fprintf(stderr, "invalid parameters\n");
    usage();
    return nullptr;
  }
}

SlogmRequest* proc_query_state(char** argv, int argc) {
  if (1 != argc) {
    fprintf(stderr, "invalid parameters\n");
    usage();
    return nullptr;
  }

  LogVector<CpType> types;

  if (parse_subsys(argv, argc, types)) {
    return nullptr;
  }

  return new QueryStateRequest{argv[0], types[0]};
}

SlogmRequest* proc_save_last_log(char** argv, int argc) {
  if (argc) {
    fprintf(stderr, "There shall be no argument for save last log command\n");
    return nullptr;
  }

  return new SaveLastLogRequest;
}

SlogmRequest* proc_agdsp_log_dest(char** argv, int argc) {
  if (1 != argc) {
    fprintf(stderr, "invalid parameters\n");
    usage();
    return nullptr;
  }

  // <output> may be off, uart or ap.
  SetAgdspLogDest* req{nullptr};
  SetAgdspLogDest::AgDspLogDest dest;
  bool valid = true;

  if (!strcmp(argv[0], "off")) {
    dest = SetAgdspLogDest::AGDSP_LOG_DEST_OFF;
  } else if (!strcmp(argv[0], "uart")) {
    dest = SetAgdspLogDest::AGDSP_LOG_DEST_UART;
  } else if (!strcmp(argv[0], "ap")) {
    dest = SetAgdspLogDest::AGDSP_LOG_DEST_AP;
  } else {
    fprintf(stderr, "Invalid AG-DSP log destination %s\n", argv[0]);
    valid = false;
  }

  if (valid) {
    req = new SetAgdspLogDest{dest};
  }

  return req;
}

SlogmRequest* proc_agdsp_pcm(char** argv, int argc) {
  if (1 != argc) {
    fprintf(stderr, "invalid parameters\n");
    usage();
    return nullptr;
  }

  // <output> may be off, uart or ap.
  SetAgdspPcm* req{nullptr};
  bool pcm_on;
  bool valid = true;

  if (!strcmp(argv[0], "off")) {
    pcm_on = false;
  } else if (!strcmp(argv[0], "on")) {
    pcm_on = true;
  } else {
    fprintf(stderr, "Invalid AG-DSP PCM setting %s\n", argv[0]);
    valid = false;
  }

  if (valid) {
    req = new SetAgdspPcm{pcm_on};
  }

  return req;
}

static SlogmRequest* proc_collect(char** argv, int argc) {
  LogVector<CpType> subsys;

  if (parse_subsys(argv, argc, subsys)) {
    fprintf(stderr, "Invalid subsystems\n");
    return nullptr;
  }

  return new CollectRequest{subsys};
}

static SlogmRequest* proc_start_evt(char** argv, int argc) {
  if (argc) {
    fprintf(stderr, "There shall be no argument for startevt command\n");
    return nullptr;
  }

  return new StartEventLogRequest;
}

static SlogmRequest* proc_last_log(char** argv, int argc) {
  if (!argc) {
    fprintf(stderr, "No subsystem defined\n");
    return nullptr;
  }

  if (1 != argc) {
    fprintf(stderr, "Only one subsystem can be defined\n");
    return nullptr;
  }

  CpType ct{CT_UNKNOWN};

  if (!strcmp(argv[0], "5mode")) {
    ct = CT_5MODE;
  } else if (!strcmp(argv[0], "wcn")) {
    ct = CT_WCN;
  } else {
    fprintf(stderr, "Unknown subsystem: %s\n", argv[0]);
    return nullptr;
  }

  return new LastLogRequest{ct};
}

int main(int argc, char** argv) {
  int ret = 1;

  if (argc < 2) {
    fprintf(stderr, "Missing argument.\n");
    usage();
    return ret;
  }

  // Ignore SIGPIPE to avoid to be killed by the kernel
  // when writing to a socket which is closed by the peer.
  struct sigaction siga;

  memset(&siga, 0, sizeof siga);
  siga.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &siga, 0);

  SlogmRequest* req{nullptr};
  info_log("The cmd is %s", argv[1]);
  if (!strcmp(argv[1], "clear")) {
    if (2 != argc) {
      fprintf(stderr, "clear command does not have any arguments\n");
    } else {
      req = new ClearRequest;
    }
  } else if (!strcmp(argv[1], "collect")) {
    req = proc_collect(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "disable")) {
    req = proc_enable_disable(argv + 2, argc - 2, false);
  } else if (!strcmp(argv[1], "enable")) {
    req = proc_enable_disable(argv + 2, argc - 2, true);
  } else if (!strcmp(argv[1], "enevt")) {
    req = proc_enable_evt_log(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "flush")) {
    if (2 != argc) {
      fprintf(stderr, "flush command does not have any arguments\n");
    } else {
      req = new FlushRequest;
    }
  } else if (!strcmp(argv[1], "getcpcapacity")) {
    req = proc_get_cp_max_size(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "getfilesize")) {
    req = proc_get_log_file_size(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "getoverwrite")) {
    req = proc_get_overwrite_state(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "getstor")) {
    if (2 != argc) {
      fprintf(stderr, "getstor command has no arguments\n");
    } else {
      req = new GetPreferredStorage;
    }
  } else if (!strcmp(argv[1], "lastlog")) {
    req = proc_last_log(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "savelastlog")) {
    req = proc_save_last_log(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "setaglog")) {
    req = proc_agdsp_log_dest(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "setagpcm")) {
    req = proc_agdsp_pcm(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "setcpcapacity")) {
    req = proc_set_cp_max_size(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "setfilesize")) {
    req = proc_set_file_size(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "setstor")) {
    req = proc_set_preferred_storage(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "setoverwrite")) {
    req = proc_overwrite(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "startevt")) {
    req = proc_start_evt(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "state")) {
    req = proc_query_state(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "setminiaplog")) {
    req = proc_set_miniap_log(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "setorcadplog")) {
    req = proc_set_orcadp_log(argv + 2, argc - 2);
  } else if (!strcmp(argv[1], "setmipilog")) {
    req = proc_set_mipi_log_info(argv + 2, argc -2);
  } else if (!strcmp(argv[1], "getmipilog")) {
    req = proc_get_mipi_log_info(argv + 2, argc -2);
  } else {
    fprintf(stderr, "Invalid command: %s\n", argv[1]);
  }

  if (req) {
    if (0 == req->exec()) {
      ret = 0;
    }
    delete req;
  }

  return ret;
}
