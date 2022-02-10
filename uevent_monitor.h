/*
 *  uevent_monitor.h - kernel will send a netlink socket to notify
 *                     the external peripheral status.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2016-03-21 Yan Zhihang
 *  Initial version.
 *  2017-02-17 Yan Zhihang
 *  Accommodate to universal kernel uevent.
 *  Add support for usb event detection
 */
#ifndef UEVENT_MONITOR_H_
#define UEVENT_MONITOR_H_

#include "multiplexer.h"

class LogController;
class Multiplexer;
class StorageManager;

class UeventMonitor : public FdHandler {
 public:
  enum Event { NONE, ADD, REMOVE, CHANGE };
  enum SubSystemType { SST_NONE, SST_BLOCK, SST_USB };
  enum UsbState { USB_NONE, DISCONNECTED, CONNECTED, CONFIGURED };

  struct DeviceNum {
    unsigned major_num;
    unsigned minor_num;
  };

  union ActionInfo {
    DeviceNum dn;
    UsbState us;
  };

  typedef void (*uevent_callback_t)(void* client_ptr,
                                    Event evt,
                                    const ActionInfo& info);

  UeventMonitor(LogController* ctrl, Multiplexer* multi);
  ~UeventMonitor();

  int init();
  void process(int events) override;

  /* subscribe_event_notifier - Subscribe uevent.
   *
   * @client_ptr - client: storage manager.
   * @type - subsystem type
   * @cb - client callback of this event notification.
   *       cb should not be null.
   */
  void subscribe_event_notifier(void* client_ptr,
				SubSystemType type,
                                uevent_callback_t cb);
  void unsubscribe_event_notifier(void* client_ptr);

 private:
  struct EventClient {
    void* client_ptr;
    SubSystemType type;
    uevent_callback_t cb;
  };

  LogList<EventClient*> m_event_clients;

  /* notify_eventt - notification of event.
   * @ evt - the storage event
   * @ type     - subsystem type (usb/sdcard)
   * @ info     - information og uevent
   */
  void notify_event(Event evt, SubSystemType type,
                    const ActionInfo& info);
};

#endif  //!UEVENT_MONITOR_H_
