LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := teapotnet
LOCAL_CFLAGS    := -Wall -DSQLITE_ENABLE_FTS3 -DSQLITE_ENABLE_FTS3_PARENTHESIS
FILE_LIST := $(wildcard $(LOCAL_PATH)/tpn/*.cpp)
LOCAL_SRC_FILES := $(FILE_LIST:$(LOCAL_PATH)/tpn/%=tpn/%) include/sqlite3.c
LOCAL_LDLIBS    := -llog

include $(BUILD_SHARED_LIBRARY)

