/*
 *  modem_parse_library_save.h - save modem log parse library
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-4-1 Yan Zhihang
 *  Initial version.
 */
#ifndef _MODEM_PARSE_LIB_
#define _MODEM_PARSE_LIB_

#include "cp_log_cmn.h"
#include "trans_log_convey.h"

class CpDirectory;
class WanModemLogHandler;

class ModemParseLibrarySave : public TransLogConvey {
 public:
  ModemParseLibrarySave(TransactionManager* mgr,
                        WanModemLogHandler* wan,
                        CpDirectory* dir);
  ~ModemParseLibrarySave();
  ModemParseLibrarySave& operator=(const ModemParseLibrarySave&) = delete;
  ModemParseLibrarySave(const ModemParseLibrarySave&) = delete;

  // Transaction::execute()
  int execute() override;

 private:
  bool init();

  bool check_parse_lib();

  /*  save_log_parsing_lib - save the MODEM log parsing lib
   *
   *  Return Value:
   *    Return 0 on success, -1 otherwise.
   *
   */
  static int save_log_parsing_lib();

  static int save_log_lib(const LogString& lib_name, const LogString& sha1_name,
                          int modem_part, size_t offset, size_t len,
                          const uint8_t* sha1);

  /*  get_image_path - get the device file path of the MODEM image partition.
   *  @ct: the WAN MODEM type
   *  @img_path: the device file path resulted.
   *
   *  Return Value:
   *    Return 0 on success, -1 otherwise.
   *
   */
  static int get_image_path(CpType ct, LogString& img_path);

  static int get_modem_img(int img_fd, uint8_t* new_sha1, size_t& lib_offset,
                           size_t& lib_len);

  static void* save_parse_lib(void* arg);

 private:
  WanModemLogHandler* wan_modem_;
  LogString modem_img_path_;
  int modem_fd_;
  uint8_t new_sha1_[40];
  size_t lib_offset_;
  size_t lib_len_;
  CpDirectory* dest_dir_;
};

#endif //!_MODEM_PARSE_LIB_
