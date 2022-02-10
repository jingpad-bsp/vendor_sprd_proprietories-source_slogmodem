/*
 *  hidl_server.h - The hidl socket server base class declaration.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2019-06-24 Gloria.He
 *  Initial version.
 */
#ifndef _HIDL_SERVER_H__
#define _HIDL_SERVER_H__
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <cutils/sockets.h>
#include <hidl/LegacySupport.h>
#include <hidl/HidlTransportSupport.h>
#include <android/log.h>
#include <vendor/sprd/hardware/cplog_connmgr/1.0/IConnectControl.h>

using vendor::sprd::hardware::cplog_connmgr::V1_0::IConnectControl;

class SocketHIDLServer {
  android::sp<IConnectControl>& service_;

  public:
    SocketHIDLServer(int fd, int client_sock, const char* server_name,
                     android::sp<IConnectControl>& service);

    SocketHIDLServer(const SocketHIDLServer&) = delete;

    virtual ~SocketHIDLServer();
    SocketHIDLServer& operator = (const SocketHIDLServer&) = delete;

    int get_hidl_fd() const { return hidl_fd_; }
    int socket_connect_process(int sock);
    int socket_data_process( int sock,const char*data, int len);
    int socket_close_process();
    void process();

  protected:
    int fd_;
    int hidl_fd_;
    int client_sock_;

    void init();

  private:
    const char* server_name_;
};

#endif
