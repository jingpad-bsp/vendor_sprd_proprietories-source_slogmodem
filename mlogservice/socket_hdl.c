/*
 *  socket_hdl.c - Socket send and receive.
 *
 *  Copyright (C) 2019 UNISOC Communications Inc.
 *
 *  History:
 *  2019-2-13 Elon Li
 *  Initial version.
*/
#include <cutils/sockets.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include<pthread.h>
#include "at_cmd.h"
#include "cp_logctl.h"
#include "misc.h"
#include "parse_at.h"
#include "socket_hdl.h"

static int at_dev = -1;

extern mutex;

int send_response(int conn, enum ResponseErrorCode err) {
  uint8_t res[MAX_RESPONSE] = {0};
  size_t len;
  int ret = -1;

  if (REC_SUCCESS == err) {
    memcpy(res, "OK\n", 3);
    len = 3;
  } else {
    len = snprintf(res, sizeof(res), "ERROR %d\n", err);
  }
  ssize_t n = write(conn, res, len);
  if (n == len) {
    ret = 0;
  }
  info_log("response is %s, len is %d, n is %d",res, len, n);
  return ret;
}

int wait_response(int fd, uint8_t* res, int wait_time) {
  struct pollfd pol;

  pol.fd = fd;
  pol.events = POLLIN;
  pol.revents = 0;

  int err = poll(&pol, 1, wait_time);
  if (err < 0) { //error
    err = -1;
    err_log("poll error ");
  } else if (0 == err) {
    err_log("poll time out");
    err = -1;
  } else if (pol.revents & POLLIN) {
    ssize_t nr = read(fd, res, MAX_AT_RESPONSE - 1);
    if (0 == nr) {  //controlled system close the conection
      err = -1;
      err_log("channel was closed");
      at_dev = -1;
    } else if (nr < 0) {
      err = -1;
      at_dev = -1;
      err_log("read error,read result = %d",nr);
    } else {
      err = 0;
    }
  }
  return err;
}

int sersendCmd(const uint8_t* cmd, char* res, uint8_t* socketName) {
  int err = -1;
  int c_fd = -1;

  c_fd = socket_local_client(socketName, ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
  if (c_fd < 0) {
    err_log("can't connect to %s", socketName);
    c_fd = -1;
  }
  ssize_t nw = write(c_fd, cmd, strlen(cmd));
  info_log("send cmd %s to slogmodem, %d, writened %d", cmd, strlen(cmd), nw);
  if (nw != strlen(cmd)) {
    info_log("request error");
    if (c_fd > 0) {
      close(c_fd);
    }
    return -1;
  }
  if (c_fd > 0) {
    close(c_fd);
  }
  return wait_response(c_fd, res, 3000);
}

int close_at_channel(void) {
  if (at_dev >= 0) {
    close(at_dev);
    at_dev = -1;
  }
  return 0;
}

int open_at_channel(void) {
  int fd = -1;
  while (1) {
    fd = open(AT_CHANNEL, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
      err_log("open at channel fail, sleep for a while");
      usleep(400 * 1000);
    } else {
      break;
    }
  }
  at_dev = fd;
  return 0;
}

int send_at(const uint8_t* cmd, int len, int* fd) {
  if (*fd < 0) {
    if ((*fd = open(AT_CHANNEL, O_RDWR | O_NONBLOCK)) < 0) {
      err_log("open channel error");
      return -1;
    }
  }
  ssize_t nw = write(*fd, cmd, len);

  if (len != nw) {
    err_log("send at command %s error,write return %d", (const char* )cmd, nw);
    return -1;
  }
  info_log("sended command %s, len is %d", (const char* )cmd, len);
  return 0;
}

int send_at_and_get_result(const uint8_t* cmd, uint8_t* res, int len) {
  int retry_cnt = 60;
  int err = -1;

  while (retry_cnt--) {
    err = send_at(cmd, len, &at_dev);
    if (err < 0) {
      usleep(400 * 1000);
    } else if (!err) {
      break;
    }
  }

  if (err < 0) {
    err_log("send at error");
    return -1;
  }

  retry_cnt = 4;
  while (retry_cnt--) {
    if (wait_response(at_dev, res, 3000)) {
      err_log("send cmd %s no response, try again", cmd);
    } else {
      return 0;
    }
  }
  return -1;
}

int send_at_parse_result(const uint8_t* cmd, uint8_t* res, int len) {
  pthread_mutex_lock(&mutex);
  int r_parse = -1;

  int  r_send = send_at_and_get_result(cmd, res, len);
  if (!r_send) {
    r_parse = parse_at_res(cmd, res, strlen(res));
  } else {
    err_log("send at still error");
    pthread_mutex_unlock(&mutex);
    return -1;
  }
  pthread_mutex_unlock(&mutex);
  return r_parse;
}
