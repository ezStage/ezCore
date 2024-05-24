#ifndef _EZ_CPU_H_
#define _EZ_CPU_H_

#include <stdint.h>
/*----------------------
int8_t		uint8_t 
int16_t		uint16_t
int32_t		uint32_t
int64_t		uint64_t
intptr_t	uintptr_t

INT8_MAX
UINT8_MAX

INT16_MAX
UINT16_MAX

INT32_MAX
UINT32_MAX

INTPTR_MAX
INTPTR_MIN
------------------------*/

#if defined(EZ_CPU_X86)
#define EZ_INTP_BYTE	4
#elif defined(EZ_CPU_X64)
#define EZ_INTP_BYTE	8
#elif defined(EZ_CPU_ARM)
#define EZ_INTP_BYTE	4
#elif defined(EZ_CPU_ARM64)
#define EZ_INTP_BYTE	8
#else
#error "you are not define any cpu"
#endif

#if EZ_INTP_BYTE != (__WORDSIZE/8)
#error "EZ_INTP_BYTE != (__WORDSIZE/8)"
#endif

typedef unsigned int uint_t;

/*================================================*/
/*浮点数目前总是用double*/

/*================================================*/
/*下面是字节序*/

#include <byteswap.h>
/* 该文件定义了: __bswap_16(),	__bswap_32(),	__bswap_64() */
/*下面是如果没有<byteswap.h>, 自己的定义
static inline uint16_t __bswap16(uint16_t x)
{
	return ((x<<8)&0xff00)|((x>>8)&0xff);
}

static inline uint32_t __bswap32(uint32_t x)
{
	return ((x<<24)&0xff000000)|
		((x<<8)&0xff0000)|
		((x>>8)&0xff00)|
		((x>>24)&0xff);
}

static inline uint64_t __bswap64(uint64_t x)
{
        return ((x<<56)&0xff00000000000000)|
		((x<<40)&0xff000000000000)|
		((x<<24)&0xff0000000000)|
		((x<<8)&0xff00000000)|
		((x>>8)&0xff000000)|
		((x>>24)&0xff0000)|
		((x>>40)&0xff00)|
		((x>>56)&0xff);
}
*/

#include <endian.h>
/*该文件中定义了 __LITTLE_ENDIAN 1234, __BIG_ENDIAN 4321,
__BYTE_ORDER 是 __LITTLE_ENDIAN 或 __BIG_ENDIAN.
并定义下面几个转换宏:
htole16(), le16toh(),
htole32(), le32toh(),
htole64(), le64toh(),

htobe16(), be16toh(),
htobe32(), be32toh(),
htobe64(), be64toh(),
*/

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define EZ_ENDIAN_LITTLE	1
#elif __BYTE_ORDER == __BIG_ENDIAN
#define EZ_ENDIAN_BIG	1
#else
#error "cpu not specify endian!"
#endif

#endif

