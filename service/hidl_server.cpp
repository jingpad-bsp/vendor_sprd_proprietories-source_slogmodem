/*
 *  hidl_server.cpp - The hidl socket server base function.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2019-06-24 Gloria.He
 *  Initial version.
 */


#include <ctype.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <cutils/sockets.h>
#include "connect_cmn.h"
#include "hidl_server.h"
#include "ConnectControlCallback.h"

#include <vendor/sprd/hardware/cplog_connmgr/1.0/IConnectControlCallback.h>

SocketHIDLServer::SocketHIDLServer(int fd,  int client_sock, const char* server_name,
                                   android::sp<IConnectControl>& service)
    : service_(service),
    hidl_fd_{fd},
    client_sock_{client_sock},
    server_name_{server_name} {}

SocketHIDLServer::~SocketHIDLServer() {
  server_name_ = NULL;
  if (hidl_fd_ >= 0) {
    ::close(hidl_fd_);
    hidl_fd_ = -1;
  }
  if (client_sock_>= 0) {
    ::close(client_sock_);
    client_sock_ = -1;
  }
}

void SocketHIDLServer::init() {
  int err;

  int sock = -1;
  while (1) {
    sock = socket_local_server(server_name_,ANDROID_SOCKET_NAMESPACE_ABSTRACT,
                               SOCK_STREAM);
    /* wait and try again */
    if (sock < 0) {
      err_log("warning: connect to %s failed, wait to try again ...\n", server_name_);
      sleep(1);
    } else {
      info_log("open server socket hidl_server fd=%d,%s  \n",sock,server_name_);
      long flags = fcntl(sock, F_GETFL);
      flags |= O_NONBLOCK;
      err = fcntl(sock, F_SETFL, flags);
      if (-1 == err) {
        ::close(sock);
        err_log("set server socket non-block error");
        continue;
      }
      hidl_fd_ = sock;
      break;
    }
  }
}

void SocketHIDLServer::process() {
  struct pollfd afd;
  struct pollfd cfd;

  int fd = get_hidl_fd();
  afd.fd = fd;
  afd.events = POLLIN;

  while (1) {
    //accept poll
    if (-1 == client_sock_) {
      int ret = poll(&afd, 1, -1);
      if (ret > 0) {
        if (POLLIN & afd.revents) {
          int sock_client = accept(afd.fd, 0, 0);
          if (sock_client >= 0) {
            info_log("accept a new client %d",sock_client);
            long flags = fcntl(sock_client, F_GETFL);
            flags |= O_NONBLOCK;
            int err = fcntl(sock_client, F_SETFL, flags);
            if (-1 == err) {
              err_log("set accepted socket to O_NONBLOCK error");
              ::close(sock_client);
              continue;
            }
            client_sock_ = sock_client;
            cfd.fd = client_sock_;
            cfd.events = POLLIN;
            int fd = socket_connect_process(client_sock_);
            info_log("get server fd_=%d",fd);
          }
        }
      }
    }

    //client poll
    if (client_sock_ >= 0) {
      char data[256] = {0};
      int ret = poll(&cfd, 1, -1);
      if (ret > 0) {
        if (POLLIN & cfd.revents) {
          info_log("data from slogmodem");
          int n = read(client_sock_,data,sizeof(data));
          if (n > 0) {
            socket_data_process(fd_,data,n);
          } else if (0 == n) {
            err_log("socket close");
            socket_close_process();
            ::close(client_sock_);
            client_sock_ = -1;
          } else if (-1 == n) {
            err_log("socket error");
            if (EAGAIN != errno && EINTR != errno) {
              socket_close_process();
              ::close(client_sock_);
              client_sock_ = -1;
            }
          }
        }
      }
    }
  }
}

int SocketHIDLServer::socket_connect_process( int sock) {

  if(NULL != server_name_) {

    Return<int> fd = service_->socketConnect(sock,server_name_ + strlen("hidl_"));
    fd_ = fd;
  } else {
    err_log("parameter error, no result return");
    return -1;
  }
  info_log("socket server fd_=%d",fd_);
  return fd_;
}

int SocketHIDLServer::socket_data_process( int sock,const char*data,int len) {

  if(NULL != data) {

    std::string pStr;
    auto cb = [&](hidl_string atResp) {
      pStr = atResp.c_str();
    };
    hidl_string str = hidl_string(data, len);

    Return<void> status = service_->socketData(sock,str, cb);
    if(!status.isOk()) {
      err_log("can not get response from server");
      return -1;
    } else {
      info_log("process socket response %s", pStr.c_str());
      return 0;
    }

  } else {
    err_log("parameter error, no result return");
    return -1;
  }

}

int SocketHIDLServer::socket_close_process() {
  int res;
  if(NULL != server_name_) {

    Return<int> result = service_->socketClose(server_name_ + strlen("hidl_"));
    res = result;
  } else {
    err_log("parameter error, no result return");
    return -1;
  }
  info_log("socket close result = %d",res);
  return 0;
}

