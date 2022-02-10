/*
 * convey_workshop.cpp - work force for convey tasks
 *
 *  Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-9-19 Yan Zhihang
 *  Initial version.
 */
#include <algorithm>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "convey_workshop.h"
#include "media_stor.h"
#include "rw_buffer.h"
#include "stor_mgr.h"

ConveyWorkshop::ConveyWorkshop(LogController* log_ctrl,
                               StorageManager* stor_mgr,
                               Multiplexer* multi)
    : FdHandler{-1, log_ctrl, multi},
      inited_{false},
      stor_mgr_{stor_mgr},
      multi_{multi},
      feedback_fd_{-1} {}

ConveyWorkshop::~ConveyWorkshop() {
  clear();

  for (int i = 0; i != CONCURRENCY_CAPACITY; ++i) {
    messages_[i].push(std::unique_ptr<unsigned>{
        new unsigned(static_cast<unsigned>(Stop << 16))});
  }

  for (unsigned i = 0; i != work_machines_.size(); ++i) {
    if (work_machines_[i].joinable()) {
      work_machines_[i].join();
    }
  }

  if (feedback_fd_ >= 0) {
    ::close(feedback_fd_);
    feedback_fd_ = -1;
  }
}

bool ConveyWorkshop::init() {
  if (inited_) {
    return true;
  }

  int socks[2] = {0, 0};
  if (-1 == socketpair(AF_LOCAL, SOCK_STREAM, 0, socks)) {
    err_log("socketpair creation for unifier is failed.");
    return false;
  }

  // Put the sockets to non-blocking mode
  if (set_nonblock(socks[0]) && set_nonblock(socks[1])) {
    err_log("unifier socket set O_NONBLOCK failed.");
    ::close(socks[0]);
    ::close(socks[1]);
    return false;
  }

  m_fd = socks[0];
  feedback_fd_ = socks[1];

  for (unsigned i = 0; i < CONCURRENCY_CAPACITY; ++i) {
    work_machines_.push_back(
        std::thread(
            [=] { work_routine(i); }
        ));
  }

  multi_->register_fd(this, POLLIN);

  inited_ = true;

  return true;
}

void ConveyWorkshop::process(int /* events */) {
  uint8_t cmd;

  ssize_t ret = ::read(m_fd, &cmd, 1);

  if (ret == 1) {
    switch (static_cast<WorkFeedback>(cmd)) {
      case ItemFinish:
        // summry of the finished products
        while (1) {
          auto cu_next = products_.get_next(false);
          if (cu_next) {
            item_done(cu_next);
          } else {
            break;
          }
        }
        break;
      default:
        info_log("cmd: %d", cmd);
        break;
    }
  } else {
    err_log("read error");
  }
}

bool ConveyWorkshop::attach_request(
    void* client, UnitDoneCallback callback,
    LogList<std::unique_ptr<ConveyUnitBase>>& cu_list) {
  bool ret = false;

  if (0 == cu_list.size()) {
    err_log("empty convey request");
    return ret;
  }

  if (nullptr == client || nullptr == callback) {
    err_log("no client notification callback is provided");
    return ret;
  }

  auto client_it =
      std::find_if(client_table_.begin(),
                   client_table_.end(),
                   [] (const std::unique_ptr<ConveyClient>& cc) {
                       return 0 == cc->num_request;
                   });

  uint8_t tag = UINT8_MAX;

  if (client_table_.end() != client_it) {
    tag = client_it - client_table_.begin();
    (*client_it)->client_ptr = client;
    (*client_it)->num_request = cu_list.size();
    (*client_it)->client_callback = callback;
  } else if (client_table_.end() == client_it &&
             UINT8_MAX > client_table_.size()) {

    if (0 == client_table_.size()) {
      // storage media event should be notified
      stor_mgr_->subscribe_media_change_evt(this, current_media_changed);
      stor_mgr_->subscribe_stor_inactive_evt(this, media_inactive);
    }

    tag = static_cast<unsigned>(client_table_.size());

    client_table_.push_back(
        std::unique_ptr<ConveyClient>{
            new ConveyClient(client,
                             static_cast<unsigned>(cu_list.size()),
                             callback)
        });
  } else {
    info_log("client table size: %d", client_table_.size());
    return ret;
  }

  for (auto it = cu_list.begin(); it != cu_list.end(); ++it) {
    (*it)->set_tag(tag);
    raw_material_.push(std::move(*it));
    ret = true;
  }

  // inform the work routine to process the request
  for (int i = 0; i != CONCURRENCY_CAPACITY; ++i) {
    messages_[i].push(
        std::unique_ptr<unsigned>{
            new unsigned(static_cast<unsigned>(NewItem << 16))});
  }

  return ret;
}

