/*
 *  cp_log_cmn.cpp - Common functions for the CP log and dump program.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-2-16 Zhang Ziyi
 *  Initial version.
 */

#include <ctime>
#ifdef HOST_TEST_
  #include "prop_test.h"
#else
  #include <cutils/properties.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "cp_log_cmn.h"
#include "def_config.h"
#include "parse_utils.h"

int get_timezone_diff(time_t tnow) {
  struct tm lt;

  if (!localtime_r(&tnow, &lt)) {
    return 0;
  }

  char tzs[16];
  if (!strftime(tzs, sizeof tzs, "%z", &lt)) {
    return 0;
  }

  // The time zone diff is in ISO 8601 format, i.e. one of:
  //   +|-hhmm
  //   +|-hh:mm
  //   +|-hh
  int sign;
  if ('+' == tzs[0]) {
    sign = 1;
  } else if ('-' == tzs[0]) {
    sign = -1;
  } else {
    return 0;
  }

  unsigned n;
  int toffset;
  if (parse_number(reinterpret_cast<uint8_t*>(tzs + 1), 2, n)) {
    return 0;
  }
  toffset = 3600 * static_cast<int>(n);

  if (!tzs[3]) {
    return sign * toffset;
  }

  // Skip the ':' if one exists.
  const uint8_t* m{reinterpret_cast<uint8_t*>(tzs + 3)};
  if (':' == tzs[3]) {
    ++m;
  }
  if (parse_number(m, 2, n)) {
    return 0;
  }

  toffset += (60 * static_cast<int>(n));
  return toffset * sign;
}

int copy_file_seg(int src_fd, int dest_fd, size_t m) {
  int err = 0;
  static const size_t FILE_COPY_BUF_SIZE = (1024 * 32);
  static uint8_t s_copy_buf[FILE_COPY_BUF_SIZE];
  size_t cum = 0;
  size_t left_len = m;

  while (cum < m) {
    size_t to_wr;
    left_len = m - cum;

    if (left_len > FILE_COPY_BUF_SIZE) {
      to_wr = FILE_COPY_BUF_SIZE;
    } else {
      to_wr = left_len;
    }

    ssize_t n = read(src_fd, s_copy_buf, to_wr);

    if (-1 == n) {
      err = -1;
      break;
    }
    if (!n) {  // End of file
      break;
    }
    cum += to_wr;

    n = write(dest_fd, s_copy_buf, to_wr);
    if (-1 == n || static_cast<size_t>(n) != to_wr) {
      err = -1;
      break;
    }
  }

  return err;
}

int copy_file(int src_fd, int dest_fd) {
  int err = 0;
  static const size_t FILE_COPY_BUF_SIZE = (1024 * 32);
  static uint8_t s_copy_buf[FILE_COPY_BUF_SIZE];

  while (true) {
    ssize_t n = read(src_fd, s_copy_buf, FILE_COPY_BUF_SIZE);

    if (-1 == n) {
      err = -1;
      break;
    }
    if (!n) {  // End of file
      break;
    }
    size_t to_wr = n;
    n = write(dest_fd, s_copy_buf, to_wr);
    if (-1 == n || static_cast<size_t>(n) != to_wr) {
      err = -1;
      break;
    }
  }

  return err;
}

int copy_file(const char* src, const char* dest) {
  // Open the source and the destination file
  int src_fd;
  int dest_fd;

  src_fd = open(src, O_RDONLY);
  if (-1 == src_fd) {
    err_log("open source file %s failed", src);
    return -1;
  }

  dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC,
                 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (-1 == dest_fd) {
    close(src_fd);
    err_log("open dest file %s failed", dest);
    return -1;
  }

  int err = copy_file(src_fd, dest_fd);

  close(dest_fd);
  close(src_fd);

  return err;
}

int set_nonblock(int fd) {
  long flag = fcntl(fd, F_GETFL);
  int ret = -1;

  if (flag != -1) {
    flag |= O_NONBLOCK;
    ret = fcntl(fd, F_SETFL, flag);
    if (ret < 0) {
      return -1;
    }
    ret = 0;
  }

  return ret;
}

