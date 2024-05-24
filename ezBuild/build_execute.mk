
$(shell mkdir -p $(STATIC_BUILD_PATH)/bin)

include $(EZ_USR_PATH)/install/ezBuild/_prev_local_flags.mk
#-----------------------------------------------------------------------

ifneq ($(filter %.ox %.oxx, $(__LOCAL_OBJS_$(LOCAL_ID))),)
LINK_CC_$(LOCAL_ID)=$(CXX)
else
LINK_CC_$(LOCAL_ID)=$(CC)
endif

LOCAL_LDFLAGS_$(LOCAL_ID) += -Wl,--allow-shlib-undefined -Wl,--no-as-needed

ifeq ($(GLOBAL_EXE_PIE),1)
LOCAL_CFLAGS_$(LOCAL_ID) += -fPIE
LOCAL_LDFLAGS_$(LOCAL_ID) += -pie -Wl,-Bsymbolic -Wl,-Bsymbolic-functions
endif

LOCAL_CFLAGS_$(LOCAL_ID) += $(LOCAL_CFLAGS) $(STATIC_CFLAGS) $(__DEFAULT_CFLAGS) $(GLOBAL_CFLAGS)
LOCAL_LDFLAGS_$(LOCAL_ID) += $(LOCAL_LDFLAGS) $(STATIC_LDFLAGS) $(__DEFAULT_LDFLAGS) $(EZ_PKG_DEP_LIBS) $(GLOBAL_LDFLAGS)

__LOCAL_DST_FILE_$(LOCAL_ID) := $(STATIC_BUILD_PATH)/bin/$(LOCAL_NAME)$(LOCAL_NAME_SUFFIX)

$(__LOCAL_DST_FILE_$(LOCAL_ID)): LOCAL_ID := $(LOCAL_ID)

$(__LOCAL_DST_FILE_$(LOCAL_ID)): $(__LOCAL_EXTRA_DEPS_$(LOCAL_ID)) $(__LOCAL_OBJS_$(LOCAL_ID))
	$(LINK_CC_$(LOCAL_ID)) $(__LOCAL_OBJS_$(LOCAL_ID)) $(LOCAL_LDFLAGS_$(LOCAL_ID)) -o $@
	[ "$(LOCAL_DEBUG)" = "1" ] || $(STRIP) $@
	@echo -e '\e[32m' ========= _build $@ end ========= '\e[0m'

__LOCAL_CLEAN_$(LOCAL_ID): LOCAL_ID := $(LOCAL_ID)

__LOCAL_CLEAN_$(LOCAL_ID):
	-rm -f $(__LOCAL_OBJS_$(LOCAL_ID)) $(__LOCAL_DEPS_$(LOCAL_ID))
	-rm -f $(__LOCAL_DST_FILE_$(LOCAL_ID))

#-----------------------------------------------------------------------
include $(EZ_USR_PATH)/install/ezBuild/_post_local_flags.mk