bool ConveyWorkshop::add_new_unit(void* client, std::unique_ptr<ConveyUnitBase> cu) {
  auto client_it =
      std::find_if(client_table_.begin(),
                   client_table_.end(),
                   [&client] (const std::unique_ptr<ConveyClient>& cc) {
                       return client == cc->client_ptr;
                   });

  if (client_table_.end() == client_it) {
    err_log("no correspond client is registered");
    return false;
  }

  uint8_t tag = client_it - client_table_.begin();
  cu->set_tag(tag);

  client_table_[tag]->add_num();
  raw_material_.push(std::move(cu));
  // inform the work routine to process the request
  for (int i = 0; i != CONCURRENCY_CAPACITY; ++i) {
    messages_[i].push(
        std::unique_ptr<unsigned>{
            new unsigned(static_cast<unsigned>(NewItem << 16))});
  }

  return true;
}

void ConveyWorkshop::item_done(std::unique_ptr<ConveyUnitBase>& item) {
  unsigned tag = item->tag();

  if (tag >= client_table_.size() || 0 == client_table_[tag]->num_request) {
    err_log("no correspond request client");
  } else {
    client_table_[tag]->item_done(std::move(item));

    // if the tag's client is the last in client table
    // and the client's request number is 0, erase all the
    // clients from the end untill who's request number non zero
    if (tag + 1 == client_table_.size()) {
      auto begin_it = client_table_.begin();
      while (0 == client_table_[tag]->num_request) {
        client_table_.erase(begin_it + tag);

        if (0 == tag) {
          break;
        } else {
          --tag;
        }
      }
      // unsubscribe the media event if no clients left
      if (0 == tag) {
        stor_mgr_->unsubscribe_media_change_evt(this);
        stor_mgr_->unsubscribe_stor_inactive_evt(this);
      }
    }
  }
}

void ConveyWorkshop::work_routine(unsigned index) {
  // inspect interface for synchronisation with main thread's events
  Inspector inspect_interface = [=] (bool block) {
    unsigned ret = None;
    auto msg = messages_[index].get_next(block);
    if (msg) {
      ret = *msg;
      info_log("event info 0X%x - event id: %u",
               static_cast<unsigned>(ret),
               static_cast<unsigned>(ret >> 16));
    }
    return ret;
  };

  // loop the convey unit task
  bool run = true;
  uint8_t fb = static_cast<uint8_t>(ItemFinish);
  while (run) {
    switch (static_cast<InspectEvent>(inspect_interface(true) >> 16)) {
      case NewItem:
        {
          // allocate a buffer
          RwBuffer rw_buf;
          while (run) {
            auto material{raw_material_.get_next(false)};
            if (material) {
              info_log("has material");
              material->unit_start();
              material->convey_method(inspect_interface,
                                      rw_buf.get_buf(),
                                      rw_buf.size());
              info_log("state: %d", material->state());
              switch (material->state()) {
                case ConveyUnitBase::NoWorkload:
                  // this work routine is closing, give it back to raw materials
                  raw_material_.push(std::move(material));
                  run = false;
                  fb = static_cast<uint8_t>(WorkRoutineExit);
                  ::write(feedback_fd_, &fb, 1);
                  break;
                case ConveyUnitBase::Clean:
                  fb = static_cast<uint8_t>(WorkRoutineClean);
                case ConveyUnitBase::Cancelled:
                case ConveyUnitBase::CommonDestVanish:
                case ConveyUnitBase::CommonDestChange:
                  material->clear_result();
                case ConveyUnitBase::SrcVanish:
                case ConveyUnitBase::DestVanish:
                case ConveyUnitBase::Done:
                case ConveyUnitBase::Failed:
                  products_.push(std::move(material));
                  ::write(feedback_fd_, &fb, 1);
                  break;
                default:
                  err_log("Wrong state got %d", material->state());
                  break;
              }
            } else {
              info_log("no material");
              break;
            }
          }
        }
        break;
      case Stop:
        run = false;
        break;
      case Clean:
        fb = static_cast<uint8_t>(WorkRoutineClean);
        ::write(feedback_fd_, &fb, 1);
        break;
      default:
        break;
    }
  }
}

void ConveyWorkshop::current_media_changed(void* client) {
  ConveyWorkshop* cw = static_cast<ConveyWorkshop*>(client);

  MediaStorage* cur_media = cw->stor_mgr_->get_media_stor();
  if (!cur_media) {
    err_log("no media storage available");
    return;
  }

  unsigned cur_priority = cur_media->priority();
  // move the original raw material queue to a tmp queue for preparation check
  // which will leave the raw material queue empty, thus block the work routines
  ConcurrentQueue<ConveyUnitBase> semi_material(std::move(cw->raw_material_));
  // inform the media change event
  for (int i = 0; i != CONCURRENCY_CAPACITY; ++i) {
    (cw->messages_)[i].push(
        std::unique_ptr<unsigned>{
            new unsigned(static_cast<unsigned>(CommonDestChange << 16) +
                         cur_priority)});
  }

  // check for the pending raw material
  while (1) {
    auto cu_next = semi_material.get_next(false);
    if (cu_next) {
      if (cu_next->auto_dest()) {
        if (cu_next->src_priority() == cur_priority) {
          cu_next->unit_done(ConveyUnitBase::Done);
        } else {
          cu_next->unit_done(ConveyUnitBase::CommonDestChange);
        }

        cw->item_done(cu_next);
      } else {
        cw->raw_material_.push(std::move(cu_next));
      }
    } else {
      break;
    }
  }
}

