/*
 *  uevent_monitor.h - kernel will send a netlink socket to notify
 *                     the external peripheral status.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-11-23 Yan Zhihang
 *  Initial version.
 */

#include <climits>
#include <fcntl.h>
#include <linux/netlink.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "cp_log_cmn.h"
#include "uevent_monitor.h"
#include "parse_utils.h"
#include "stor_mgr.h"

UeventMonitor::UeventMonitor(LogController* ctrl, Multiplexer* multi)
    : FdHandler(-1, ctrl, multi) {}

UeventMonitor::~UeventMonitor() {
  clear_ptr_container(m_event_clients);
}

int UeventMonitor::init() {
  struct sockaddr_nl nls;

  memset(&nls, 0, sizeof(struct sockaddr_nl));
  nls.nl_family = AF_NETLINK;
  nls.nl_pid = getpid();
  nls.nl_groups = -1;
  m_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);

  if (m_fd < 0) {
    err_log("Unable to create uevent socket.");
    return -1;
  }

  long flags = fcntl(m_fd, F_GETFL);
  flags |= O_NONBLOCK;
  int err = fcntl(m_fd, F_SETFL, flags);
  if (-1 == err) {
    err_log("set netlink socket to O_NONBLOCK error");
    ::close(m_fd);
    m_fd = -1;
    return -1;
  }

  int ret = 0;
  if (bind(m_fd, (const sockaddr*)&nls, sizeof(struct sockaddr_nl))) {
    err_log("netlink bind err");
    ::close(m_fd);
    m_fd = -1;
    ret = -1;
  } else {
    multiplexer()->register_fd(this, POLLIN);
  }

  return 0;
}

void UeventMonitor::subscribe_event_notifier(void* client_ptr,
    SubSystemType type, uevent_callback_t cb) {

  EventClient* c = new EventClient;
  c->client_ptr = client_ptr;
  c->type = type;
  c->cb = cb;
  m_event_clients.push_back(c);
}

void UeventMonitor::unsubscribe_event_notifier(void* client_ptr) {
  for (auto it = m_event_clients.begin(); it != m_event_clients.end(); ++it) {
    EventClient* client = *it;
    if (client_ptr == client->client_ptr) {
      m_event_clients.erase(it);
      delete client;
      break;
    }
  }
}

void UeventMonitor::notify_event(Event evt, SubSystemType type,
                                 const ActionInfo& info) {
  for (auto it = m_event_clients.begin(); it != m_event_clients.end(); ++it) {
    EventClient* client = *it;

    if (client->type == type) {
      client->cb(client->client_ptr, evt, info);
    }
  }
}

