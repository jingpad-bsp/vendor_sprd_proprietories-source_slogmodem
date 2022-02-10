/*
 *  modem_parse_library_save.cpp - save modem log parse library
 *
 *  Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 *  History:
 *  2017-4-1 Yan Zhihang
 *  Initial version.
 */

#include <cstdlib>
#ifdef HOST_TEST_
#include "prop_test.h"
#else
#include <cutils/properties.h>
#endif

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "copy_to_log_file.h"
#include "cp_log_cmn.h"
#include "def_config.h"
#include "log_ctrl.h"
#include "log_config.h"
#include "log_file.h"
#include "modem_parse_library_save.h"
#include "wan_modem_log.h"
#include "write_to_log_file.h"

ModemParseLibrarySave::ModemParseLibrarySave(TransactionManager* mgr,
                                             WanModemLogHandler* wan,
                                             CpDirectory* dir)
    : TransLogConvey{mgr, TransModem::MODEM_PARSE_LIB_SAVE,
                     wan->controller()->convey_workshop()},
      wan_modem_{wan},
      modem_fd_{-1},
      new_sha1_{0},
      lib_offset_{0},
      lib_len_{0},
      dest_dir_{dir} {}

ModemParseLibrarySave::~ModemParseLibrarySave() {
  ::close(modem_fd_);
  modem_fd_ = -1;
}

bool ModemParseLibrarySave::init() {
  CpType ct = wan_modem_->type();

  if (CT_UNKNOWN == ct) {
    err_log("can not get MODEM type");
    return false;
  }

  if (get_image_path(ct, modem_img_path_)) {
    err_log("can not get modem image path");
    return false;
  }

  modem_fd_ = ::open(ls2cstring(modem_img_path_), O_RDONLY);

  if (-1 == modem_fd_) {
    err_log("open MODEM image failed");
    return false;
  }

  int modem_img_ret = get_modem_img(modem_fd_,
                                    new_sha1_,
                                    lib_offset_,
                                    lib_len_);

  if (modem_img_ret) {  // Get parsing lib sha1, offset and length failed
    ::close(modem_fd_);
    modem_fd_ = -1;
    err_log("get MODEM image info error");
    return false;
  }

  return true;
}

bool ModemParseLibrarySave::check_parse_lib() {
  bool trigger = false;
  bool same_lib_len = false;
  bool same_sha1 = false;
  std::shared_ptr<LogFile> lib;
  std::shared_ptr<LogFile> sha1;

  for (auto lf : dest_dir_->log_files()) {
    if (LogFile::LT_MODEM_PARSE_LIB == lf->type()) {
      if (LogString("modem_db.gz") == lf->base_name()) {
        lib = lf;
        if (lib_len_ == lf->size()) {
          same_lib_len = true;
        }
      } else if (LogString("sha1.txt") == lf->base_name()) {
        sha1 = lf;
        int sha1_fd = ::open(ls2cstring(lf->dir()->path() +
                                        "/" +
                                        lf->base_name()),
                             O_RDONLY);
        if (sha1_fd >= 0) {
          uint8_t old_sha1_content[40];
          ssize_t read_len = read(sha1_fd,
                                  old_sha1_content,
                                  sizeof old_sha1_content);
          ::close(sha1_fd);

          if (40 == read_len &&
              !memcmp(old_sha1_content, new_sha1_, 40)) {
            same_sha1 = true;
          }
        }
      }

      if (lib && sha1) {
        break;
      }
    }
  }

  if (!same_sha1 || !same_lib_len) {
    if (lib) {
      dest_dir_->remove(lib);
    }
    if (sha1) {
      dest_dir_->remove(sha1);
    }

    trigger = true;
    info_log("trigger modem parse library");
  }

  return trigger;
}