void ConveyWorkshop::media_inactive(void* client, unsigned priority) {
  ConveyWorkshop* cw = static_cast<ConveyWorkshop*>(client);
  // same as media change event
  auto semi_material(std::move(cw->raw_material_));
  // inform the media vanish event with correspond media priority
  for (int i = 0; i != CONCURRENCY_CAPACITY; ++i) {
    (cw->messages_)[i].push(
        std::unique_ptr<unsigned>{
            new unsigned(static_cast<unsigned>(Vanish << 16) + priority)});
  }

  while (1) {
    auto cu_next = semi_material.get_next(false);

    if (cu_next) {
      if (cu_next->src_priority() == priority) {
        cu_next->unit_done(ConveyUnitBase::SrcVanish);
        cw->item_done(cu_next);
      } else if (cu_next->dest_priority() == priority) {
        if (cu_next->auto_dest()) {
          cu_next->unit_done(ConveyUnitBase::CommonDestVanish);
        } else {
          cu_next->unit_done(ConveyUnitBase::DestVanish);
        }

        cw->item_done(cu_next);
      } else {
        cw->raw_material_.push(std::move(cu_next));
      }
    } else {
      break;
    }
  }
}

void ConveyWorkshop::clear() {
  if (0 == client_table_.size()) {
    return;
  }

  // clear all the material
  raw_material_.clear();

  int i = 0;
  // inform the work routine
  for (i = 0; i != CONCURRENCY_CAPACITY; ++i) {
    messages_[i].push(std::unique_ptr<unsigned>{
        new unsigned(static_cast<unsigned>(Clean << 16))});
  }

  struct pollfd rsp_pol;
  int err = 0;
  while (i) {
    rsp_pol.fd = m_fd;
    rsp_pol.events = POLLIN;
    err = poll(&rsp_pol, 1, -1);
    if (err > 0 && (rsp_pol.revents & POLLIN)) {
      uint8_t res;
      ssize_t n = ::read(fd(), &res, 1);
      if (1 == n && (WorkRoutineClean == static_cast<WorkFeedback>(res))) {
        --i;
      }
    }
  }

  products_.clear();

  // clear all client
  for (auto client_it = client_table_.begin();
       client_it != client_table_.end(); ++client_it) {
    if (*client_it){
      (*client_it)->client_callback((*client_it)->client_ptr, nullptr);
    } else {
      err_log("*client_it is null");
    }
  }
  client_table_.clear();
  stor_mgr_->unsubscribe_media_change_evt(this);
  stor_mgr_->unsubscribe_stor_inactive_evt(this);
}

void ConveyWorkshop::cancel_client(void* client) {
  auto client_it =
      std::find_if(client_table_.begin(),
                   client_table_.end(),
                   [&client] (const std::unique_ptr<ConveyClient>& cc) {
                       return cc->client_ptr == client; }
                   );
  if (client_table_.end() == client_it) {
    err_log("no correspond client in convey table list");
    return;
  }

  auto current_raw(std::move(raw_material_));

  uint8_t tag = static_cast<uint8_t>(client_it - client_table_.begin());
  // inform the work routine to cancel the correspond unit
  for (int i = 0; i != CONCURRENCY_CAPACITY; ++i) {
    messages_[i].push(
        std::unique_ptr<unsigned>{
            new unsigned(static_cast<unsigned>(Cancel << 16) + tag)});
  }

  (*client_it)->client_ptr = nullptr;
  (*client_it)->client_callback = nullptr;

  while (1) {
    auto cu_next = current_raw.get_next(false);
    if (cu_next) {
      if (cu_next->tag() == tag) {
        --(*client_it)->num_request;
        continue;
      }

      raw_material_.push(std::move(cu_next));
    } else {
      break;
    }
  }
  // if the tag's client is the last in client table
  // and the request unit's number is 0, erase all the
  // clients from the client table who's request number is 0
  if (tag + 1 == client_table_.size() &&
      0 == (*client_it)->num_request) {
    auto begin_it = client_table_.begin();
    do {
      client_table_.erase(begin_it + tag);
      if (0 == tag) {
        break;
      }
      --tag;
    } while (0 == client_table_[tag]->num_request);

    if (0 == tag) {
      stor_mgr_->unsubscribe_media_change_evt(this);
      stor_mgr_->unsubscribe_stor_inactive_evt(this);
    }
  }
}
