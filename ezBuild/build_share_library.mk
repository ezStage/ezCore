
$(shell mkdir -p $(STATIC_BUILD_PATH)/lib)

include $(EZ_USR_PATH)/install/ezBuild/_prev_local_flags.mk
#-----------------------------------------------------------------------

ifneq ($(filter %.ox %.oxx, $(__LOCAL_OBJS_$(LOCAL_ID))),)
LINK_CC_$(LOCAL_ID)=$(CXX)
else
LINK_CC_$(LOCAL_ID)=$(CC)
endif

LOCAL_CFLAGS_$(LOCAL_ID) += -fPIC -DPIC
LOCAL_LDFLAGS_$(LOCAL_ID) += -shared -Wl,-Bsymbolic -Wl,-Bsymbolic-functions

ifneq ($(LOCAL_LIB_UNDEFINED),1)
LOCAL_LDFLAGS_$(LOCAL_ID) += -Wl,--no-undefined
endif

LOCAL_LDFLAGS_$(LOCAL_ID) += -Wl,-soname,$(LOCAL_NAME).so$(LOCAL_NAME_SUFFIX)

ifneq ($(LOCAL_LIB_VERSION),)
__LOCAL_LIB_NAME_$(LOCAL_ID) := $(LOCAL_NAME).so.$(LOCAL_LIB_VERSION)$(LOCAL_NAME_SUFFIX)
__LOCAL_LIB_LINK_$(LOCAL_ID) := $(LOCAL_NAME).so$(LOCAL_NAME_SUFFIX)
else
__LOCAL_LIB_NAME_$(LOCAL_ID) := $(LOCAL_NAME).so$(LOCAL_NAME_SUFFIX)
endif

LOCAL_CFLAGS_$(LOCAL_ID) += $(LOCAL_CFLAGS) $(STATIC_CFLAGS) $(__DEFAULT_CFLAGS) $(GLOBAL_CFLAGS)
LOCAL_LDFLAGS_$(LOCAL_ID) += $(LOCAL_LDFLAGS) $(STATIC_LDFLAGS) $(__DEFAULT_LDFLAGS) $(EZ_PKG_DEP_LIBS) $(GLOBAL_LDFLAGS)

__LOCAL_DST_FILE_$(LOCAL_ID) := $(STATIC_BUILD_PATH)/lib/$(__LOCAL_LIB_NAME_$(LOCAL_ID))

$(__LOCAL_DST_FILE_$(LOCAL_ID)): LOCAL_ID := $(LOCAL_ID)

$(__LOCAL_DST_FILE_$(LOCAL_ID)): $(__LOCAL_EXTRA_DEPS_$(LOCAL_ID)) $(__LOCAL_OBJS_$(LOCAL_ID))
	$(LINK_CC_$(LOCAL_ID)) $(__LOCAL_OBJS_$(LOCAL_ID)) $(LOCAL_LDFLAGS_$(LOCAL_ID)) -o $@
	[ "$(LOCAL_DEBUG)" = "1" ] || $(STRIP) $@
	@[ -z $(__LOCAL_LIB_LINK_$(LOCAL_ID)) ] || ln -sf $(__LOCAL_LIB_NAME_$(LOCAL_ID)) $(STATIC_BUILD_PATH)/lib/$(__LOCAL_LIB_LINK_$(LOCAL_ID))
	@echo -e '\e[32m' ========= _build $@ end ========= '\e[0m'

__LOCAL_CLEAN_$(LOCAL_ID): LOCAL_ID := $(LOCAL_ID)

__LOCAL_CLEAN_$(LOCAL_ID):
	-rm -f $(__LOCAL_OBJS_$(LOCAL_ID)) $(__LOCAL_DEPS_$(LOCAL_ID))
	-cd $(STATIC_BUILD_PATH)/lib ; rm -f $(__LOCAL_LIB_NAME_$(LOCAL_ID)) $(__LOCAL_LIB_LINK_$(LOCAL_ID))

#-----------------------------------------------------------------------
include $(EZ_USR_PATH)/install/ezBuild/_post_local_flags.mk
