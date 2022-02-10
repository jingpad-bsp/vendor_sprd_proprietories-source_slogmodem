/*
 * convey_unit.cpp - a collection of source stuffs and
 *                   destionationto be copied
 *
 * Copyright (C) 2017 Spreadtrum Communication Inc.
 *
 * History:
 * 2017-9-18 YAN Zhihang
 * Initial version
 */

#include "convey_unit.h"
#include "convey_workshop.h"
#include "stor_mgr.h"

template<typename SrcType, typename DestType>
ConveyUnit<SrcType, DestType>::ConveyUnit(StorageManager* sm,
                                          CpType ct,
                                          CpClass cpclass,
                                          const SrcType* src,
                                          unsigned src_priority,
                                          DestType* dest,
                                          unsigned dest_priority)
    : ConveyUnitBase(sm, ct, cpclass, src_priority, dest_priority),
      src_{src},
      dest_{dest} {}

template<typename SrcType, typename DestType>
bool ConveyUnit<SrcType, DestType>::inspect_event_check(unsigned event_info) {
  bool run = true;
  ConveyWorkshop::InspectEvent ie =
      static_cast<ConveyWorkshop::InspectEvent>(
          static_cast<uint8_t>(event_info >> 16));

  if (ConveyWorkshop::None == ie) {
    return run;
  }

  if (ConveyWorkshop::Stop == ie) {
    run = false;
    state_ = NoWorkload;
  } else if (ConveyWorkshop::Clean == ie) {
    run = false;
    state_ = Clean;
  } else if (ConveyWorkshop::Cancel == ie) {
    if (tag_ == static_cast<uint8_t>(event_info)) {
      run = false;
      state_ = Cancelled;
    }
  } else {
    StorageManager::MediaPrior mp =
        static_cast<StorageManager::MediaPrior>(
            static_cast<uint8_t>(event_info));
    StorageManager::MediaType mt =
        static_cast<StorageManager::MediaType>(
            static_cast<uint8_t>(event_info >> 8));

    unsigned priority = (static_cast<unsigned>(mt) << 8) +
        static_cast<unsigned>(mp);

    if (ConveyWorkshop::Vanish == ie) {
      if (src_priority() == priority) {
        state_ = SrcVanish;
        run = false;
      } else if (dest_priority() == priority) {
        run = false;
        if (auto_dest_) {
          state_ = CommonDestVanish;
          dest_ = nullptr;
        } else {
          state_ = DestVanish;
        }
      }
    } else if (ConveyWorkshop::CommonDestChange == ie &&
               auto_dest_ &&
               dest_priority() != priority) {
      run = false;
      state_ = CommonDestChange;
    }
  }

  return run;
}

/* cp directory to cp directory convey */
template class ConveyUnit<CpDirectory, CpDirectory>;
/* small memory content copy to a single log file */
template class ConveyUnit<uint8_t, LogFile>;
/* copy a file to a single log file of a file path */
template class ConveyUnit<char, LogFile>;
