/*
 *  stor_mgr.h - storage manager.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-5-14 Zhang Ziyi
 *  Initial version.
 */
#ifndef _STOR_MGR_H_
#define _STOR_MGR_H_

#include "uevent_monitor.h"
#include "media_stor.h"

class CpStorage;
class FileWatcher;
class LogController;
class LogPipeHandler;
class MediaStorCheck;
class Multiplexer;

class StorageManager {
 public:
  enum MediaType {
    MT_INT_STOR,   // the internal storage media
    MT_EXT_STOR,   // the external storage media
    MT_STOR_END    // count of different media storage
  };

  enum MediaPrior {
    MPRIOR_0,   // lower priority
    MPRIOR_1    // higher priority
  };

  enum StorageState {
    NOT_READY,   // not available from the very beginning
    STOR_READY,  // available for use
    VANISHED     // in use or ever used but not available now
  };

  using stor_event_callback_t = void (*)(void* client_ptr);
  using stor_umount_callback_t = void (*)(void* client_ptr,
                                          unsigned priority);

  StorageManager();
  StorageManager(const StorageManager&) = delete;
  ~StorageManager();

  StorageManager& operator = (const StorageManager&) = delete;

  /*  init - initialize the storage manager.
   *  @internal_stor_pos: top directory of preferred internal storage
   *  @extern_stor_pos: top directory of SD card.
   *  @int_stor_active : activation of internal storage
   *  @ext_stor_active : activation of external storage
   *  @ctrl: LogController object
   *  @multiplexer: Multiplexer object
   *
   *  Return 0 on success, -1 on failure.
   */
  int init(const LogString& internal_stor_pos,
           const LogString& extern_stor_pos,
           bool int_stor_active,
           bool ext_stor_active,
           LogController* ctrl,
           Multiplexer* multiplexer);

  UeventMonitor* uevt_monitor() const { return m_uevent_monitor; }
  bool use_fuse_stor() const { return m_use_ext_stor_fuse; }

  /*  enable_media - enable/disable the specified media administratively.
   *  @mt: Media Type
   *  @en: true - enable the media, false - disable the media.
   *
   *  Return Value:
   *    0 on success, -1 otherwise.
   */
  int enable_media(MediaType mt, bool en = true);

  /* stop_all_cps - stop all thr cp logs' saving
   */
  void stop_all_cps();
  /*  create_storage - create CP storage handle.
   *  @cp_log: the LogPipeHandler object reference.
   *  @max_buf: maximum size of one buffer
   *  @max_num: maximum number of buffers
   *  @cmt_size: threshold of data commit size
   *
   *  Return the CpStorage pointer on success, NULL on failure.
   */
  CpStorage* create_storage(LogPipeHandler& cp_log,
                            size_t max_buf,
                            size_t max_num,
                            size_t cmt_size);
  void delete_storage(CpStorage* stor);

  /* get_media_stor - get media storage (internal or external) according
   *                  to media type.
   * @MediaType: MT_INTERNAL or MT_SD_CARD
   * Return the media storage
   */
  MediaStorage* get_media_stor(MediaType t);

  MediaStorage* get_media_stor();
  /*
   * sync_cp_directory - sync all the media storage of cp's log files
   *
   * @type: cp type
   * @logs: log directories found
   *
   */
  void sync_cp_directory(CpType type, LogVector<CpDirectory*>& logs);
  /*  sync_media_storage - set the path and priority of a media storage
   *                       to synchronise the media storage
   *  @ms: media storage pointer
   *  @path: path of the storage
   *  @priority: priority of the storage path
   *
   *  Return
   *    0 the media sync success, -1 otherwise.
   */
  int sync_media_storage(MediaStorage* ms, LogString& path, unsigned priority);
  /*  check_media_change - check if the media change
   *
   *  Return true if the media changed, false otherwise.
   */
  bool check_media_change();

  /*  clear - delete all logs.
   */
  void clear();

  /*  proc_working_dir_removed - process the removal of working directory.
   */
  void proc_working_dir_removed(MediaStorage* ms);

  /*  subscribe_media_change_evt - subscribe the service of media change
   *                               from internal storage to external storage.
   *
   *  @client_ptr - log pipe handler client(wan, wcn or gnss)
   *  @cb - callback how the client handle the event of media change
   */
  void subscribe_media_change_evt(void* client_ptr, stor_event_callback_t cb);

  /*  unsubscribe_media_change_evt - unsubscribe the service of media change
   *                                 from internal storage to external storage.
   *
   *  @client_ptr - log pipe handler client(wan, wcn or gnss)
   */
  void unsubscribe_media_change_evt(void* client_ptr);

  /*  subscribe_stor_inactive_evt - subscribe the service of storage
   *                                inactivation event.
   *
   *  @client_ptr - non log file clients:
   *                sleep log/ringbuffer/minidump/memory dump
   *  @cb - callback how the client handle the event of umount
   */
  void subscribe_stor_inactive_evt(void* client_ptr,
                                   stor_umount_callback_t cb);

  /*  unsubscribe_stor_inactivet_evt - unsubscribe the service of storage
   *                                   inactivation event.
   *
   *  @client_ptr - non log file clients:
   *                sleep log/ringbuffer/minidump/memory dump
   */
  void unsubscribe_stor_inactive_evt(void* client_ptr);

  /*  notify_media_change - storage manager notify the subscribed handler
   *                        client when storage media changes.
   */
  void notify_media_change();

  MediaStorCheck* current_storage() {
    return m_current_storage;
  }

 private:
  /*  media_determiner - check the availability of the specified media.
   *  @msc: the media to be checked. msc can not be the same as
   *        m_current_storage.
   *  @se: the state of the media.
   *
   *  If the media is ready, and is being used when the function returns,
   *  return true; otherwise return false.
   */
  bool media_determiner(MediaStorCheck* msc, StorageState se);
  /*
   * sync_media_storages - synchronisation of the all medias
   *
   */
  void sync_media_storages();

  /* notify_stor_inactive - storage manager notify the subscribed handler
   *                        client when storage can not be used.
   */
  void notify_stor_inactive(unsigned priority);

 private:
  struct EventClient {
    void* client_ptr;
    union {
      stor_event_callback_t cb;
      stor_umount_callback_t cb_umnt;
    };
  };

  LogVector<MediaStorCheck*> m_stor_check;
  MediaStorCheck* m_current_storage;
  MediaStorage m_media_storage[MT_STOR_END];
  // CP storage handle
  LogList<CpStorage*> m_cp_handles;
  // File watcher
  FileWatcher* m_file_watcher;
  // Event clients
  LogList<EventClient*> m_event_clients;
  // Umount event clients
  LogList<EventClient*> m_umt_event_clients;
  // Use external storage on FUSE? (Only valid for auto detection)
  bool m_use_ext_stor_fuse;
  // uevent monitor
  UeventMonitor* m_uevent_monitor;
};

#endif  // !_STOR_MGR_H_
