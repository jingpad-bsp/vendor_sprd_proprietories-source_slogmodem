/*
 *  service.cpp - socket communication service in system side.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2019-6-24 Gloria He
 *  Initial version.
 */
#include "connect_cmn.h"
#include "ConnectControlCallback.h"
#include <signal.h>
#include <log/log.h>
#include <hidl/LegacySupport.h>
#include <hidl/HidlTransportSupport.h>
#include <hidl/HidlSupport.h>
#include <vendor/sprd/hardware/cplog_connmgr/1.0/IConnectControl.h>

using vendor::sprd::hardware::cplog_connmgr::V1_0::IConnectControlCallback;
using vendor::sprd::hardware::cplog_connmgr::V1_0::IConnectControl;
using vendor::sprd::hardware::cplog_connmgr::V1_0::implementation::ConnectControlCallback;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using ::android::hardware::hidl_death_recipient;
using android::sp;
using android::status_t;
using android::OK;
using android::wp;

//struct hidl_death_recipient;
struct MyDeathRecipient : hidl_death_recipient {
  virtual void serviceDied(uint64_t cookie, const wp<::android::hidl::base::V1_0::IBase>& who) {
    // Deal with the fact that the service died
    err_log("IConnectControl service died ,exit");
    exit(0);
  }
};

int main() {
  // parse arg first
  const char *socketName = "hidl_slogmodem";
  sp<IConnectControl> service = NULL;
  sp<MyDeathRecipient>  deathRecipient = new MyDeathRecipient();

  // Ignore SIGPIPE to avoid to be killed by the kernel
  // when writing to a socket which is closed by the peer.
  struct sigaction siga;

  memset(&siga, 0, sizeof siga);
  siga.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &siga, 0);

  while(true) {
    service = IConnectControl::getService();
    if (service != NULL) {
      service->linkToDeath(deathRecipient, 1);
      ConnectControlCallback* callback = new ConnectControlCallback(false,
          socketName,service);
      callback->start(service);
      service->setCallback((sp<IConnectControlCallback>)callback);

      break;
    } else {
      err_log("IConnectControl getService failed");
      sleep(1);
      continue;
    }
  }
  while (true) {
    sleep(1000 * 1000);
  }
  return 0;
}
