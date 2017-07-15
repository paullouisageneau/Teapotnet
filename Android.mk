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
LOCAL_SRC_FILES := $(HOME)/cerbero/dist/$(CERBERO_ABI)/lib/libgnutls.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := nettle
LOCAL_SRC_FILES := $(HOME)/cerbero/dist/$(CERBERO_ABI)/lib/libnettle.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := hogweed
LOCAL_SRC_FILES := $(HOME)/cerbero/dist/$(CERBERO_ABI)/lib/libhogweed.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := gmp
LOCAL_SRC_FILES := $(HOME)/cerbero/dist/$(CERBERO_ABI)/lib/libgmp.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := tasn1
LOCAL_SRC_FILES := $(HOME)/cerbero/dist/$(CERBERO_ABI)/lib/libtasn1.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := intl
LOCAL_SRC_FILES := $(HOME)/cerbero/dist/$(CERBERO_ABI)/lib/libintl.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := iconv
LOCAL_SRC_FILES := $(HOME)/cerbero/dist/$(CERBERO_ABI)/lib/libiconv.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := z
LOCAL_SRC_FILES := $(HOME)/cerbero/dist/$(CERBERO_ABI)/lib/libz.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := argon2
LOCAL_SRC_FILES := $(HOME)/src/argon2/libargon2.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := teapotnet
LOCAL_CFLAGS    := -DHAVE_PTHREADS -DSQLITE_ENABLE_FTS3 -DSQLITE_ENABLE_FTS3_PARENTHESIS
LOCAL_SRC_FILES := $(wildcard $(LOCAL_PATH)/pla/*.cpp) $(wildcard $(LOCAL_PATH)/tpn/*.cpp) include/sqlite3.c
LOCAL_C_INCLUDES := $(HOME)/cerbero/dist/$(CERBERO_ABI)/include $(HOME)/src/argon2/include
LOCAL_LDLIBS    := -llog
LOCAL_STATIC_LIBRARIES := gnutls nettle hogweed gmp tasn1 intl iconv z
include $(BUILD_SHARED_LIBRARY)
