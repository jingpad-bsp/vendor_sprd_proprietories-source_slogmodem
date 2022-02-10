/*
 *  connectcontrolcallback.h - Common functions declarations.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2019-6-24 Gloria He
 *  Initial version.
 */
#ifndef VENDOR_SPRD_HARDWARE_LOG_V1_0_CONNECTCONTROLCALLBACK_H
#define VENDOR_SPRD_HARDWARE_LOG_V1_0_CONNECTCONTROLCALLBACK_H
#include "modem_state_hidl_server.h"
#include "modem_time_sync_hidl_server.h"
#include "wcn_state_hidl_server.h"

#include <vendor/sprd/hardware/cplog_connmgr/1.0/types.h>
#include <vendor/sprd/hardware/cplog_connmgr/1.0/IConnectControlCallback.h>
#include <vendor/sprd/hardware/cplog_connmgr/1.0/IConnectControl.h>

#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <unistd.h>

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;

using vendor::sprd::hardware::cplog_connmgr::V1_0::IConnectControl;
using vendor::sprd::hardware::cplog_connmgr::V1_0::IConnectControlCallback;
using ::android::sp;

namespace vendor {
namespace sprd {
namespace hardware {
namespace cplog_connmgr {
namespace V1_0 {
namespace implementation  {
class ConnectControlCallback : public IConnectControlCallback {
  // Methods from ILogCallback follow.

 public:
  ConnectControlCallback(bool has_notifier_client,const char *socket_name, sp<IConnectControl>& service);
  ~ConnectControlCallback();

  Return<void> onCmdresp(int sock, const hidl_string& cmd, int len,onCmdresp_cb _hidl_cb) override;
  Return<void> onTimeresp(int sock, const ::vendor::sprd::hardware::cplog_connmgr::V1_0::time_sync& time, int len) override;
  Return<void> onCmdreq(int sock, const hidl_string& cmd, int len, onCmdreq_cb _hidl_cb) override;
  Return<void> onSocketclose(int sock) override;

  int process_reqs(int sock, const char *cmd, int len, char *response, int size);

  void dump_notifier_process();
  bool has_notifier_client() const {return has_notifier_client_;}
  void is_notifier_client(const char* cmd, int fd, int client);

  void start(sp<IConnectControl>& service);


 private:
  int connect_socket_local_server();
  void process_close();
  static void* thread_check_dump(void *arg);

  struct notifier_sock_pair {
    int fd;
    int client;
  };

  bool has_notifier_client_;
  notifier_sock_pair dump_notifier_;

  const char* socket_name_;
  sp<IConnectControl>& service_;
  ModemStateHIDLServer* ms_hidl_server_;
  WcnStateHIDLServer* ws_hidl_server_;
  ModemTimesyncHIDLServer* mt_hidl_server_;

};

}  // namespace implementation
}  // namespace V1_0
}  // namespace slogmodemconnmgr
}  // namespace hardware
}  // namespace sprd
}  // namespace vendor

#endif  // VENDOR_SPRD_HARDWARE_LOG_V1_0_CONNECTCONTROLCALLBACK_H


