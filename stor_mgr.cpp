/*
 *  stor_mgr.cpp - storage manager.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-5-14 Zhang Ziyi
 *  Initial version.
 */

#include <algorithm>
#include <climits>
#include <cstdlib>
#ifdef HOST_TEST_
#include "prop_test.h"
#else
#include <cutils/properties.h>
#endif
#include <unistd.h>

#include "cp_log_cmn.h"
#include "cp_set_dir.h"
#include "cp_stor.h"
#include "file_watcher.h"
#include "log_pipe_hdl.h"
#include "parse_utils.h"
#include "stor_mgr.h"
#include "uevent_monitor.h"
#include "media_stor_check.h"

StorageManager::StorageManager()
    : m_current_storage{},
      m_media_storage{this, this},
      m_file_watcher{},
      m_use_ext_stor_fuse{true},
      m_uevent_monitor{} {}

StorageManager::~StorageManager() {
  // delete m_file_watcher;
  clear_ptr_container(m_event_clients);
  clear_ptr_container(m_umt_event_clients);
  clear_ptr_container(m_stor_check);
  // m_stor_check may depend on m_uevent_monitor, so destroy
  // m_uevent_monitor later.
  delete m_uevent_monitor;
}

int StorageManager::init(const LogString& internal_stor_pos,
                         const LogString& external_stor_pos,
                         bool int_stor_active,
                         bool ext_stor_active,
                         LogController* ctrl,
                         Multiplexer* multiplexer) {
  // initialisation of the uevnt monitor
  m_uevent_monitor = new UeventMonitor(ctrl, multiplexer);
  if (m_uevent_monitor->init()) {
    delete m_uevent_monitor;
    m_uevent_monitor = nullptr;
    err_log("UeventMonitor init failed");
  }

  m_stor_check.push_back(new MediaStorCheck{this,
                                            &m_media_storage[MT_INT_STOR],
                                            MT_INT_STOR,
                                            MPRIOR_0,
                                            int_stor_active,
                                            LogString(DATA_PATH)});

  // if emulated internal MTP storage path is specified,
  // internal MTP storage have higher priority than /data path
  if (!str_empty(internal_stor_pos)) {
    m_stor_check.push_back(new MediaStorCheck{this,
                                              &m_media_storage[MT_INT_STOR],
                                              MT_INT_STOR,
                                              MPRIOR_1,
                                              int_stor_active,
                                              internal_stor_pos});
  }

  // the external storage which is extrnal MTP storage
  // external MTP storage have the highest priority for selection
  m_stor_check.push_back(new MediaStorCheck{this,
                                            &m_media_storage[MT_EXT_STOR],
                                            MT_EXT_STOR,
                                            MPRIOR_0,
                                            ext_stor_active,
                                            external_stor_pos});

  return 0;
}

MediaStorage* StorageManager::get_media_stor(MediaType t) {
  return &m_media_storage[t];
}

MediaStorage* StorageManager::get_media_stor() {
  MediaStorage* ms{};

  if (m_current_storage) {
    ms = m_current_storage->ms();
  }

  return ms;
}

CpStorage* StorageManager::create_storage(LogPipeHandler& cp_log,
                                          size_t max_buf,
                                          size_t max_num,
                                          size_t cmt_size) {
  CpStorage* cp_stor = new CpStorage(*this, cp_log);

  cp_stor->set_log_buffer_size(max_buf, max_num);
  cp_stor->set_log_commit_threshold(cmt_size);
  if (!cp_stor->init()) {
    m_cp_handles.push_back(cp_stor);
  } else {
    delete cp_stor;
    cp_stor = nullptr;
  }
  return cp_stor;
}

void StorageManager::delete_storage(CpStorage* stor) {
  LogList<CpStorage*>::iterator it;

  for (it = m_cp_handles.begin(); it != m_cp_handles.end(); ++it) {
    CpStorage* cp = *it;
    if (stor == cp) {
      m_cp_handles.erase(it);
      delete stor;
      break;
    }
  }
}

