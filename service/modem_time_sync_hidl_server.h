/*
 *  modem_time_sync_hidl_server.h - The modem time sync socket server base class declaration.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2019-06-24 Gloria.He
 *  Initial version.
 */

#ifndef _MODEM_TIME_SYNC_HIDL_SERVER_H__
#define _MODEM_TIME_SYNC_HIDL_SERVER_H__
#include "hidl_server.h"
#include <ctype.h>
#include <hidl/LegacySupport.h>
#include <hidl/HidlTransportSupport.h>
#include <android/log.h>
#include <vendor/sprd/hardware/cplog_connmgr/1.0/IConnectControl.h>

using android::sp;
using vendor::sprd::hardware::cplog_connmgr::V1_0::IConnectControl;

class ModemTimesyncHIDLServer : public SocketHIDLServer {
  public:
    ModemTimesyncHIDLServer(sp<IConnectControl>& service,
                         const char* server_name);
    void start();

  protected:
    static const int MODEM_TIME_BUF_SIZE = 64;

  private:
    static void *thread_modem_time_check(void *arg);
};

#endif
