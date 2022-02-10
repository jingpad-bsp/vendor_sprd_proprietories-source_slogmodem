/*
 *  evt_notifier.cpp - The event notifier class.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-5-15 Zhang Ziyi
 *  Initial version.
 */

#include "evt_notifier.h"

int EventNotifier::add_client(void* client, CallbackFun cb) {
  if (!cb) {
    return -1;
  }

  auto it = evt_subs_.begin();

  for (; it != evt_subs_.end(); ++it) {
    ClientEntry* e = *it;
    if (e->client == client && e->cb == cb) {
      break;
    }
  }

  int ret{-1};

  if (it == evt_subs_.end()) {
    evt_subs_.push_back(new ClientEntry{client, cb});
    ret = 0;
  }

  return ret;
}

void EventNotifier::remove_client(void* client, CallbackFun cb) {
  auto it = evt_subs_.begin();

  for (; it != evt_subs_.end(); ++it) {
    ClientEntry* e = *it;
    if (e->client == client && e->cb == cb) {
      if (is_notifying_) {
        e->removed = true;
      } else {
        evt_subs_.erase(it);
        delete e;
      }
      break;
    }
  }
}

void EventNotifier::remove_client(void* client) {
  auto it = evt_subs_.begin();

  while (it != evt_subs_.end()) {
    ClientEntry* e = *it;
    if (e->client == client) {
      if (is_notifying_) {
        e->removed = true;
        ++it;
      } else {
        it = evt_subs_.erase(it);
        delete e;
      }
    } else {
      ++it;
    }
  }
}

void EventNotifier::notify() {
  is_notifying_ = true;

  // Only process current clients.
  size_t i = 0;
  size_t total = evt_subs_.size();
  auto it = evt_subs_.begin();
  while (i < total) {
    ClientEntry* e = *it;
    if (!e->removed) {
      e->cb(e->client);
    }
    ++i;
    ++it;
  }

  is_notifying_ = false;

  it = evt_subs_.begin();
  while (it != evt_subs_.end()) {
    ClientEntry* e = *it;
    if (e->removed) {
      it = evt_subs_.erase(it);
      delete e;
    } else {
      ++it;
    }
  }
}