void StorageManager::proc_working_dir_removed(MediaStorage* ms) {
  if (m_current_storage->ms() == ms) {
    stop_all_cps();
  }
}

void StorageManager::stop_all_cps() {
  for (auto hdlr: m_cp_handles) {
    hdlr->stop();
  }
}

void StorageManager::subscribe_media_change_evt(void* client_ptr,
                                                stor_event_callback_t cb) {
  EventClient* c = new EventClient;
  c->client_ptr = client_ptr;
  c->cb = cb;
  m_event_clients.push_back(c);
}

void StorageManager::unsubscribe_media_change_evt(void* client_ptr) {
  for (auto it = m_event_clients.begin(); it != m_event_clients.end(); ++it) {
    EventClient* client = *it;
    if (client_ptr == client->client_ptr) {
      m_event_clients.erase(it);
      delete client;
      break;
    }
  }
}

void StorageManager::subscribe_stor_inactive_evt(void* client_ptr,
                                                 stor_umount_callback_t cb) {
  if (m_umt_event_clients.end() ==
          std::find_if(m_umt_event_clients.begin(),
                       m_umt_event_clients.end(),
                       [&](const EventClient* elem) {
                         return elem->client_ptr == client_ptr &&
                                elem->cb_umnt == cb;
                       })) {
    EventClient* c = new EventClient;
    c->client_ptr = client_ptr;
    c->cb_umnt = cb;
    m_umt_event_clients.push_back(c);
  }
}

void StorageManager::unsubscribe_stor_inactive_evt(void* client_ptr) {
  auto it = m_umt_event_clients.begin();
  while (it != m_umt_event_clients.end()) {
    EventClient* client = *it;
    if (client_ptr == client->client_ptr) {
      it = m_umt_event_clients.erase(it);
      delete client;
    } else {
      ++it;
    }
  }
}

void StorageManager::notify_stor_inactive(unsigned priority) {
  for (auto client: m_umt_event_clients) {
    client->cb_umnt(client->client_ptr, priority);
  }
}

void StorageManager::notify_media_change() {
  for (auto client: m_event_clients) {
    client->cb(client->client_ptr);
  }
}

bool StorageManager::media_determiner(MediaStorCheck* msc,
                                      StorageState se) {
  bool ret{};

  switch (se) {
    case STOR_READY:
      if (!msc->permit()) {
        if (m_current_storage == msc) {
          m_current_storage = nullptr;
          msc->set_use(false);
        }
      } else {
        if (m_current_storage == msc) {
          ret = true;
        } else {
          unsigned priority = (static_cast<unsigned>(msc->mt()) << 8) +
                               static_cast<unsigned>(msc->mp());
          LogString log_path = msc->path() + "/" + PARENT_LOG;
          if (msc->log_synchronized() ||
              0 == sync_media_storage(msc->ms(), log_path, priority)) {
            msc->ms()->media_init(log_path, priority);

            if (nullptr != m_current_storage) {
              // reset the storage use status
              m_current_storage->set_use(false);
              // stop the log saving
              stop_all_cps();
            }

            // change the new current storage
            msc->set_use(true);
            msc->set_sync(true);

            m_current_storage = msc;

            ret = true;
            info_log("switch to storage %s", ls2cstring(msc->path()));
          } else {
            err_log("Fail to switch to %s", ls2cstring(msc->path()));
          }
        }
      }
      break;
    case VANISHED:
      info_log("storage %s vanished", ls2cstring(msc->path()));

      stop_all_cps();
      msc->set_use(false);
      msc->set_sync(false);
      notify_stor_inactive((static_cast<unsigned>(msc->mt()) << 8) +
                           static_cast<unsigned>(msc->mp()));

      msc->ms()->stor_media_vanished((static_cast<unsigned>(msc->mt()) << 8) +
                                     static_cast<unsigned>(msc->mp()));
      msc->set_path_vanished();

      m_current_storage = nullptr;

      break;
    default:
      // if the media has been use before, but not available now, clear
      // the log directory in media storage
      if (msc->log_synchronized()) {
        msc->set_sync(false);
        notify_stor_inactive((static_cast<unsigned>(msc->mt()) << 8) +
                             static_cast<unsigned>(msc->mp()));
        msc->ms()->stor_media_vanished((static_cast<unsigned>(msc->mt()) << 8) +
                                       static_cast<unsigned>(msc->mp()));
        msc->set_path_vanished();
     }
      break;
  }

  return ret;
}

