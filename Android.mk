# CP log
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_INIT_RC := slogmodem.rc
LOCAL_MODULE := slogmodem
LOCAL_MODULE_TAGS := optional

LOCAL_CPPFLAGS += -fexceptions

LOCAL_CFLAGS += -DINIT_CONF_DIR=\"/vendor/etc/\"

LOCAL_SRC_FILES := client_hdl.cpp \
                   client_hdl_miniap.cpp \
                   client_hdl_mipilog.cpp \
                   client_mgr.cpp \
                   client_req.cpp \
                   convey_unit.cpp \
                   convey_unit_base.cpp \
                   convey_workshop.cpp \
                   copy_dir_to_dir.cpp \
                   copy_to_log_file.cpp \
                   cp_convey_mgr.cpp \
                   cp_dir.cpp \
                   cp_dump.cpp \
                   cp_log_cmn.cpp \
                   cp_log.cpp \
                   cp_set_dir.cpp \
                   cp_stat_hdl.cpp \
                   cp_stor.cpp \
                   data_buf.cpp \
                   data_proc_hdl.cpp \
                   dev_file_hdl.cpp \
                   dev_file_open.cpp \
                   diag_dev_hdl.cpp \
                   diag_stream_parser.cpp \
                   evt_notifier.cpp \
                   ext_gnss_log.cpp \
                   ext_wcn_dump.cpp \
                   fd_hdl.cpp \
                   file_watcher.cpp \
                   io_chan.cpp \
                   io_sched.cpp \
                   log_config.cpp \
                   log_config_with_modemtype.cpp \
                   log_ctrl.cpp \
                   log_ctrl_miniap.cpp \
                   log_ctrl_mipilog.cpp \
                   log_file.cpp \
                   log_pipe_dev.cpp \
                   log_pipe_hdl.cpp \
                   major_minor_num_a6.cpp \
                   media_stor.cpp \
                   media_stor_check.cpp \
                   modem_at_ctrl.cpp \
                   modem_cmd_ctrl.cpp \
                   modem_parse_library_save.cpp \
                   modem_stat_hdl.cpp \
                   move_dir_to_dir.cpp \
                   multiplexer.cpp \
                   orca_dp_log.cpp \
                   orca_miniap_log.cpp \
                   parse_utils.cpp \
                   pm_modem_dump.cpp \
                   pm_sensorhub_log.cpp \
                   rw_buffer.cpp \
                   stor_mgr.cpp \
                   timer_mgr.cpp \
                   trans.cpp \
                   trans_diag.cpp \
                   trans_disable_log.cpp \
                   trans_dump_rcv.cpp \
                   trans_enable_log.cpp \
                   trans_ext_gnss_col.cpp \
                   trans_global.cpp \
                   trans_modem.cpp \
                   trans_last_log.cpp \
                   trans_log_col.cpp \
                   trans_log_convey.cpp \
                   trans_mgr.cpp \
                   trans_modem_col.cpp \
                   trans_modem_ver.cpp \
                   trans_normal_log.cpp \
                   trans_orca_etb.cpp \
                   trans_pmsh_col.cpp \
                   trans_save_evt_log.cpp \
                   trans_save_sleep_log.cpp \
                   trans_start_evt_log.cpp \
                   trans_stop_cellular_log.cpp \
                   trans_stop_wcn_log.cpp \
                   trans_wcn_last_log.cpp \
                   uevent_monitor.cpp \
                   wan_modem_log.cpp \
                   wan_modem_time_sync.cpp \
                   wcdma_iq_mgr.cpp \
                   wcdma_iq_notifier.cpp \
                   wcnd_stat_hdl.cpp \
                   write_to_log_file.cpp \
                   wan_modem_time_sync_refnotify.cpp \
                   wan_modem_time_sync_atcmd.cpp \
                   trans_modem_time.cpp

ifeq ($(strip $(SPRD_CP_LOG_WCN)), TSHARK)
    LOCAL_SRC_FILES += int_wcn_log.cpp \
                       trans_int_wcn_col.cpp
    LOCAL_CFLAGS += -DSUPPORT_WCN \
                    -DWCN_ASSERT_MESSAGE=\"WCN-CP2-EXCEPTION\" \
                    -DWCN_STAT_SOCK_NAME=\"wcnd\"
else ifeq ($(strip $(SPRD_CP_LOG_WCN)), MARLIN)
    LOCAL_SRC_FILES += ext_wcn_log.cpp \
                       trans_ext_wcn_col.cpp
    LOCAL_CFLAGS += -DSUPPORT_WCN \
                    -DEXTERNAL_WCN \
                    -DWCN_ASSERT_MESSAGE=\"WCN-EXTERNAL-DUMP\" \
                    -DWCN_STAT_SOCK_NAME=\"external_wcn_slog\"
