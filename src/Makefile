EZ_USR_PATH:=../_build
STATIC_TOP_PATH:=..
#=============================================
include $(EZ_USR_PATH)/install/ezBuild/init.mk
#=============================================

LOCAL_NAME := libezCore
LOCAL_CFLAGS := -D_GNU_SOURCE -Werror -Wno-unused-result
LOCAL_LDFLAGS := -Wl,--version-script -Wl,symbol_export
LOCAL_LIB_VERSION:=0.9

LOCAL_SRC_FILES += base/arg.c
LOCAL_SRC_FILES += base/runtime.c

ifeq ($(EZ_OS_LINUX),1)
LOCAL_SRC_FILES += platform/os_linux.c
endif

LOCAL_SRC_FILES += base/btree.c
LOCAL_SRC_FILES += base/map.c
LOCAL_SRC_FILES += base/dict.c
LOCAL_SRC_FILES += base/ezml.c
LOCAL_SRC_FILES += base/vector.c
LOCAL_SRC_FILES += base/string.c
LOCAL_SRC_FILES += utils/utils.c

include $(BUILD_SHARE_LIBRARY)