int StorageManager::enable_media(MediaType mt, bool en) {
  for (auto msc : m_stor_check) {
    if (mt != msc->mt()) {
      continue;
    }

    if (en != msc->permit()) {
      msc->set_permission(en);
      if (!en) {
        notify_stor_inactive((static_cast<unsigned>(mt) << 8) +
                             static_cast<unsigned>(msc->mp()));
        // If this is the current media, inform all CpStorage objects
        // and stop using it.
        if (msc == m_current_storage) {
          for (auto cstor: m_cp_handles) {
            cstor->on_cur_media_disabled();
          }
          msc->set_use(false);
          m_current_storage = nullptr;
        }
      }
    }
  }

  return 0;
}

int StorageManager::sync_media_storage(MediaStorage* ms, LogString& path,
                                       unsigned priority) {
  return ms->sync_media(path, priority, m_file_watcher);
}

void StorageManager::sync_cp_directory(CpType type,
                                       LogVector<CpDirectory*>& logs) {
  for (auto msc : m_stor_check) {
    unsigned priority = (static_cast<unsigned>(msc->mt()) << 8) +
        static_cast<unsigned>(msc->mp());

    if (StorageManager::STOR_READY == msc->get_state()) {
      if (!msc->log_synchronized()) {
        LogString log_path = msc->path() + "/" + PARENT_LOG;
        if (0 == sync_media_storage(msc->ms(), log_path, priority)) {
          msc->set_sync(true);
        }
      }

      if (msc->log_synchronized()) {
        for (auto cpset : msc->ms()->all_cp_sets()) {
          if (priority != cpset->priority()) {
            continue;
          }

          auto cpdir = cpset->get_cp_dir(type);
          if (cpdir) {
            logs.push_back(cpdir);
          }
        }
      }
    }
  }
}

void StorageManager::sync_media_storages() {
  for (auto it = m_stor_check.begin(); it != m_stor_check.end(); ++it) {
    MediaStorCheck* msc = *it;

    unsigned priority = (static_cast<unsigned>(msc->mt()) << 8) +
        static_cast<unsigned>(msc->mp());

    if (STOR_READY == msc->get_state()) {
      if (!msc->log_synchronized()) {
        LogString log_path = msc->path() + "/" + PARENT_LOG;
        if (sync_media_storage(msc->ms(), log_path, priority)) {
          info_log("storage %s synchronisation fail", ls2cstring(msc->path()));
        } else {
          msc->set_sync(true);
        }
      }
    } else {
      // if the media has been use before, but not available now, clear
      // the log directory in media storage
      if (msc->log_synchronized()) {
        msc->ms()->stor_media_vanished(priority);
        msc->set_sync(false);
      }
    }
  }
}

bool StorageManager::check_media_change() {
  bool changed{};
  MediaStorCheck* spi_pre = m_current_storage;

  // Check from the highest priority.
  for (auto it = m_stor_check.rbegin();
       it != m_stor_check.rend(); ++it) {
    MediaStorCheck* msc = *it;
    if (media_determiner(msc, msc->get_state())) {
     break;
    }
  }

  if (m_current_storage != spi_pre) {
    notify_media_change();
    changed = true;
  }

  return changed;
}

void StorageManager::clear() {
  if (m_current_storage) {
    stop_all_cps();
    info_log("stop_all_cps()");
    m_current_storage->set_use(false);
    m_current_storage = nullptr;
  }

  sync_media_storages();

  for (int mt = MT_INT_STOR; mt < MT_STOR_END; ++mt) {
    m_media_storage[static_cast<MediaType>(mt)].clear();
    info_log("clear %d",mt);
  }
}
