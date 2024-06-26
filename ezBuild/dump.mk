
define __DUMP
$(warning $(1)=$($(1)))
endef

$(call __DUMP,EZ_USR_PATH)
$(call __DUMP,EZ_PKG_PATH)
$(call __DUMP,EZ_PKG_NAME)
$(call __DUMP,EZ_PKG_DEP_LIBS)

$(call __DUMP,GLOBAL_CROSS)
$(call __DUMP,GLOBAL_CFLAGS)
$(call __DUMP,GLOBAL_CXXFLAGS)
$(call __DUMP,GLOBAL_LDFLAGS)
$(call __DUMP,GLOBAL_DEBUG)
$(call __DUMP,GLOBAL_EXE_PIE)

$(call __DUMP,STATIC_TOP_PATH)
$(call __DUMP,STATIC_BUILD_PATH)
$(call __DUMP,STATIC_CFLAGS)
$(call __DUMP,STATIC_LDFLAGS)
$(call __DUMP,STATIC_DEBUG)
$(call __DUMP,STATIC_SRC_DIR)
$(call __DUMP,STATIC_DST_DIR)

$(call __DUMP,LOCAL_NAME)
$(call __DUMP,LOCAL_NAME_SUFFIX)
$(call __DUMP,LOCAL_SRC_FILES)
$(call __DUMP,LOCAL_CFLAGS)
$(call __DUMP,LOCAL_LDFLAGS)
$(call __DUMP,LOCAL_DEBUG)
$(call __DUMP,LOCAL_LIB_VERSION)
$(call __DUMP,LOCAL_LIB_UNDEFINED)
$(call __DUMP,LOCAL_EXTRA_OBJS)
$(call __DUMP,LOCAL_EXTRA_DEPS)

$(call __DUMP,LOCAL_ID)
$(call __DUMP,LOCAL_NAME_$(LOCAL_ID))
$(call __DUMP,LOCAL_CFLAGS_$(LOCAL_ID))
$(call __DUMP,LOCAL_LDFLAGS_$(LOCAL_ID))
$(call __DUMP,__LOCAL_OBJS_$(LOCAL_ID))
$(call __DUMP,__LOCAL_DEPS_$(LOCAL_ID))
