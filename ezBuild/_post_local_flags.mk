#-----------------------------------------------------------------------

$(LOCAL_NAME): $(__LOCAL_DST_FILE_$(LOCAL_ID))

all: $(__LOCAL_DST_FILE_$(LOCAL_ID))

$(LOCAL_NAME)_clean: __LOCAL_CLEAN_$(LOCAL_ID)

clean: __LOCAL_CLEAN_$(LOCAL_ID)

#-----------------------------------------------------------------------

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
-include $(__LOCAL_DEPS_$(LOCAL_ID))
endif
endif
