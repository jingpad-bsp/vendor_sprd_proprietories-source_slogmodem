/*
 *  wcn_state_hidl_server.h - The wcn state socket server base class declaration.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2019-06-24 Gloria.He
 *  Initial version.
 */

#ifndef _WCN_STATE_HIDL_SERVER_H__
#define _WCN_STATE_HIDL_SERVER_H__
#include "hidl_server.h"
#include <ctype.h>
#include <hidl/LegacySupport.h>
#include <hidl/HidlTransportSupport.h>
#include <android/log.h>
#include <vendor/sprd/hardware/cplog_connmgr/1.0/IConnectControl.h>

using android::sp;
using vendor::sprd::hardware::cplog_connmgr::V1_0::IConnectControl;

class WcnStateHIDLServer : public SocketHIDLServer {
  public:
    WcnStateHIDLServer(sp<IConnectControl>& service,
                         const char* server_name);
    void start();

  protected:
    static const int WCN_STATE_BUF_SIZE = 256;

  private:
    static void *thread_wcn_state_check (void *arg);
};

#endif
