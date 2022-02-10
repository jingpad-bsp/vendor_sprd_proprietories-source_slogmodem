/*
 * convey_unit.h - workshop's standard unit
 *
 * Conveyright (C) 2017 Spreadtrum Communication Inc.
 *
 * History:
 * 2017-9-18 YAN Zhihang
 * Initial version
 *
 */

#ifndef _CONVEY_UNIT_
#define _CONVEY_UNIT_

#include "convey_unit_base.h"

template<typename SrcType, typename DestType>
class ConveyUnit : public ConveyUnitBase {
 public:
  ConveyUnit(StorageManager* sm,  /* storage manager */
             CpType type,         /* each unit is correspond to a cp type */
             CpClass cpclass,     /* and a cp class */
             const SrcType* src,  /* source (cp log file, cp log directory,
                                     or any path out of slogmodem control) */
             unsigned src_priority, /* src media storage priority */
             DestType* dest = nullptr, /* destination under slogmodem control */
             unsigned dest_priority = UINT_MAX /* dest media storage priority */
             );
  virtual ~ConveyUnit() {}
  ConveyUnit(const ConveyUnit&) = delete;
  ConveyUnit& operator=(const ConveyUnit&) = delete;
  ConveyUnit& operator=(ConveyUnit&&) = delete;
  ConveyUnit(ConveyUnit&& cu) = delete;

 protected:
  /*
   * inspect_event_check - check the message of event
   *
   * @event_info: event information
   *
   * Return Value:
   *  True if event is NOT correspond, otherwise false.
   */
  bool inspect_event_check(unsigned event_info);

 protected:
  const SrcType* src_;
  DestType* dest_;
};

#endif // !_CONVEY_UNIT_
