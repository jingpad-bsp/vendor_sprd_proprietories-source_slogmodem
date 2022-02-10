Configuration options for slogmodem:

1. Make file variables.

The following make file variables can be defined in project make files.

SPRD_CP_LOG_AGDSP
    If this variable is true, then AGDSP log is supported.
    If this variable is not true, or it's not defined, AGDSP log is not
    supported.

SPRD_CP_LOG_WCN
    The WCN type. Supported types:
        NONE
            No WCN.
        TSHARK
            WCN integrated in TShark.
        MARLIN
            MARLIN
        MARLIN2
            MARLIN2

SPRD_LOG_MODEM_TALIGN
    If this variable is AT_CMD, then atcmd will be used for getting modem time.
    If NONE or it's not defined,refnotify will be used for getting modem time

2. Macros

The following macros can be defined in Android.mk.

INT_STORAGE_PATH
    The internal storage path. This can only be /data or /storage/emulated/0.

EXT_STORAGE_PATH
    The external storage path. If this macro is not defined, slogmodem will
    get the external storage path from /proc/mounts.

POWER_ON_PERIOD
    The time span of the poweron phase, in unit of second.

3. Command line arguments

slogmodem has no command line arguments.
