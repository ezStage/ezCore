EZ_USR_PATH:=../_build
STATIC_TOP_PATH:=..
STATIC_BUILD_PATH:=../_build_demo
#==============================================
include $(EZ_USR_PATH)/install/ezBuild/init.mk
#==============================================
include $(EZ_USR_PATH)/install/ezCore/config.mk
#==============================================

STATIC_CFLAGS := -I../include
STATIC_LDFLAGS := -Wl,-rpath,"$(EZ_USR_PATH)/lib"

LOCAL_NAME := 00-list
LOCAL_SRC_FILES := 00-list.c
include $(BUILD_EXECUTE)

#--------------------------------------------
include $(CLEAR_LOCAL_VARS)
#--------------------------------------------

LOCAL_NAME := 01-mem
LOCAL_SRC_FILES := 01-mem.c
include $(BUILD_EXECUTE)

#--------------------------------------------
include $(CLEAR_LOCAL_VARS)
#--------------------------------------------

LOCAL_NAME := 02-string
LOCAL_SRC_FILES := 02-string.c
include $(BUILD_EXECUTE)

#--------------------------------------------
include $(CLEAR_LOCAL_VARS)
#--------------------------------------------

LOCAL_NAME := 03-map
LOCAL_SRC_FILES := 03-map.c
include $(BUILD_EXECUTE)

#--------------------------------------------
include $(CLEAR_LOCAL_VARS)
#--------------------------------------------

LOCAL_NAME := 04-task
LOCAL_SRC_FILES := 04-task.c
include $(BUILD_EXECUTE)

#--------------------------------------------
include $(CLEAR_LOCAL_VARS)
#--------------------------------------------

LOCAL_NAME := 05-arg
LOCAL_SRC_FILES := 05-arg.c
include $(BUILD_EXECUTE)

#--------------------------------------------
include $(CLEAR_LOCAL_VARS)
#--------------------------------------------

LOCAL_NAME := 06-dict
LOCAL_SRC_FILES := 06-dict.c
include $(BUILD_EXECUTE)

#--------------------------------------------
include $(CLEAR_LOCAL_VARS)
#--------------------------------------------

LOCAL_NAME := 07-ezml
LOCAL_SRC_FILES := 07-ezml.c
include $(BUILD_EXECUTE)

all:
	cp -a example.ezml $(STATIC_BUILD_PATH)/bin/