void UeventMonitor::process(int /*events*/) {
  uint8_t buf[2048];
  uint8_t* pbuf = buf;
  uint8_t* pnext = 0;
  struct sockaddr_nl sa;
  struct iovec iov = { buf, sizeof(buf) - 1 };
  struct msghdr msg = { &sa, sizeof sa, &iov, 1, NULL, 0, 0 };

  unsigned evt_major_num = UINT_MAX;
  unsigned evt_minor_num = UINT_MAX;
  UsbState evt_usbstate = USB_NONE;

  SubSystemType subsystem = SST_NONE;
  Event evt_action = NONE;

  ssize_t len = recvmsg(m_fd, &msg, 0);

  if (-1 == len) {
    return;
  }

  size_t remain_len = len;
  uint8_t* end_ptr = buf + len;
  // netlink message format:
  //   (add/remove/change)@(device node path)\0... ...
  //   ACTION=(add/remove)\0
  //   MAJOR=xxx\0
  //   MINOR=xxx\0... ...
  //   SUBSYSTEM=block\0
  //   ... ...\0\0
  while (remain_len) {
    if ((remain_len >= 7) &&
        (!memcmp(pbuf, "ACTION=", 7))) {
      remain_len -= 7;
      pbuf += 7;
      const uint8_t* tok;
      size_t tok_len;
      tok = get_token(pbuf, remain_len, tok_len);
      if (tok) {
        if ((3 == tok_len) && !memcmp(tok, "add", 3)) {
          evt_action = ADD;
        } else if (6 == tok_len) {
          if (!memcmp(tok, "remove", 6)) {
            evt_action = REMOVE;
          } else if(!memcmp(tok, "change", 6)) {
            evt_action = CHANGE;
          }
        }
        if (NONE == evt_action) {
          // Action is not one of add, remove and change
          break;
        }

        pbuf = const_cast<uint8_t*>(tok) + tok_len;
        remain_len = end_ptr - pbuf;
      } else {
        // No token after ACTION=
        break;
      }
    } else if (((evt_action == ADD) || (evt_action == REMOVE)) &&
               (remain_len >= 6) &&
               (!strncmp(reinterpret_cast<char*>(pbuf), "MAJOR=", 6))) {
      remain_len -= 6;
      pbuf += 6;
      pnext = static_cast<uint8_t*>(memchr(pbuf, '\0', remain_len));
      if ((pnext <= pbuf) ||
          (parse_number(pbuf, static_cast<size_t>(pnext - pbuf),
                        evt_major_num))) {
        break;
      }
    } else if (((evt_action == ADD) || (evt_action == REMOVE)) &&
               (remain_len >= 6) &&
               (!strncmp(reinterpret_cast<char*>(pbuf), "MINOR=", 6))) {
      remain_len -= 6;
      pbuf += 6;
      pnext = static_cast<uint8_t*>(memchr(pbuf, '\0', remain_len));
      if ((pnext <= pbuf) ||
          (parse_number(reinterpret_cast<uint8_t*>(pbuf),
                        static_cast<size_t>(pnext - pbuf), evt_minor_num))) {
        break;
      }
    } else if ((remain_len >= 10) &&
               (!strncmp(reinterpret_cast<char*>(pbuf), "SUBSYSTEM=", 10))) {
      remain_len -= 10;
      pbuf += 10;
      const uint8_t* tok;
      size_t tok_len;
      tok = get_token(pbuf, remain_len, tok_len);
      if (tok) {
        if ((5 == tok_len) && !memcmp(tok, "block", 5)) {
          // SUBSYSTEM=block (10+5)
          subsystem = SST_BLOCK;
        } else if ((11 == tok_len) && (!memcmp(tok, "android_usb", 11))) {
          // SUBSYSTEM=android_usb (10+11)
          subsystem = SST_USB;
        } else {
          // SUBSYSTEM is not block, android_usb
          break;
        }
        pbuf = const_cast<uint8_t*>(tok) + tok_len;
        remain_len = end_ptr - pbuf;
      } else {
        // No token after SUBSYSTEM=
        break;
      }
    } else if ((evt_action == CHANGE) && (remain_len >= 10) &&
               (!strncmp(reinterpret_cast<char*>(pbuf), "USB_STATE=", 10))) {
      remain_len -= 10;
      pbuf += 10;
      const uint8_t* tok;
      size_t tok_len;
      tok = get_token(pbuf, remain_len, tok_len);
      if (tok) {
        if ((9 == tok_len) && !memcmp(tok, "CONNECTED", 9)) {
          // USB_STATE=CONNECTED (10+9)
          evt_usbstate = CONNECTED;
        } else if ((10 == tok_len) && (!memcmp(tok, "CONFIGURED", 10))) {
          // USB_STATE=CONFIGURED (10+10)
          evt_usbstate = CONFIGURED;
        } else if ((12 == tok_len) && (!memcmp(tok, "DISCONNECTED", 12))) {
          // USB_STATE=DISCONNECTED
          evt_usbstate = DISCONNECTED;
        } else {
          // USB_STATE is not CONNECTED/CONFIGURED/DISCONNECTED
          break;
        }
        pbuf = const_cast<uint8_t*>(tok) + tok_len;
        remain_len = end_ptr - pbuf;
      } else {
        // No token after USB_STATE=
        break;
      }
    }

    pnext = static_cast<uint8_t*>(memchr(pbuf, '\0', remain_len));

    if (pnext) {
      pbuf = pnext + 1;
      remain_len = end_ptr - pbuf;
    } else {
      // No '\0' in remaining data
      break;
    }
  }

  bool valid_evt = true;
  ActionInfo info;
  if ((SST_BLOCK == subsystem) &&
      ((ADD == evt_action) || (REMOVE == evt_action)) &&
      (UINT_MAX != evt_major_num) && (UINT_MAX != evt_minor_num)) {
    info_log("Block device:%u,%u %s event message.",
             evt_major_num, evt_minor_num,
             (evt_action == REMOVE) ? "remove" : "add");
    info.dn.major_num = evt_major_num;
    info.dn.minor_num = evt_minor_num;
  } else if (SST_USB == subsystem && CHANGE == evt_action &&
             CONFIGURED == evt_usbstate) {
    info_log("usb state is configured");
    info.us = CONFIGURED;
  } else {
    valid_evt = false;
  }

  if (valid_evt) {
    notify_event(evt_action, subsystem, info);
  }
}
