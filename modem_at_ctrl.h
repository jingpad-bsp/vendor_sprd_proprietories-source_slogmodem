/*
 *  modem_at_ctrl.h - The MODEM AT controller.
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2017-6-3 Zhang Ziyi
 *  Initial version.
 */

#ifndef MODEM_AT_CTRL_H_
#define MODEM_AT_CTRL_H_

#include "dev_file_hdl.h"
#include "timer_mgr.h"
#include "trans_mgr.h"

class ModemAtController : public DeviceFileHandler {
 public:
  enum State {
    IDLE,
    BUSY
  };

  enum ActionType {
    AT_NONE,
    AT_SUBSCRIBE_EVENT,
    AT_UNSUBSCRIBE_EVENT,
    AT_EXPORT_LOG,
    AT_SYNC_TIME
  };

  // Action result
  static const int RESULT_OK = 0;
  static const int RESULT_ERROR = -1;
  static const int RESULT_TIMEOUT = -2;

  enum ModemEvent {
    ME_UNRECOGNIZED_SIM,
    ME_LIMITED_SERVICE,
    ME_REG_REJECTED = 0x100,
    ME_NET_SEL_FAILURE,
    ME_CSFB_CALL_FAILURE,
    ME_PDP_ACT_REJECTED,
    ME_IMS_LINK_FAILURE = 0x200,
    ME_IMS_NET_NO_RSP,
    ME_IMS_REG_REJECTED,
    ME_LTE_REG_FAILURE = 0x300,
    ME_LTE_LINK_FAILURE,
    ME_LTE_REEST,
    ME_LTE_NET_DROP,
    ME_LTE_UPLINK_OOS,
    ME_LTE_MAX_SR,
    ME_LTE_RA_FAILURE,
    ME_LTE_MAX_HARQ_RETR,
    ME_LTE_MAX_RLC_RETR,
    ME_WCDMA_REG_FAILURE = 0x400,
    ME_WCDMA_LINK_FAILURE,
    ME_WCDMA_REEST,
    ME_WCDMA_NET_DROP,
    ME_WCDMA_RLC_RESET,
    ME_WCDMA_RLC_UNRECOV,
    ME_WCDMA_DMA_HANG,
    ME_WCDMA_DECRYPT_FAILURE,
    ME_GSM_BAD_SACCH_REC = 0x500,
    ME_GSM_MODE_SWITCH_FAILURE
  };

  using ResultCallback_t = void (*)(void*, ActionType, int);
  using IntResultCallback_t = void (*)(void* client, const uint8_t* line,
                                       size_t len);
  /*  EventCallback_t - the MODEM event notification function.
   *  @client: the client pointer.
   *  @sim: the 0 based index of the SIM.
   *  @event: the event ID.
   */
  using EventCallback_t = void (*)(void* client, int sim, int event);

  ModemAtController(LogController* ctrl, Multiplexer* multiplexer);
  ModemAtController(const ModemAtController&) = delete;
  ~ModemAtController();

  ModemAtController& operator = (const ModemAtController&) = delete;

  bool is_busy() const { return BUSY == state_; }
  ActionType action() const { return action_; }

  /*  crash - send AT+SPATASSERT=1 to crash the MODEM.
   *
   *  crash() won't check whether there is currently a command
   *  being executed and will send the crash command directly.
   *
   *  Return 0 if the command is sent, -1 otherwise.
   */
  int crash();

  /*  start_subscribe_event - start to subscribe to the event.
   *  @client: client pointer.
   *  @cb: result callback function.
   *
   *  Return Value:
   *    0: process started.
   *    1: process finished successfully.
   *    -1: error.
   */
  int start_subscribe_event(void* client, ResultCallback_t cb);
  int start_unsubscribe_event(void* client, ResultCallback_t cb);
  int start_get_tick(void* client,ResultCallback_t cb,
                     IntResultCallback_t int_cb);

  /*  start_export_evt_log - start to export event log.
   *  @client: client pointer.
   *  @cb: result callback function.
   *  @to: the timeout in unit of millisecond.
   *
   *  Return Value:
   *    0: process started.
   *    -1: error.
   */
  int start_export_evt_log(void* client, ResultCallback_t cb,
                           unsigned to);

  void cancel_trans();

  void register_events(void* client, EventCallback_t cb);

  void unregister_events();

 private:
  enum CommandResult {
    CR_OK,
    CR_ERROR
  };

  // DeviceFileHandler::process_data()
  bool process_data(DataBuffer& buf) override;

  void parse_line(const uint8_t* line, size_t len);
  void on_command_result(int res);
  void process_event(const uint8_t* line, size_t len);

  static bool is_evt_valid(unsigned long evt);
  static void at_cmd_timeout(void* client);

 private:
  State state_;
  ActionType action_;
  void* client_;
  ResultCallback_t result_cb_;
  IntResultCallback_t int_result_cb_;
  void* event_client_;
  EventCallback_t event_cb_;
  TimerManager::Timer* cmd_timer_;
};

#endif  // !MODEM_AT_CTRL_H_