void data2HexString(uint8_t* out_buf, const uint8_t* data, size_t len) {
  static uint8_t s_hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                              '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  int n = 0;
  for (size_t i = 0; i < len; ++i, n += 2) {
    out_buf[n] = s_hex[data[i] >> 4];
    out_buf[n + 1] = s_hex[data[i] & 0xf];
  }
}
int get_sys_gsi_flag(char* value) {
  property_get("ro.product.device", value, "");
  return 0;
}


int get_sys_run_mode(SystemRunMode& rm) {
  char mode_val[PROPERTY_VALUE_MAX];
  bool is_unknown{};

  property_get("ro.bootmode", mode_val, "");
  if (!strcmp(mode_val, "factorytest")) {
    rm = SRM_FACTORY_TEST;
  } else if (!strcmp(mode_val, "recovery")) {
    rm = SRM_RECOVERY;
  } else if (!strcmp(mode_val, "cali")) {
    rm = SRM_CALIBRATION;
  } else if (!strcmp(mode_val, "charger")) {
    rm = SRM_CHARGER;
  } else {
    is_unknown = true;
  }

  if (!is_unknown) {
    return 0;
  }

  // parse /proc/cmdline to determine if
  // bootmode is calibration, autotest or normal
  FILE* pf = fopen("/proc/cmdline", "r");
  if (!pf) {
    err_log("can't open /proc/cmdline");
    return -1;
  }

  int ret{};
  uint8_t cmdline[LINUX_KERNEL_CMDLINE_SIZE]{};

  if (fgets(reinterpret_cast<char*>(cmdline), LINUX_KERNEL_CMDLINE_SIZE, pf)) {
    rm = SRM_NORMAL;
    size_t tok_len;
    const uint8_t* mode_str = cmdline;
    const uint8_t* tok;

    while ((tok = get_token(mode_str, tok_len))) {
      if ((tok_len > 17) &&
          (!memcmp(tok, "androidboot.mode=", 17))) {
        if ((8 == tok_len - 17) &&
                   (!memcmp(tok + 17, "autotest", 8))) {
          rm = SRM_AUTOTEST;
        }
        break;
      }

      mode_str = tok + tok_len;
    }
  } else if (ferror(pf)) {
    ret = -1;
    err_log("error when reading /proc/cmdline");
  } else {
    rm = SRM_NORMAL;
  }

  fclose(pf);

  return ret;
}

int get_calendar_time(timeval& tnow, CalendarTime& ct) {
  int ret = gettimeofday(&tnow, nullptr);

  if (-1 == ret) {
    return -1;
  }

  struct tm local_tm;

  ret = -1;

  if (localtime_r(&tnow.tv_sec, &local_tm)) {
    ct.year = local_tm.tm_year + 1900;
    ct.mon = local_tm.tm_mon + 1;
    ct.day = local_tm.tm_mday;
    ct.hour = local_tm.tm_hour;
    ct.min = local_tm.tm_min;
    ct.sec = local_tm.tm_sec;
    ct.msec = static_cast<int>((tnow.tv_usec + 500) / 1000);
    ct.lt_diff = get_timezone_diff(tnow.tv_sec);

    ret = 0;
  }

  return ret;
}

int operator - (const timeval& t1, const timeval& t2) {
  time_t sdiff = t1.tv_sec - t2.tv_sec;
  int diff;

  if (t1.tv_usec >= t2.tv_usec) {
    diff = static_cast<int>((t1.tv_usec - t2.tv_usec) / 1000 + sdiff * 1000);
  } else {
    --sdiff;
    diff = static_cast<int>((t1.tv_usec + 1000000 - t2.tv_usec) / 1000 + sdiff * 1000);
  }

  return diff;
}

bool is_data_mounted() {
  return 0 == access(WORK_CONF_DIR, W_OK | X_OK);
}

int is_vendor_version() {
#ifdef VENDOR_VERSION_
    return 1;
#else
    return 0;
#endif
}


int read_number_prop(const char* prop, unsigned& num) {
  char val[PROPERTY_VALUE_MAX];

  property_get(prop, val, "");
  if (!val[0]) {
    return -1;
  }

  // Skip all spaces in val
  const uint8_t* tok;
  size_t len;

  tok = get_token(reinterpret_cast<uint8_t*>(val), len);
  if (!tok) {
    return -1;
  }

  int ret = parse_number(tok, len, num);
  if (!ret) {
    tok += len;
    if (*tok) {
      tok = get_token(tok, len);
      if (tok) {
        ret = -1;
      }
    }
  }

  return ret;
}
