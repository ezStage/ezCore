
ifneq ($(LOCAL_ID),0)
$(error you need include CLEAR_LOCAL_VARS)
endif

LOCAL_ID := $(words $(__LOCAL_IDS))
__LOCAL_IDS += $(LOCAL_ID)

#只需要在首次编译前做一次
ifeq ($(LOCAL_ID),1)
STATIC_DEBUG := $(if $(STATIC_DEBUG),$(STATIC_DEBUG),$(GLOBAL_DEBUG))
CC := $(GLOBAL_CROSS)gcc
CXX := $(GLOBAL_CROSS)g++
LD := $(GLOBAL_CROSS)ld
AR := $(GLOBAL_CROSS)ar
RANLIB := $(GLOBAL_CROSS)ranlib
STRIP := $(GLOBAL_CROSS)strip
endif

ifeq ($(LOCAL_NAME),)
$(error LOCAL_NAME is NULL)
endif

#LOCAL_NAME中不能有.号
ifneq ($(filter %.%, $(LOCAL_NAME)),)
$(error LOCAL_NAME \"$(LOCAL_NAME)\" contains . )
endif

#如果LOCAL_NAME_SUFFIX不为空, 必须以.号开始
ifneq ($(LOCAL_NAME_SUFFIX),)
ifeq ($(filter .%, $(LOCAL_NAME_SUFFIX)),)
$(error "LOCAL_NAME_SUFFIX \"$(LOCAL_NAME_SUFFIX)\" is not beginwith . ")
endif
endif

__TEMP_CFLAGS := -fno-common -Wall -D_REENTRANT
__TEMP_LDFLAGS := 

LOCAL_DEBUG := $(if $(LOCAL_DEBUG),$(LOCAL_DEBUG),$(STATIC_DEBUG))
ifeq ($(LOCAL_DEBUG),1)
__TEMP_CFLAGS += -DEZ_DEBUG=1 -O0 -ggdb3
#__TEMP_CFLAGS += -pg -fprofile-arcs -ftest-coverage
#__TEMP_LDFLAGS += -lgcov 
else
__TEMP_CFLAGS += -DNDEBUG -O2 -fomit-frame-pointer
endif

LOCAL_CFLAGS_$(LOCAL_ID) := $(__TEMP_CFLAGS)
LOCAL_LDFLAGS_$(LOCAL_ID) := $(__TEMP_LDFLAGS)

#----------------------------------------------------------------
#LOCAL_SRC_FILES中的绝对路径不变, 相对路径加上curdir前缀变成绝对路径
__TEMP_SRCS := $(foreach f,$(LOCAL_SRC_FILES),$(if $(filter /%,$(f)),$(f),$(STATIC_SRC_DIR)/$(f)))
__TEMP_SRCS := $(abspath $(__TEMP_SRCS))

#init中排除了_build是top的前缀，但是top可能是_build的前缀,
#src中不以_build为前缀的路径,用patsubst把前缀top改为_build
LOCAL_SRC_FILES := $(foreach f,$(__TEMP_SRCS),$(if $(filter $(STATIC_BUILD_PATH)/%,$(f)),$(f),$(patsubst $(STATIC_TOP_PATH)/%,$(STATIC_BUILD_PATH)/%,$(f))))

#判断最后所有的srcs文件都应该实在_build目录下, 否则出错
__TEMP_SRCS := $(filter_out $(STATIC_BUILD_PATH)/%,$(LOCAL_SRC_FILES))
ifneq ($(__TEMP_SRCS),)
$(error src files is out of top $(__TEMP_SRCS))
endif

#sort会去除重复目录, 创建_build目录
$(foreach f,$(sort $(dir $(LOCAL_SRC_FILES))),$(shell mkdir -p $(f)))

#----------------------------------------------------------------

LOCAL_NAME_$(LOCAL_ID) := $(LOCAL_NAME)

__TEMP_OBJS := $(patsubst %.c, %.o, $(LOCAL_SRC_FILES))
__TEMP_OBJS := $(patsubst %.cc, %.ox, $(__TEMP_OBJS))
__TEMP_OBJS := $(patsubst %.cpp, %.oxx, $(__TEMP_OBJS))
__LOCAL_OBJS_$(LOCAL_ID) := $(__TEMP_OBJS) $(LOCAL_EXTRA_OBJS)

__LOCAL_DEPS_$(LOCAL_ID) := $(addsuffix .Po, $(__LOCAL_OBJS_$(LOCAL_ID)))

__LOCAL_EXTRA_DEPS_$(LOCAL_ID) := $(LOCAL_EXTRA_DEPS)
