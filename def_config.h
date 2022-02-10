/*
 *  def_config.h - The default parameter configuration.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-2-16 Zhang Ziyi
 *  Initial version.
 */

#ifndef _DEF_CONFIG_H_
#define _DEF_CONFIG_H_

// Config file names in various modes.
static const char* kNormalConfName = "slog_modem.conf";
static const char* kCaliConfName = "slog_modem_cali.conf";
static const char* kFactoryConfName = "slog_modem_factory.conf";
static const char* kAutoTestConfName = "slog_modem_autotest.conf";

#define PARENT_LOG "ylog"
#define CONCURRENCY_CAPACITY 4
#define PRODUCT_PARTITIONPATH "ro.vendor.product.partitionpath"
#define PERSIST_MODEM_CHAR "persist.vendor.modem."

#define MODEM_W_DEVICE_PROPERTY "persist.vendor.modem.w.enable"
#define MODEM_TD_DEVICE_PROPERTY "persist.vendor.modem.t.enable"
#define MODEM_WCN_DEVICE_PROPERTY "ro.vendor.modem.wcn.enable"
#define MODEM_GNSS_DEVICE_PROPERTY "ro.vendor.modem.gnss.enable"
#define MODEM_L_DEVICE_PROPERTY "persist.vendor.modem.l.enable"
#define MODEM_TL_DEVICE_PROPERTY "persist.vendor.modem.tl.enable"
#define MODEM_FL_DEVICE_PROPERTY "persist.vendor.modem.lf.enable"
#define MODEM_TYPE_PROPERTY "ro.vendor.radio.modemtype"

#define MODEM_WCN_LOG_PROPERTY "ro.vendor.modem.wcn.log"
#define MODEM_GNSS_LOG_PROPERTY "ro.vendor.modem.gnss.log"
#define MODEM_LOG_PROPERTY "ro.vendor.modem.log"
#define MODEM_ORCA_AP_LOG_PROPERTY "ro.vendor.modem.miniap.log"
#define MODEM_ORCA_DP_LOG_PROPERTY "ro.vendor.modem.dp.log"

#define AGDSP_LOG_PROPERTY "ro.vendor.ag.log"
#define PM_SENSORHUB_LOG_PROPERTY "ro.vendor.sp.log"

#define MODEM_WCN_DIAG_PROPERTY "ro.vendor.modem.wcn.diag"
#define MODEM_GNSS_DIAG_PROPERTY "ro.vendor.modem.gnss.diag"
#define MODEM_DIAG_PROPERTY "ro.vendor.modem.diag"

#define MINI_AP_KERNEL_LOG_PROP "persist.vendor.miniap.log"

// tty device path property
#define MODEM_TTY_PROPERTY "ro.vendor.modem.tty"

#define MODEM_COMMAND_PROPERTY "ro.vendor.modem.command"

#define MODEM_WCN_DEVICE_RESET "persist.vendor.sys.wcnreset"
#define MODEM_WCN_DUMP_LOG_COMPLETE "persist.vendor.sys.wcnlog.result"

#ifdef VENDOR_VERSION_
#define SLOG_MODEM_SERVER_SOCK_NAME "slogmodem"
#define CP_TIME_SYNC_SERVER_SOCKET "cp_time_sync_server"
#define MODEM_SOCKET_NAME "modemd"
#define WCN_SOCKET_NAME "wcnd"
#else
#define SLOG_MODEM_SERVER_SOCK_NAME "hidl_slogmodem"
#define MODEM_SOCKET_NAME "hidl_modemd"
#define CP_TIME_SYNC_SERVER_SOCKET "hidl_cp_time_sync_server"
#define WCN_SOCKET_NAME "hidl_wcnd"
#endif

#define MODEMRESET_PROPERTY "persist.vendor.sys.modemreset"
#define MODEM_SAVE_DUMP_PROPERTY "persist.vendor.sys.modem.save_dump"

#define MODEM_VERSION "gsm.version.baseband"

#define DATA_PATH "/data/vendor"

#ifdef HOST_TEST_
#define DEBUG_SMSG_PATH "/data/local/tmp/smsg"
#define DEBUG_SBUF_PATH "/data/local/tmp/sbuf"
#define DEBUG_SBLOCK_PATH "/data/local/tmp/sblock"
#define DEBUG_MAILBOX_PATH "/data/local/tmp/mbox"
#else
#define DEBUG_SMSG_PATH "/d/sipc/smsg"
#define DEBUG_SBUF_PATH "/d/sipc/sbuf"
#define DEBUG_SBLOCK_PATH "/d/sipc/sblock"
#define DEBUG_MAILBOX_PATH "/d/sipc/mbox"
#endif

#define MINI_DUMP_LTE_SRC_FILE "/proc/cptl/mini_dump"
#define MINI_DUMP_WCDMA_SRC_FILE "/proc/cpw/mini_dump"
#define MINI_DUMP_TD_SRC_FILE "/proc/cpt/mini_dump"

#define AON_IRAM_FILE "/proc/cptl/aon-iram"
#define PUBCP_IRAM_FILE "/proc/cptl/pubcp-iram"

#define PM_LOG_CTRL "/dev/sctl_pm"

#ifdef USE_ORCA_MODEM
static const char* kPmMemDevPath = "/dev/pmsys";
static const char* kModemDevPath = "/dev/modem";
static const char* kDpDevPath = "/dev/dpsys";
#else
static const char* kPmMemDevPath = "/proc/pmic/mem";
#endif

#ifdef MODEM_LOG_VENDOR_PATH_
#define AUDIO_PARAM_FILE_PATH "/data/vendor/local/media"
#define CP_GROUP_EN "/data/vendor/local/cgroup_en"
#define DATA_PATH "/data/vendor"
#else
#define AUDIO_PARAM_FILE_PATH "/data/local/media"
#define CP_GROUP_EN "/data/local/cgroup_en"
#define DATA_PATH "/data"
#endif

static const int DEFAULT_EXT_LOG_SIZE_LIMIT = (512 * 1024 * 1024);
static const int DEFAULT_INT_LOG_SIZE_LIMIT = (5 * 1024 * 1024);

static const int MODEM_LAST_LOG_PERIOD = 1500;
static const int LOG_FILE_WRITE_BUF_SIZE = (1024 * 64);

static const int SECURE_BOOT_SIZE = 1024;
static const int LINUX_KERNEL_CMDLINE_SIZE  = 1024;

static const int MIX_FILE_MAX_NUM  = 3;

#endif  // !_DEF_CONFIG_H_
