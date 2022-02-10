LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := mlogservice
LOCAL_INIT_RC := mlogservice.rc
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

LOCAL_SRC_FILES := client_hdl.c \
                   config.c \
                   cp_logctl.c \
                   misc.c \
                   parse_at.c \
                   proc_cmd.c \
                   service_main.c \
                   socket_hdl.c

LOCAL_SHARED_LIBRARIES := libc \
                          libcutils \
                          liblog

ifeq ($(LOCAL_PROPRIETARY_MODULE),true)
       LOCAL_CFLAGS += -DINIT_CONF_DIR=\"/vendor/etc/\"
else
       LOCAL_CFLAGS += -DINIT_CONF_DIR=\"/system/etc/\"
endif

LOCAL_CFLAGS += -DWORK_CONF_DIR=\"/data/vendor/local/mlogservice/\"
LOCAL_CFLAGS += -DAT_CHANNEL=\"/dev/stty_nr27\"
LOCAL_CFLAGS += -DLOG_TAG=\"MODEMLSV\"
include $(BUILD_EXECUTABLE)
CUSTOM_MODULES += mlogservice
