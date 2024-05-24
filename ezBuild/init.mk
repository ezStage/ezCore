#----------------------------------------------------------------------
#判断 usr 目录
EZ_USR_PATH := $(strip $(EZ_USR_PATH))
ifeq ($(EZ_USR_PATH),)
$(error "not set EZ_USR_PATH dir")
endif
ifneq ($(shell if [ -d "$(EZ_USR_PATH)" ]; then echo true; fi),true)
$(error "EZ_USR_PATH dir $(EZ_USR_PATH) not exist")
endif
EZ_USR_PATH := $(shell cd $(EZ_USR_PATH) && /bin/pwd)

#处理 top 目录
STATIC_TOP_PATH := $(strip $(STATIC_TOP_PATH))
ifeq ($(STATIC_TOP_PATH),)
$(error "not set STATIC_TOP_PATH dir")
endif
ifneq ($(shell if [ -d "$(STATIC_TOP_PATH)" ]; then echo true; fi),true)
$(error "STATIC_TOP_PATH directory $(STATIC_TOP_PATH) not exist")
endif
STATIC_TOP_PATH := $(shell cd $(STATIC_TOP_PATH) && /bin/pwd)

#处理 _build 目录
STATIC_BUILD_PATH := $(strip $(STATIC_BUILD_PATH))
ifeq ($(STATIC_BUILD_PATH),)
STATIC_BUILD_PATH :=  $(STATIC_TOP_PATH)/_build
endif
$(shell mkdir -p "$(STATIC_BUILD_PATH)")
STATIC_BUILD_PATH := $(shell cd $(STATIC_BUILD_PATH) && /bin/pwd)

#判断:_build不能等于TOP, 也不能是TOP的前缀.
ifneq ($(filter $(STATIC_BUILD_PATH)/%,$(STATIC_TOP_PATH)/),)
$(error top dir $(STATIC_TOP_PATH) is equal or in _build dir $(STATIC_BUILD_PATH))
endif

#获取Makefile所在src目录和对应在build中的dst目录 
STATIC_SRC_DIR := $(shell cd $(CURDIR) && /bin/pwd)
#判断STATIC_SRC_DIR应该在top目录下或者等于top目录.
ifeq ($(filter $(STATIC_TOP_PATH) $(STATIC_TOP_PATH)/%,$(STATIC_SRC_DIR)),)
$(error cur dir $(STATIC_SRC_DIR) is out of top $(STATIC_TOP_PATH))
endif
#用patsubst把前缀top改为_build, 得到src dir对于_build目录下的dst dir
STATIC_DST_DIR := $(patsubst $(STATIC_TOP_PATH)%,$(STATIC_BUILD_PATH)%,$(STATIC_SRC_DIR))

#----------------------------------------------------------------------
#_build目录必须是一个完整的运行环境
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
$(shell if [ -d "$(STATIC_TOP_PATH)/lib" -a ! -e "$(STATIC_BUILD_PATH)/lib" ]; then cp -a "$(STATIC_TOP_PATH)/lib" "$(STATIC_BUILD_PATH)/"; fi)
$(shell if [ -d "$(STATIC_TOP_PATH)/bin" -a ! -e "$(STATIC_BUILD_PATH)/bin" ]; then cp -a "$(STATIC_TOP_PATH)/bin" "$(STATIC_BUILD_PATH)/"; fi)
$(shell if [ -d "$(STATIC_TOP_PATH)/var" -a ! -e "$(STATIC_BUILD_PATH)/var" ]; then cp -a "$(STATIC_TOP_PATH)/var" "$(STATIC_BUILD_PATH)/"; fi)
$(shell if [ -d "$(STATIC_TOP_PATH)/local" -a ! -e "$(STATIC_BUILD_PATH)/local" ]; then cp -a "$(STATIC_TOP_PATH)/local" "$(STATIC_BUILD_PATH)/"; fi)
endif
endif

