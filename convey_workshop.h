/*
 * convey_workshop.h - work force for convey tasks
 *
 *  Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-9-19 Yan Zhihang
 *  Initial version.
 */

#ifndef _CONVEY_WORKSHOP_
#define _CONVEY_WORKSHOP_

#include <functional>
#include <thread>

#include "convey_unit_base.h"
#include "def_config.h"
#include "fd_hdl.h"
#include "concurrent_queue.h"

class LogController;
class Multiplexer;
class StorageManager;

class ConveyWorkshop : public FdHandler {
 public:
  using UnitDoneCallback =
      std::function<void(void* client,
                         std::unique_ptr<ConveyUnitBase>&& item)>;

  enum InspectEvent {
    None,
    NewItem,
    Stop,
    Vanish,
    CommonDestChange,
    Clean,
    Cancel
  };

  ConveyWorkshop(LogController* log_ctrl,
                 StorageManager* stor_mgr, Multiplexer* multi);
  ~ConveyWorkshop();
  ConveyWorkshop(const ConveyWorkshop&) = delete;
  ConveyWorkshop& operator=(const ConveyWorkshop&) = delete;

  bool init();
  void process(int events) override;
  /*
   * attach_request - new client registered with a list of convey unit
   *
   * @client: the client
   * @callback: the callback if one unit of client is done
   * @cu_lsit: the list of the client's units
   *
   * Return Value:
   *  True if attach OK, else false.
   */
  bool attach_request(void* client, UnitDoneCallback callback,
                      LogList<std::unique_ptr<ConveyUnitBase>>& cu_list);
  /*
   * add_new_unit - add a new unit of the already registered client
   *
   * @client: the client registred
   * @cu: new convey unit of the registered client
   *
   * Return Value:
   *  True if attach OK, else false.
   */
  bool add_new_unit(void* client, std::unique_ptr<ConveyUnitBase> cu);

  void cancel_client(void* client);
  void clear();

 private:
  /*
   * work_routine - convey work process
   *
   * @index: index of message queue
   */
  void work_routine(unsigned index);

  void item_done(std::unique_ptr<ConveyUnitBase>& item);

  static void current_media_changed(void* client);
  static void media_inactive(void* client, unsigned priority);

 private:
  using Inspector = std::function<unsigned(bool)>;
  using MsgQueue = ConcurrentQueue<unsigned>;

  enum WorkFeedback {
    ItemFinish,
    WorkRoutineClean,
    WorkRoutineIdle,
    WorkRoutineExit
  };

  struct ConveyClient {
    void* client_ptr;
    unsigned num_request;
    UnitDoneCallback client_callback;

    ConveyClient(void* client, unsigned num, UnitDoneCallback callback)
        : client_ptr{client}, num_request{num}, client_callback{callback} {}

    void add_num(unsigned n = 1) { num_request += n; }
    void item_done(std::unique_ptr<ConveyUnitBase>&& cu) {
      --num_request;

      if (client_ptr && client_callback) {
        client_callback(client_ptr, std::move(cu));
      }

      if (0 == num_request) {
        client_ptr = nullptr;
        client_callback = nullptr;
      }
    }
  };

  // inited state
  bool inited_;
  StorageManager* stor_mgr_;
  Multiplexer* multi_;
  // raw material queue contains items to be processed
  ConcurrentQueue<ConveyUnitBase> raw_material_;
  // products contains finished items
  ConcurrentQueue<ConveyUnitBase> products_;
  // work loads
  LogVector<std::thread> work_machines_;
  MsgQueue messages_[CONCURRENCY_CAPACITY];
  int feedback_fd_;
  LogVector<std::unique_ptr<ConveyClient>> client_table_;
};

#endif // !_CONVEY_WORKSHOP_
