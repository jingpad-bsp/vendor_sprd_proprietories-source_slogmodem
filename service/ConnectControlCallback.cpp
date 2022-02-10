/*
 *  connectcontrolcallback.cpp - main socket communication functions.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2019-6-24 Gloria He
 *  Initial version.
 */
#include <cutils/sockets.h>
#include <cutils/properties.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <log/log.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include "connect_cmn.h"
#include "ConnectControlCallback.h"


#define MAX_CLIENT_NUM 8
#define ARRAY_LEN(A) (sizeof(A)/sizeof((A)[0]))
#define MAX_RETRY_TIME 5

static struct poll_pair_entry sock_pair[MAX_CLIENT_NUM] = {{-1,-1},{-1,-1},{-1,-1},{-1,-1},{-1,-1},{-1,-1},{-1,-1},{-1,-1}};

static struct pollfd pfds[MAX_CLIENT_NUM];

namespace vendor {
namespace sprd {
namespace hardware {
namespace cplog_connmgr {
namespace V1_0 {
namespace implementation  {

// Methods from ILogCallback follow.
ConnectControlCallback::ConnectControlCallback(bool has_notifier_client, const char* socket_name,
                                               sp<IConnectControl>& service)
    :has_notifier_client_{has_notifier_client},
    socket_name_{socket_name},
    service_{service},
    ms_hidl_server_{NULL},
    ws_hidl_server_{NULL},
    mt_hidl_server_{NULL} {}

ConnectControlCallback::~ConnectControlCallback() {
  if (ms_hidl_server_) {
    delete ms_hidl_server_;
  }
  if (ws_hidl_server_) {
    delete ws_hidl_server_;
  }
  if (mt_hidl_server_) {
     delete mt_hidl_server_;
  }
}

Return<void> ConnectControlCallback::onCmdresp(int sock, const hidl_string& cmd, int len,
                                               onCmdresp_cb _hidl_cb) {
    // TODO implement
    hidl_string ret;
    char response[256] = {0};
    int size = sizeof(response);

    const char *p = cmd.c_str();
    info_log("sock == %d, cmd == %s", sock, p);
    int n = write(sock,p,len);
    if (n == len) {
      snprintf(response, size-1, "%s", "cmd response send sucess");
    } else {
      snprintf(response, size-1, "%s", "cmd response send fail");
    }

    ret.setToExternal(response, strlen(response));
    _hidl_cb(ret);
    return Void();
}

Return<void> ConnectControlCallback::onTimeresp(int sock,
    const ::vendor::sprd::hardware::cplog_connmgr::V1_0::time_sync& time, int len) {

  struct time_info timeinfo;
  timeinfo.sys_cnt = time.sys_cnt;
  timeinfo.uptime = time.uptime;

  ssize_t n = write(sock,&timeinfo,sizeof(struct time_info));
  if (static_cast<size_t>(n) == len) {
    info_log("write time sync to slogmodem success ");
  } else {
    err_log("write time sync to slogmodem fail ");
  }

  return Void();
}

Return<void> ConnectControlCallback::onSocketclose(int sock) {
    // TODO implement
  for(int i = 0; i < MAX_CLIENT_NUM; i++) {
    if(sock == sock_pair[i].client) {
      close(sock_pair[i].fd);
      sock_pair[i].fd = -1;
      sock_pair[i].client= -1;
      break;
    }
  }
  return Void();
}

Return<void> ConnectControlCallback::onCmdreq(int sock, const hidl_string& cmd,
                                              int len,onCmdreq_cb _hidl_cb) {

  hidl_string ret;
  char response[256] = {0};
  int size = sizeof(response);
  int ret_val = 0, cnt = 0;

  const char* p = cmd.c_str();
  do {
    ret_val = process_reqs(sock,p, len,response, size - 1);
    ++cnt;
  } while (ret_val < 0 && cnt < MAX_RETRY_TIME);

  ret.setToExternal(response, strlen(response));
  _hidl_cb(ret);
  return Void();
}

int ConnectControlCallback::connect_socket_local_server() {
  int fd = -1;
  do {
    fd = socket_local_client(socket_name_, ANDROID_SOCKET_NAMESPACE_ABSTRACT,
                             SOCK_STREAM);

    if (fd < 0) {
      info_log("waiting for modem log service ready -> msleep(200)");
      usleep(200*1000);
    } else {
      long flags = fcntl(fd, F_GETFL);
      flags |= O_NONBLOCK;
      int err = fcntl(fd, F_SETFL, flags);
      if (-1 == err) {
        close(fd);
        err_log("set time sync socket O_NONBLOCK fail.");
        continue;
      }
      break;
    }
  } while(1);

  return fd;
}

void ConnectControlCallback::process_close() {
  // set sock -pair invalid, need reconnect when communication
  for(int i = 0; i < MAX_CLIENT_NUM; i++) {
    if(sock_pair[i].client != -1) {
      close(sock_pair[i].fd); //Coverity ID:789635
      sock_pair[i].fd = -1;
      sock_pair[i].client = -1;
    }
  }
}

void ConnectControlCallback::is_notifier_client(const char* cmd, int fd, int client) {
  const char* cmd_str = strstr(cmd, "SUBSCRIBE WCN DUMP");
  if(cmd_str) {
    dump_notifier_.fd = fd;
    dump_notifier_.client = client;
    has_notifier_client_ = true;
  }
}

int ConnectControlCallback::process_reqs(int sock, const char *cmd, int len,
                                         char *response, int size) {

  bool has_connected = 0;
  char buf[256] = {0};
  char return_buf[256] = {0};
  struct pollfd pfd[2];

  int fd = -1;
  int ret;

  info_log(" process_reqs:%s,len=%d", cmd,len);

  memcpy(buf,cmd,strlen(cmd));

  for (int i = 0;i < MAX_CLIENT_NUM;i++) {
    if (sock == sock_pair[i].client) {
      info_log("has connected already");
      has_connected = 1;
      fd = sock_pair[i].fd;
      break;
    }
  }

  if (has_connected != 1) {
    fd = connect_socket_local_server();
    for (int i = 0;i < MAX_CLIENT_NUM;i++) {
      if (sock_pair[i].fd == -1) {
        sock_pair[i].fd = fd;
        sock_pair[i].client = sock;
        info_log("sock_pair[%d].fd=%d,sock_pair[%d].client=%d",
                 i,sock_pair[i].fd,i,sock_pair[i].client);
        break;
      }
    }
  }

  ssize_t written;
  int cnt{0};
  do {
    if (cnt > 0) {
      err_log("write to fd %d, cnt = %d, written = %d", fd, cnt, written);
      usleep(5 * 1000);
    }
    written = write(fd, buf, len);
    ++cnt;
  } while (written < 0 && (EAGAIN == errno || EINTR == errno) && cnt < MAX_RETRY_TIME);

  if (written < 0) {
    process_close();
    snprintf(response, size, "%s", "ERROR\n");
    return 0;
  }

  pfd[0].fd = fd;
  pfd[0].events = POLLIN;
  pfd[0].revents = 0;
  int retry_cnt = 3;
  for (;;) {
    if ((ret = poll(pfd, 1, 2000)) > 0) {
      if (pfd[0].revents) {
        info_log("get events from slogmodem");
        ret = read(pfd[0].fd, return_buf, sizeof(return_buf));
        if (ret > 0) {
          info_log("get response: %s", return_buf);
          snprintf(response, size, "%s", return_buf);
          break;
        } else if (0 == ret) {
          err_log("slogmodem socket close");
          process_close();
          snprintf(response, size, "%s", "ERROR\n");
          return 0;
        } else if (-1 == ret) {
          err_log("slogmodem socket error");
          process_close();
          snprintf(response, size, "%s", "ERROR\n");
          return -1;
        }
      } else {
        err_log("revent is null");
      }
    } else {
      retry_cnt--;
      if(retry_cnt <= 0){
        err_log("retry_cnt <= 0, return");
        process_close();
        snprintf(response, size, "%s", "ERROR\n");
        return -1;
      } else {
        err_log("poll error or timeout, try again");
      }
    }
  }

  is_notifier_client(cmd, fd, sock);

  return 0;
}

void* ConnectControlCallback::thread_check_dump(void *arg) {
  ((ConnectControlCallback*)arg)->dump_notifier_process();
  return NULL;
}

void ConnectControlCallback::dump_notifier_process() {
  struct pollfd fds[2];

  while (1) {
    if(has_notifier_client()) {
      fds[0].fd = dump_notifier_.fd;
      fds[0].events = POLLIN;
    } else {
      err_log("It has no notifier client");
      usleep(2000 * 1000);
      continue;
    }

    char data[256] ={0};
    int ret = poll(fds, 1, 2000);
    if (ret > 0) {
      if (POLLIN & fds[0].revents) {
        int n = read(fds[0].fd,data,sizeof(data));
        if (n > 0) {
          std::string pStr;
          auto cb = [&](hidl_string atResp) {
            pStr = atResp.c_str();
          };
          Return<void> status = service_->socketData(
              dump_notifier_.client, data, cb);
          if (!status.isOk()) {
            err_log("socket send data fail");
          } else {
            info_log("response from service %s", pStr.c_str());
          }
        } else if (0 == n) {
          err_log("slogmodem socket close");
          dump_notifier_.fd = connect_socket_local_server();
          write(dump_notifier_.fd, "SUBSCRIBE 5MODE DUMP\n",21);
          write(dump_notifier_.fd, "SUBSCRIBE WCN DUMP\n",19);
        } else if (-1 == n) {
          err_log("slogmodem socket error");
          if (EAGAIN != errno && EINTR != errno) {
            dump_notifier_.fd = connect_socket_local_server();
            write(dump_notifier_.fd, "SUBSCRIBE 5MODE DUMP\n",21);
            write(dump_notifier_.fd, "SUBSCRIBE WCN DUMP\n",19);
          }
        }
      } else if ((POLLERR | POLLNVAL) & fds[0].revents) {
        info_log("revents is POLLNVAL or POLLERR");
        usleep(2000 * 1000);
      }
    }
  }
}

void ConnectControlCallback::start(sp<IConnectControl>& service) {

  ms_hidl_server_ = new ModemStateHIDLServer(service,SOCKET_NAME_HIDL_MODEMD);
  ms_hidl_server_->start();
  ws_hidl_server_ = new WcnStateHIDLServer(service,SOCKET_NAME_HIDL_WCND);
  ws_hidl_server_->start();
  mt_hidl_server_ = new ModemTimesyncHIDLServer(service,SOCKET_NAME_HIDL_CP_TIME_SYNC);
  mt_hidl_server_->start();

  pthread_t notifier_tid;
  int err = pthread_create(&notifier_tid, NULL, thread_check_dump, this);
  if (err ) {
    err_log("warning: pthread create for cp-check fail: %s\n", strerror(err));
  }
  pthread_detach(notifier_tid);
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace slogmodemconnmgr
}  // namespace hardware
}  // namespace sprd
}  // namespace vendor
