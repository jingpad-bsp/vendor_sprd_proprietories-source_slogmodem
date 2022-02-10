/*
 *  modem_cmd_ctrl.h - The MODEM command controller.
 *
 *  Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 *  History:
 *
 *  2019-9-20 Pizer.Fan
 *  Initial version.
 */

#ifndef MODEM_CMD_CTRL_H_
#define MODEM_CMD_CTRL_H_

#include "dev_file_hdl.h"
#include "log_config.h"

class ModemCmdController : public DeviceFileHandler {
 public:

  ModemCmdController(LogController* ctrl, Multiplexer* multiplexer);
  ModemCmdController(const ModemCmdController&) = delete;
  ~ModemCmdController();

  ModemCmdController& operator = (const ModemCmdController&) = delete;

  int try_open();

  /*  set_agdsp_log_dest - set the destination of AG DSP LOG in MODEM.
   *  @dest: the destination
   *  Return Value:
   *    Return 0 on success, -1 on failure.
   */
  int set_agdsp_log_dest(LogConfig::AgDspLogDestination dest);

  /*  set_agdsp_pcm_dump - set to dump pcm or not.
   *  @dest: the destination
   *  Return Value:
   *    Return 0 on success, -1 on failure.
   */
  int set_agdsp_pcm_dump(bool pcm);

  /*  set_miniap_log_status - set to miniap log on or off.
   *  @status: the status
   *  Return Value:
   *    Return 0 on success, -1 on failure.
   */
  int set_miniap_log_status(bool status);

  /*  set_orcadp_log_status - set to orcadp log on or off.
   *  @status: the status
   *  Return Value:
   *    Return 0 on success, -1 on failure.
   */
  int set_orcadp_log_status(bool status);

  /*  set_miniap_log_status - set to mipi log information.
   *  @MipiLogList mipi log info list
   *  Return Value:
   *    Return 0 on success, -1 on failure.
   */
  int set_mipi_log_info(LogConfig::MipiLogList mipi_log);

  /*  set_etb_save - set to save etb or not.
   *  Return Value:
   *  Return 0 on success, -1 on failure.
   */
  int set_etb_save();

 private:

  void do_suspend_actions();

  bool is_suspend_set_agdsp_log_dest() const { return agdsp_log_dest_suspended_; }
  void suspend_set_agdsp_log_dest(LogConfig::AgDspLogDestination dest) {
    agdsp_log_dest_suspended_ = true;
    agdsp_log_dest_ = dest;
  }
  void resume_set_agdsp_log_dest();

  bool is_suspend_set_agdsp_pcm_dump() const { return agdsp_pcm_dump_suspended_; }
  void suspend_set_agdsp_pcm_dump(bool pcm) {
    agdsp_pcm_dump_suspended_ = true;
    agdsp_pcm_dump_ = pcm;
  }

  void resume_set_agdsp_pcm_dump();

  bool is_suspend_set_miniap_log_status() const { return miniap_log_status_suspended_; }
  void suspend_set_miniap_log_status(bool status) {
    miniap_log_status_suspended_ = true;
    miniap_log_status = status;
  }
  void resume_set_miniap_log_status();

  bool is_suspend_set_orcadp_log_status() const { return orcadp_log_status_suspended_; }
  void suspend_set_orcadp_log_status(bool status) {
    orcadp_log_status_suspended_ = true;
    orcadp_log_status = status;
  }
  void resume_set_orcadp_log_status();

  bool is_suspend_set_mipi_log_info() const { return mipi_log_info_suspended_; }
  void suspend_set_mipi_log_info(LogConfig::MipiLogList mipi_log_info) {
    mipi_log_info_suspended_ = true;
    mipi_log_info_ = mipi_log_info;
  }
  void resume_set_mipi_log_info();

  /*
   *    reopen_cmd_dev - Try to open the command device.
   *
   *    This function is called by the multiplexer on check event.
   */
  static void reopen_cmd_dev(void* param);

  // DeviceFileHandler::process_data()
  bool process_data(DataBuffer& buf) override;

 private:
  bool agdsp_log_dest_suspended_;
  LogConfig::AgDspLogDestination agdsp_log_dest_;
  bool agdsp_pcm_dump_suspended_;
  bool agdsp_pcm_dump_;

  bool miniap_log_status_suspended_;
  bool miniap_log_status;

  bool orcadp_log_status_suspended_;
  bool orcadp_log_status;

  bool mipi_log_info_suspended_;
  LogConfig::MipiLogList mipi_log_info_;
};

#endif  // !MODEM_CMD_CTRL_H_
