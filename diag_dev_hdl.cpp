/*
 *  diag_dev_hdl.cpp - The diagnosis device handler.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-6-13 Zhang Ziyi
 *  Initial version.
 */

#include "diag_dev_hdl.h"
#include "trans_diag.h"

DiagDeviceHandler::DiagDeviceHandler(const LogString& file_path,
                                     TransDiagDevice* trans,
                                     LogController* ctrl,
                                     Multiplexer* multiplexer)
    : DeviceFileHandler(file_path, 1024 * 32, ctrl, multiplexer),
      diag_trans_{trans} {}

DiagDeviceHandler::DiagDeviceHandler(int fd,
                                     TransDiagDevice* trans,
                                     LogController* ctrl,
                                     Multiplexer* multiplexer)
    : DeviceFileHandler(fd, 1024 * 32, ctrl, multiplexer),
      diag_trans_{trans} {}

bool DiagDeviceHandler::process_data(DataBuffer& buffer) {
  bool ret{true};

  if (diag_trans_) {
    ret = diag_trans_->process(buffer);
  } else {
    err_log("No diag trans for %s", ls2cstring(path()));
  }

  return ret;
}
