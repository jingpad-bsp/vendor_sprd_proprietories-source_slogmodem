LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_INIT_RC := modemlog_connmgr_service.rc
LOCAL_MODULE := modemlog_connmgr_service
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libnetutils \
    liblog \
    libutils \
    libhidlbase \
    libsysutils \
    vendor.sprd.hardware.cplog_connmgr@1.0 \
    libhidltransport

LOCAL_SRC_FILES := service.cpp \
                   ConnectControlCallback.cpp \
                   hidl_server.cpp \
                   modem_state_hidl_server.cpp \
                   modem_time_sync_hidl_server.cpp \
                   wcn_state_hidl_server.cpp

LOCAL_CFLAGS += -DLOG_TAG=\"CPLOG_CONNMGR\"
include $(BUILD_EXECUTABLE)
CUSTOM_MODULES += modemlog_connmgr_service
