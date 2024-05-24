
__NO_STD_INSTALL:=

ifeq ($(EZ_PKG_NAME),)
#EZ_PKG_NAME为空, 没有std_install
__NO_STD_INSTALL:=1
endif

#--------------------------------------------------------------------
#如果EZ_PKG_PATH不为空, 必须是绝对路径, 以'/'开始
EZ_PKG_PATH := $(strip $(EZ_PKG_PATH))
ifneq ($(EZ_PKG_PATH),)

ifneq ($(shell if [ -d "$(EZ_PKG_PATH)" ]; then echo true; fi),true)
#$(error "EZ_PKG_PATH directory $(EZ_PKG_PATH) does not exist")
__NO_STD_INSTALL:=1
endif

ifeq ($(filter /%, $(EZ_PKG_PATH)),)
#$(error "EZ_PKG_PATH directory $(EZ_PKG_PATH) is not abs path")
__NO_STD_INSTALL:=1
endif

#如果pkg_path以'/'结尾, 去掉结尾的'/'
EZ_PKG_PATH := $(patsubst %/, %, $(EZ_PKG_PATH))

else
#EZ_PKG_PATH为空, 没有std_install
__NO_STD_INSTALL:=1
endif

#--------------------------------------------------------------------
ifeq ($(__NO_STD_INSTALL),)

#pkg_path不用realpath替换, 保持用户原样的设置
_PKG_PATH := $(EZ_PKG_PATH)/$(EZ_PKG_NAME)

std_install_pkg: all
	rm -rf $(_PKG_PATH) && mkdir -p $(_PKG_PATH)
	[ ! -d $(STATIC_BUILD_PATH)/lib ] || cp -au $(STATIC_BUILD_PATH)/lib $(_PKG_PATH)/
	[ ! -d $(STATIC_BUILD_PATH)/bin ] || cp -au $(STATIC_BUILD_PATH)/bin $(_PKG_PATH)/
	[ ! -d $(STATIC_BUILD_PATH)/local ] || cp -au $(STATIC_BUILD_PATH)/local $(_PKG_PATH)/
	[ ! -d $(STATIC_BUILD_PATH)/doc ] || cp -au $(STATIC_BUILD_PATH)/doc $(_PKG_PATH)/
	[ ! -d $(STATIC_BUILD_PATH)/include ] || cp -au $(STATIC_BUILD_PATH)/include $(_PKG_PATH)/
	[ ! -f $(STATIC_BUILD_PATH)/config.mk ] || cp -au $(STATIC_BUILD_PATH)/config.mk $(_PKG_PATH)/
	[ ! -d $(STATIC_TOP_PATH)/var ] || cp -au $(STATIC_TOP_PATH)/var $(_PKG_PATH)/
	[ ! -d $(STATIC_TOP_PATH)/doc ] || cp -au $(STATIC_TOP_PATH)/doc $(_PKG_PATH)/
	[ ! -d $(STATIC_TOP_PATH)/include ] || cp -au $(STATIC_TOP_PATH)/include $(_PKG_PATH)/
	@echo -e '\e[32m' ========= std_install_pkg $(STATIC_BUILD_PATH) to $(_PKG_PATH) end ========= '\e[0m'

std_remove_pkg:
	rm -rf $(_PKG_PATH)

else

std_install_pkg:
	@echo -e '\e[33;1m' not set EZ_PKG_NAME or EZ_PKG_PATH no exist, no std_install_pkg. '\e[0m'

std_remove_pkg:
	@echo -e '\e[33;1m' not set EZ_PKG_NAME or EZ_PKG_PATH no exist, no std_remove_pkg. '\e[0m'

endif

