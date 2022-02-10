/*
 *  dev_file_hdl.h - The device file handler.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-6-13 Zhang Ziyi
 *  Initial version.
 */
#ifndef _DEV_FILE_HDL_H_
#define _DEV_FILE_HDL_H_

#include "cp_log_cmn.h"
#include "data_buf.h"
#include "fd_hdl.h"

class DeviceFileHandler : public FdHandler {
 public:
  DeviceFileHandler(const LogString& dev_path, size_t buf_len,
                    LogController* ctrl, Multiplexer* multiplexer);
  DeviceFileHandler(int fd, size_t buf_len, LogController* ctrl,
                    Multiplexer* multiplexer);
  DeviceFileHandler(size_t buf_len, LogController* ctrl,
                    Multiplexer* multiplexer);
  DeviceFileHandler(const DeviceFileHandler&) = delete;
  ~DeviceFileHandler();

  DeviceFileHandler& operator = (const DeviceFileHandler&) = delete;

  const LogString& path() const { return m_file_path; }

  void set_path(const char* path) { m_file_path = path; }

  int open();
  /*  open_reg - open the file and register to the multiplexer.
   *
   *  Return the file descriptor on success, -1 otherwise.
   */
  int open_reg();
  int close();
  void close_unreg();

  // FdHandler::process()
  void process(int events) override;

 private:
  /*  process_data - process the read data.
   *
   *  process_data() is called by process() to process the data read.
   *  If process_data returns true, process() will stop trying read
   *  and return; otherwise process() will continue reading the file.
   */
  virtual bool process_data(DataBuffer& buffer) = 0;

 private:
  // Close the file descriptor on destruction
  bool m_close_fd;
  // Full path of the device file
  LogString m_file_path;
  // Data buffer
  DataBuffer m_buffer;
};

#endif  // !_DEV_FILE_HDL_H_
