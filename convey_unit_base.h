/*
 * convey_unit_base.h - workshop's standard base unit
 *
 * Conveyright (C) 2017 Spreadtrum Communication Inc.
 *
 * History:
 * 2017-9-18 YAN Zhihang
 * Initial version
 *
 */

#ifndef _CONVEY_UNIT_BASE_
#define _CONVEY_UNIT_BASE_

#include <climits>
#include <functional>

#include "cp_log_cmn.h"

class StorageManager;

class ConveyUnitBase {
 public:
  enum State {
    Wait,
    OnGoing,
    NoWorkload,
    SrcVanish,
    DestVanish,
    CommonDestVanish,
    CommonDestChange,
    Clean,
    Cancelled,
    Failed,
    Done
  };

  ConveyUnitBase(StorageManager* sm, CpType type, CpClass cpclass,
                 unsigned src_priority = UINT_MAX,
                 unsigned dest_priority = UINT_MAX);
  virtual ~ConveyUnitBase() {}

  ConveyUnitBase(const ConveyUnitBase&) = delete;
  ConveyUnitBase& operator=(const ConveyUnitBase&) = delete;
  ConveyUnitBase& operator=(ConveyUnitBase&&) = delete;
  ConveyUnitBase(ConveyUnitBase&& cu) = delete;

  State state() const { return state_; }

  bool auto_dest() const { return auto_dest_; }

  void set_tag(uint8_t tag) { tag_ = tag; }
  uint8_t tag() const { return tag_; }

  void unit_start() { state_ = OnGoing; }
  void unit_done(State state) { state_ = state; }

  unsigned src_priority() const { return src_priority_; }
  unsigned dest_priority() const { return dest_priority_; }
  /*
   * same_src_dest - test if src is the same as dest
   *
   * Return true if accessible, else false
   */
  bool same_src_dest() const { return src_priority_ == dest_priority_; }
  /*
   * pre_convey - prepare for convey process
   */
  virtual bool pre_convey() = 0;
  /*
   * convey_method - method to do the convey
   *
   * @inspector: workshop's check function during the unit's work
   * @buf: buffer for disk IO
   * @size_t: size of buffer
   */
  virtual void convey_method(std::function<unsigned(bool)> inspector,
                             uint8_t* const buf,
                             size_t buf_size) = 0;
  /*
   * post_convey - process to be done after the unit's work done
   */
  virtual void post_convey() = 0;
  /*
   * clear_result - clear all conveyed result
   */
  virtual void clear_result() = 0;
  /*
   * check_src - check validation of source
   *
   * Return true if accessible, else false
   */
  virtual bool check_src() = 0;
  /*
   * check_dest - check validation of destination
   *
   * Return true if accessible, else false
   */
  virtual bool check_dest() = 0;

 protected:
  // convey state
  State state_;
  StorageManager* sm_;
  CpType type_;
  CpClass cpclass_;
  unsigned src_priority_;
  unsigned dest_priority_;
  // index of client table in Convey Workshop
  uint8_t tag_;
  // if destination is not set initially
  bool auto_dest_;
};

#endif // !_CONVEY_UNIT_BASE_
