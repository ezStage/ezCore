
MY_PKG_NAME := ezCore
MY_PKG_SUFFIX := .beta
EZ_PKG_DEP_LIBS += -lezCore

#=======================================

GLOBAL_DEBUG := 1
#GLOBAL_CROSS
#GLOBAL_CXXFLAGS

GLOBAL_CFLAGS := -m32
GLOBAL_LDFLAGS := -m32 -lpthread -lc

#=======================================
#在Makefile的config规则中调用$(BUILD_CONFIG_H)把
#下面用eval定义的变量转换成指定C语言头文件中的宏定义.
#注意变量定义中不能含有$符号, 因为转换时不替换变量.

$(eval EZ_CPU_X86 := 1)
#转换成: #define EZ_CPU_X86 1

#$(eval EZ_CPU_X64 := 1)
#$(eval EZ_CPU_ARM := 1)
#$(eval EZ_CPU_ARM64 := 1)
#转换成: /* #undef EZ_CPU_X64 */
#        /* #undef EZ_CPU_ARM */
#        /* #undef EZ_CPU_ARM64 */

$(eval EZ_OS_LINUX := 1)
#$(eval EZ_THREAD_NO := 1)
#=======================================
