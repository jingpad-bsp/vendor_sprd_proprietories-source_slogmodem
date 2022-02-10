/*
 *  tans_log_convey.cpp - The log convey class
 *
 *  Copyright (C) 2015 - 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-10-13 YAN Zhihang
 *  Initial version.
 *
 */

#include "convey_workshop.h"
#include "trans_log_convey.h"

TransLogConvey::TransLogConvey(TransactionManager* tmgr,
                               TransModem::Type type,
                               ConveyWorkshop* workshop)
    : TransModem{tmgr, type},
      workshop_{workshop},
      item_num_{0},
      fail_num_{0} {}

TransLogConvey::~TransLogConvey() {
  TransLogConvey::cancel();
}

void TransLogConvey::add_units(LogVector<std::unique_ptr<ConveyUnitBase>>& items) {
  for (auto it = items.begin(); it != items.end(); ++it) {
    auto item = std::move(*it);

    if (item->auto_dest()) {
      if (false == item->pre_convey()) {
        err_log("fail to prepare destination");
        continue;
      }
    }

    if (validation_check(item)) {
      trans_list_.push_back(std::move(item));
      ++item_num_;
      info_log("add unit: %d", item_num_);
    } else {
      info_log("not add unit: %d", item_num_);
    }
  }
}

int TransLogConvey::execute() {
  if (0 == item_num_) {
    err_log("nothing will be convey");
    on_finished(TRANS_R_FAILURE);
    return TRANS_E_SUCCESS;
  }

  workshop_->attach_request(this, unit_finish, trans_list_);

  on_started();

  return TRANS_E_STARTED;
}

bool TransLogConvey::validation_check(std::unique_ptr<ConveyUnitBase>& item) {
  bool ret = false;

  if (item->same_src_dest()) {
    err_log("same src and dest");
    return ret;
  }

  if (item->check_src()) {
    if (item->check_dest()) {
      ret = true;
    } else {
      err_log("destination is not accessible");
    }
  } else {
    err_log("source is not accessible");
  }

  return ret;
}

void TransLogConvey::cancel() {
  if (TS_EXECUTING == state()) {
    workshop_->cancel_client(this);
    item_num_ = 0;
    on_canceled();
  }
}

void TransLogConvey::unit_finish(void* trans,
                                 std::unique_ptr<ConveyUnitBase>&& item) {
  TransLogConvey* tlc = static_cast<TransLogConvey*>(trans);
  if (tlc->item_num_ <= 0) {
    err_log("no unfinished item exist");
    return;
  }

  if (nullptr == item) {
    tlc->item_num_ = 0;
  } else {
    --tlc->item_num_;

    switch (item->state()) {
      case ConveyUnitBase::Failed:
      case ConveyUnitBase::SrcVanish:
        ++tlc->fail_num_;
      case ConveyUnitBase::Done:
        item->post_convey();
        break;
      case ConveyUnitBase::DestVanish:
        ++tlc->fail_num_;
        break;
      case ConveyUnitBase::CommonDestVanish:
      case ConveyUnitBase::CommonDestChange:
        if (!item->auto_dest()) {
          err_log("Not a auto destination unit");
          ++tlc->fail_num_;
        } else if (item->pre_convey() &&
                   tlc->validation_check(item)) {
          tlc->workshop_->add_new_unit(tlc, std::move(item));
          ++tlc->item_num_;
        } else {
          ++tlc->fail_num_;
        }
        break;
      default:
        err_log("Wrong state got %d", item->state());
        break;
    }
  }

  if (0 == tlc->item_num_) {
    int res = TRANS_R_SUCCESS;

    if (tlc->fail_num_ > 0) {
      res = TRANS_R_FAILURE;
    }

    tlc->finish(res);
  }
}