int ModemParseLibrarySave::execute() {
  if (false == init()) {
    on_finished(TRANS_R_FAILURE);
    return TRANS_E_SUCCESS;
  }

  if (false == check_parse_lib()) {
    on_finished(TRANS_R_SUCCESS);
    return TRANS_E_SUCCESS;
  }

  LogFile* lib = new LogFile(ls2cstring("modem_db.gz"), dest_dir_,
                             LogFile::LT_MODEM_PARSE_LIB, 0, false);

  if (lib->create()) {
    err_log("fail to create modem_db.gz");
    delete lib;
    on_finished(TRANS_R_FAILURE);
    return TRANS_E_SUCCESS;
  }

  LogFile* sha1 = new LogFile("sha1.txt", dest_dir_,
                              LogFile::LT_MODEM_PARSE_LIB, 0, false);

  if (sha1->create()) {
    err_log("fail to create sha1.txt");
    delete sha1;
    lib->remove(lib->dir()->path());
    delete lib;
    on_finished(TRANS_R_FAILURE);
    return TRANS_E_SUCCESS;
  }

  LogVector<std::unique_ptr<ConveyUnitBase>> units;
  units.push_back(
      std::unique_ptr<ConveyUnitBase>{
          new CopyToLogFile(&(wan_modem_->stor_mgr()),
                            wan_modem_->type(),
                            dest_dir_->cp_set_dir()->cp_class(),
                            ls2cstring(modem_img_path_),
                            LogFile::LT_MODEM_PARSE_LIB,
                            lib_len_,
                            modem_fd_,
                            "modem_db.gz",
                            lib,
                            dest_dir_->cp_set_dir()->priority(),
                            lib_offset_)}
                  );
  units.push_back(
      std::unique_ptr<ConveyUnitBase>{
          new WriteToLogFile(&(wan_modem_->stor_mgr()),
                             wan_modem_->type(),
                             dest_dir_->cp_set_dir()->cp_class(),
                             new_sha1_,
                             40,
                             LogFile::LT_MODEM_PARSE_LIB,
                             dest_dir_->cp_set_dir()->priority(),
                             "sha1.txt",
                             sha1)}
                  );

  add_units(units);

  return TransLogConvey::execute();
}

int ModemParseLibrarySave::get_image_path(CpType ct,
                                          LogString& img_path) {
  char path_prefix[PROPERTY_VALUE_MAX];

  property_get(PRODUCT_PARTITIONPATH, path_prefix, "");
  if (!path_prefix[0]) {
    err_log("no %s property",PRODUCT_PARTITIONPATH);
    return -1;
  }

  char prop[32];
  const char* prop_suffix;

  switch (ct) {
    case CT_WCDMA:
    case CT_TD:
    case CT_3MODE:
    case CT_4MODE:
    default:  // CT_5MODE
      prop_suffix = "nvp";
      break;
  }
  snprintf(prop, sizeof prop, "%s%s", PERSIST_MODEM_CHAR, prop_suffix);

  char path_mid[PROPERTY_VALUE_MAX];

  property_get(prop, path_mid, "");
  if (!path_mid[0]) {
    err_log("no %s property", prop);
    return -1;
  }

  img_path = path_prefix;
  img_path += path_mid;
  img_path += "modem";

  return 0;
}

int ModemParseLibrarySave::get_modem_img(int img_fd, uint8_t* new_sha1,
                                         size_t& lib_offset, size_t& lib_len) {

#ifdef SECURE_BOOT_ENABLE
  if (SECURE_BOOT_SIZE != lseek(img_fd, SECURE_BOOT_SIZE, SEEK_SET)) {
    err_log("lseek secure boot fail.");
    return -1;
  }
#endif

  // There are two headers at least
  uint8_t buf[24];
  ssize_t n = read(img_fd, buf, 24);

  if (24 != n) {
    err_log("MODEM image length less than two headers");
    return -1;
  }

  if (memcmp(buf, "SCI1", 4)) {
    err_log("MODEM image not SCI1 format");
    return -1;
  }

  bool found = false;

  while (true) {
    if (2 == buf[12]) {
      if (buf[13] & 4) {
        found = true;
      } else {
        err_log("MODEM image has no SHA-1 checksum");
      }
      break;
    }
    if (buf[13] & 1) {
      break;
    }
    n = read(img_fd, buf + 12, 12);
    if (12 != n) {
      err_log("can not read more headers %d", static_cast<int>(n));
      break;
    }
  }

  if (found) {
    // Offset
    uint32_t offset = buf[16];

    offset += (static_cast<uint32_t>(buf[17]) << 8);
    offset += (static_cast<uint32_t>(buf[18]) << 16);
    offset += (static_cast<uint32_t>(buf[19]) << 24);
    lib_offset = offset + 20;
#ifdef SECURE_BOOT_ENABLE
    lib_offset += SECURE_BOOT_SIZE;
#endif

    // Length
    uint32_t val = buf[20];
    val += (static_cast<uint32_t>(buf[21]) << 8);
    val += (static_cast<uint32_t>(buf[22]) << 16);
    val += (static_cast<uint32_t>(buf[23]) << 24);
    lib_len = val - 20;

    // Get SHA-1 checksum
    if (static_cast<off_t>(offset) == lseek(img_fd, offset, SEEK_SET)) {
      uint8_t sha1[20];

      n = read(img_fd, sha1, 20);
      if (20 != n) {
        found = false;
        err_log("read SHA-1 error %d", static_cast<int>(n));
      } else {
        data2HexString(new_sha1, sha1, 20);
      }
    } else {
      err_log("lseek MODEM image error");
    }
  }

  return found ? 0 : -1;
}
