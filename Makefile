#---------------------------------------------
EZ_USR_PATH ?= /work/ezStage/usr
EZ_PKG_PATH ?= /work/ezStage/pkg_x64
PLAT ?= x64-linux-gcc
#---------------------------------------------

#ezCore比较特殊，因为这里是配置环境的开始，
#并且不能使用用户设置的配置环境EZ_USR_PATH，
#先make config创建自己的配置环境(./_build)
#并且还要把自己的配置环境模拟成usr
BAK_USR_PATH:=$(EZ_USR_PATH)
EZ_USR_PATH:=./_build

#_build目录既是ezCore的pkg目录, 又是模拟的usr目录
#config改变后, 所有的_build需要重建, 先distclean
ifeq ($(MAKECMDGOALS),config)
$(shell rm -rf ./_build* ; mkdir -p ./_build/install)
$(shell ln -sf ../../ezBuild ./_build/install/ezBuild)
$(shell ln -sf .. ./_build/install/ezCore)
endif

ifeq ($(MAKECMDGOALS),install)
$(shell mkdir -p $(EZ_PKG_PATH))
endif

STATIC_TOP_PATH:=.
#===========================================================
include $(EZ_USR_PATH)/install/ezBuild/init.mk
#===========================================================

CONFIG_MK := $(STATIC_BUILD_PATH)/config.mk
CONFIG_H := $(STATIC_BUILD_PATH)/include/ezCore/ezCore_config.h

#前面的命令包含了distclean
config:
	cp -a config/$(PLAT).mk $(CONFIG_MK)
	$(BUILD_CONFIG_H) $(CONFIG_MK) $(CONFIG_H)

all:
	make -C src
	make -C demo

clean:
	make -C src clean
	make -C demo clean

distclean:
	rm -rf _build*

install: std_install_pkg
	rm -rf $(EZ_PKG_PATH)/ezBuild ; cp -a ezBuild $(EZ_PKG_PATH)/ezBuild
	cp -a config/make_usr.sh $(EZ_PKG_PATH)/make_usr.sh
	$(EZ_PKG_PATH)/make_usr.sh $(BAK_USR_PATH)

remove: std_remove_pkg
	$(USR_REMOVE) $(BAK_USR_PATH) $(EZ_PKG_PATH)/$(EZ_PKG_NAME) -force_var