else ifeq ($(strip $(SPRD_CP_LOG_WCN)), MARLIN2)
    LOCAL_SRC_FILES += ext_wcn_log.cpp \
                       trans_ext_wcn_col.cpp
    LOCAL_CFLAGS += -DSUPPORT_WCN \
                    -DEXTERNAL_WCN \
                    -DWCN_ASSERT_MESSAGE=\"WCN-CP2-EXCEPTION\" \
                    -DWCN_STAT_SOCK_NAME=\"wcnd\"
endif

ifeq ($(strip $(SPRD_CP_LOG_AGDSP)), true)
    LOCAL_SRC_FILES += agdsp_dump.cpp \
                       agdsp_log.cpp \
                       agdsp_pcm.cpp \
                       client_hdl_agdsp.cpp \
                       dump_stor_notifier.cpp \
                       log_config_agdsp.cpp \
                       log_ctrl_agdsp.cpp \
                       trans_agdsp_col.cpp
    LOCAL_CFLAGS += -DSUPPORT_AGDSP
endif

ifeq ($(strip $(SPRD_LOG_MODEM_TALIGN)), AT_CMD)
    LOCAL_CFLAGS += -DMODEM_TALIGN_AT_CMD_
endif

ifeq ($(strip $(BOARD_SECURE_BOOT_ENABLE)), true)
    LOCAL_CFLAGS += -DSECURE_BOOT_ENABLE
endif

ifeq ($(strip $(USE_SPRD_ORCA_MODEM)), true)
    LOCAL_CFLAGS += -DUSE_ORCA_MODEM
    LOCAL_SRC_FILES += modem_utils.cpp \
                       trans_orca_dump.cpp
endif

LOCAL_SHARED_LIBRARIES := libc \
                          libc++ \
                          libcutils \
                          liblog
LOCAL_CFLAGS += -DLOG_TAG=\"SLOGCP\" -DUSE_STD_CPP_LIB_ \
                -DETB_FILE_PATH=\"/dev/tmc_etb\" \
                -D_REENTRANT

# slogmodem run time MACROs
# that need to be defined according to requirements:

# 1. INT_STORAGE_PATH: Specify the preferred internal storage path.
#                      If not defined, slogmodem will use /data as
#                      the internal storage by default.

LOCAL_CFLAGS += -DINT_STORAGE_PATH=\"/storage/emulated/0\" \
                -DWORK_CONF_DIR=\"/data/local/slogmodem/\"


# 2. EXT_STORAGE_PATH: Specify the mount point of the external SD.
#                      If not defined, slogmodem will use the first
#                      vfat or exfat file system in /proc/mounts.
#    LOCAL_CFILES += -DEXT_STORAGE_PATH=\"path of external storage\"

# 3. POWER_ON_PERIOD: Specify a period in seconds since boot up,
#                     during which the log directory is under the
#                     power on directory. When the time period
#                     is reached, switch to the normal directory
LOCAL_CFLAGS += -DPOWER_ON_PERIOD=120u
LOCAL_CFLAGS += -DSUPPORT_RATE_STATISTIC
LOCAL_CPPFLAGS += -std=c++11
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := flush_slog_modem
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES := utility/flush_slog_modem.cpp
LOCAL_SHARED_LIBRARIES := libc \
                          libc++ \
                          libcutils \
                          liblog
LOCAL_CFLAGS += -DLOG_TAG=\"CPLOG_FLUSH\" -DUSE_STD_CPP_LIB_
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := cplogctl
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES := client_req.cpp \
                   parse_utils.cpp \
                   utility/clear_req.cpp \
                   utility/collect_req.cpp \
                   utility/cplogctl.cpp \
                   utility/cplogctl_cmn.cpp \
                   utility/en_evt_req.cpp \
                   utility/flush_req.cpp \
                   utility/get_cp_max_size.cpp \
                   utility/get_log_file_size.cpp \
                   utility/get_mipi_log.cpp \
                   utility/get_preferred_storage.cpp \
                   utility/last_log_req.cpp \
                   utility/on_off_req.cpp \
                   utility/overwrite_req.cpp \
                   utility/query_state_req.cpp \
                   utility/save_last_log_req.cpp \
                   utility/set_ag_log_dest.cpp \
                   utility/set_ag_pcm.cpp \
                   utility/set_cp_max_size.cpp \
                   utility/set_log_file_size.cpp \
                   utility/set_mini_ap.cpp \
                   utility/set_mipi_log.cpp \
                   utility/set_orca_dp.cpp \
                   utility/set_preferred_storage.cpp \
                   utility/slogm_req.cpp \
                   utility/start_evt_req.cpp
LOCAL_SHARED_LIBRARIES := libc \
                          libcutils \
                          liblog \
                          libutils
LOCAL_CFLAGS += -DLOG_TAG=\"CPLOG_CTL\"
include $(BUILD_EXECUTABLE)

CUSTOM_MODULES += slogmodem flush_slog_modem cplogctl
include $(call all-makefiles-under,$(LOCAL_PATH))
