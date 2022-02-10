/*
 *  evt_notifier.h - The event notifier class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-5-15 Zhang Ziyi
 *  Initial version.
 */

#ifndef EVT_NOTIFIER_H_
#define EVT_NOTIFIER_H_

#include "cp_log_cmn.h"

class EventNotifier {
 public:
  using CallbackFun = void (*)(void*);

  EventNotifier():is_notifying_{false} {}
  EventNotifier(const EventNotifier&) = delete;
  ~EventNotifier() { clear_ptr_container(evt_subs_); }

  EventNotifier& operator = (const EventNotifier&) = delete;

  /*  add_client - add a client to the subscription.
   *  @client: the client handle.
   *  @cb: the callback function pointer.
   *
   *  If the client is new, return 0; otherwise the client won't be added
   *  and add_client returns -1.
   */
  int add_client(void* client, CallbackFun cb);

  /*  remove_client - remove a client from the subscription.
   *  @client: the client handle.
   *  @cb: the callback function pointer.
   *
   */
  void remove_client(void* client, CallbackFun cb);

  /*  remove_client - remove all clients with the specified handle.
   *  @client: the client handle.
   *
   */
  void remove_client(void* client);

  /*  notify - notify all clients.
   */
  void notify();

 private:
  struct ClientEntry {
    void* client;
    CallbackFun cb;
    bool removed;

    ClientEntry(void* cli, CallbackFun cb_fun)
        :client{cli},
         cb{cb_fun},
         removed{false} {}
    ClientEntry(const ClientEntry&) = delete;

    ClientEntry& operator = (const ClientEntry&) = delete;
  };

  LogList<ClientEntry*> evt_subs_;
  bool is_notifying_;
};

#endif  // !EVT_NOTIFIER_H_
