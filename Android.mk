LOCAL_PATH:= $(call my-dir)

ifeq ($(TARGET_ARCH_ABI), armeabi)
CERBERO_ABI:= android_arm
endif

ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
CERBERO_ABI:= android_armv7
endif

ifeq ($(TARGET_ARCH_ABI), x86)
CERBERO_ABI:= android_x86
endif

include $(CLEAR_VARS)
LOCAL_MODULE := gnutls
LOCAL_SRC_FILES := $(HOME)/cerbero/dist/$(CERBERO_ABI)/lib/libgnutls.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := nettle
LOCAL_SRC_FILES := $(HOME)/cerbero/dist/$(CERBERO_ABI)/lib/libnettle.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := hogweed
LOCAL_SRC_FILES := $(HOME)/cerbero/dist/$(CERBERO_ABI)/lib/libhogweed.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := gmp
LOCAL_SRC_FILES := $(HOME)/cerbero/dist/$(CERBERO_ABI)/lib/libgmp.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := teapotnet
LOCAL_CFLAGS    := -DSQLITE_ENABLE_FTS3 -DSQLITE_ENABLE_FTS3_PARENTHESIS
LOCAL_SRC_FILES := $(wildcard $(LOCAL_PATH)/pla/*.cpp) $(wildcard $(LOCAL_PATH)/tpn/*.cpp) include/sqlite3.c
LOCAL_C_INCLUDES := $(HOME)/cerbero/dist/$(CERBERO_ABI)/include
LOCAL_LDLIBS    := -llog
LOCAL_SHARED_LIBRARIES := gnutls nettle hogweed gmp
include $(BUILD_SHARED_LIBRARY)