#----------------------------------------------------------------------
CLEAR_LOCAL_VARS := $(EZ_USR_PATH)/install/ezBuild/clear_local_vars.mk
BUILD_STATIC_LIBRARY := $(EZ_USR_PATH)/install/ezBuild/build_static_library.mk
BUILD_SHARE_LIBRARY := $(EZ_USR_PATH)/install/ezBuild/build_share_library.mk
BUILD_EXECUTE := $(EZ_USR_PATH)/install/ezBuild/build_execute.mk
BUILD_DUMP := $(EZ_USR_PATH)/install/ezBuild/dump.mk
BUILD_CONFIG_H := $(EZ_USR_PATH)/install/ezBuild/build_config_h.sh
USR_INSTALL := $(EZ_USR_PATH)/install/ezBuild/usr_install.sh
USR_REMOVE := $(EZ_USR_PATH)/install/ezBuild/usr_remove.sh

__DEFAULT_CFLAGS := -I$(STATIC_BUILD_PATH)/include -I$(STATIC_TOP_PATH)/include
__DEFAULT_LDFLAGS := -L$(STATIC_BUILD_PATH)/lib -L$(STATIC_TOP_PATH)/lib

ifneq ($(STATIC_TOP_PATH),$(EZ_USR_PATH))
__DEFAULT_CFLAGS += -I$(EZ_USR_PATH)/include
__DEFAULT_LDFLAGS += -L$(EZ_USR_PATH)/lib
endif

__LOCAL_IDS := 0
LOCAL_ID := 0

#======================================================================

.PHONY: FORCE all clean distclean std_distclean config install std_install_pkg remove std_remove_pkg

all:

clean:

distclean: std_distclean

std_distclean:
	-rm -rf $(STATIC_BUILD_PATH)
	@echo -e '\e[32m' ========= std_distclean end ========= '\e[0m'

%.mk: ;

%.Po: ;

%: ;

#======================================================================
#首先包含自己的build/config.mk(如果存在), 获取EZ_PKG_NAME
-include $(STATIC_BUILD_PATH)/config.mk
EZ_PKG_NAME :=
#如果MY_PKG_NAME不为空, 内容中不能有.号
MY_PKG_NAME := $(strip $(MY_PKG_NAME))
ifneq ($(MY_PKG_NAME),)

ifneq ($(filter %.%, $(MY_PKG_NAME)),)
$(error "MY_PKG_NAME \"$(MY_PKG_NAME)\" contains . ")
endif

#如果MY_PKG_SUFFIX不为空, 必须以.号开始
MY_PKG_SUFFIX := $(strip $(MY_PKG_SUFFIX))
ifneq ($(MY_PKG_SUFFIX),)
ifeq ($(filter .%, $(MY_PKG_SUFFIX)),)
$(error "MY_PKG_SUFFIX \"$(MY_PKG_SUFFIX)\" is not beginwith . ")
endif
endif

EZ_PKG_NAME := $(MY_PKG_NAME)$(MY_PKG_SUFFIX)

__DEFAULT_CFLAGS += -DMY_PKG_NAME=\"$(MY_PKG_NAME)\"

endif

#已经包含了自己的config.mk,编译自己时不需要依赖自己.
#所以清除EZ_PKG_DEP_LIBS.
EZ_PKG_DEP_LIBS:=

#在后面包含install.mk, 因为需要 EZ_PKG_NAME
include $(EZ_USR_PATH)/install/ezBuild/install.mk

#======================================================================
# OUT_CFLAGS必须是延迟展开的赋值方式
OUT_CFLAGS = -MMD -MP -MF $@.Po -o $@ $<

$(STATIC_BUILD_PATH)/%.o: $(STATIC_TOP_PATH)/%.c
	$(CC) -c $(LOCAL_CFLAGS_$(LOCAL_ID)) $(OUT_CFLAGS)

$(STATIC_BUILD_PATH)/%.ox: $(STATIC_TOP_PATH)/%.cc
	$(CXX) -c $(LOCAL_CFLAGS_$(LOCAL_ID)) $(GLOBAL_CXXFLAGS) $(OUT_CFLAGS)

$(STATIC_BUILD_PATH)/%.oxx: $(STATIC_TOP_PATH)/%.cpp
	$(CXX) -c $(LOCAL_CFLAGS_$(LOCAL_ID)) $(GLOBAL_CXXFLAGS) $(OUT_CFLAGS)
