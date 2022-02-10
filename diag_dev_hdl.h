/*
 *  diag_dev_hdl.h - The diagnosis device handler.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-6-13 Zhang Ziyi
 *  Initial version.
 */
#ifndef _DIAG_DEV_HDL_H_
#define _DIAG_DEV_HDL_H_

#include "dev_file_hdl.h"

class TransDiagDevice;

class DiagDeviceHandler : public DeviceFileHandler {
 public:
  DiagDeviceHandler(const LogString& file_path, TransDiagDevice* trans,
                    LogController* ctrl, Multiplexer* multiplexer);
  DiagDeviceHandler(int fd, TransDiagDevice* trans, LogController* ctrl,
                    Multiplexer* multiplexer);

 private:
  // DeviceFileHandler::process_data()
  bool process_data(DataBuffer& buffer) override;

 private:
  TransDiagDevice* diag_trans_;
};

#endif  // !_DIAG_DEV_HDL_H_
