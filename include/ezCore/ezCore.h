#ifndef _EZ_CORE_H
#define _EZ_CORE_H

//由编译配置文件ezCore_config.mk生成的头文件
#include <ezCore/ezCore_config.h>

//定义编译器相关宏
#include <ezCore/platform/compile.h>

EZ_EXTERN_C_BEGIN

//包含处理器相关内容
#include <ezCore/platform/cpu.h>

//包含操作系统相关内容
#if defined(EZ_OS_LINUX)
#include <ezCore/platform/os_linux.h>
#else
#error("you are not define any os")
#endif

/***************************************************************
 * 上面都是定义和平台(包括编译器, 处理器, 操作系统)相关的内容,
 * 使得下面ezCore的实现尽量和具体平台无关.
 ***************************************************************/

#include <ezCore/utils.h>
#include <ezCore/list.h>
#include <ezCore/runtime.h>
#include <ezCore/vector.h>
#include <ezCore/string.h>
#include <ezCore/map.h>
#include <ezCore/dict.h>
#include <ezCore/ezml.h>
#include <ezCore/arg.h>

EZ_EXTERN_C_END

#endif

