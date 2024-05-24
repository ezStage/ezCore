#ifndef _EZ_COMPILE_H
#define _EZ_COMPILE_H

/**********************************************************/
//定义编译器的相关宏

#if defined(__cplusplus) || defined(c_plusplus)
#define EZ_EXTERN_C_BEGIN extern "C" {
#define EZ_EXTERN_C_END }
#else
#define EZ_EXTERN_C_BEGIN
#define EZ_EXTERN_C_END
#endif

#define EZ_ATTR_ALIGN(N)	__attribute__((aligned(N)))

#define EZ_MEMBER_OFFSET(type, member)			__builtin_offsetof(type, member)
#define EZ_FROM_MEMBER(type, member, mp)	((type *)((char *)(mp) - EZ_MEMBER_OFFSET(type, member)))

#define EZ_PREFETCH(addr)	__builtin_prefetch(addr)
#define EZ_PREFETCHW(addr)	__builtin_prefetch(addr,1)

#define EZ_ABS(a)	({ typeof(a) _a=(a); _a < 0 ? -_a : _a; })

#define EZ_MIN(a,b)	({ typeof(a) _a=(a); typeof(b) _b=(b); _a < _b ? _a : _b; })
#define EZ_MAX(a,b)	({ typeof(a) _a=(a); typeof(b) _b=(b); _a > _b ? _a : _b; })

#define EZ_SWAP(a,b)	do { typeof(a) _tmp=a; a=b; b=_tmp; } while(0)

#define EZ_GET_LOWBIT(x)		((x) & -(x))
#define EZ_CLEAR_LOWBIT(x)		((x) & ((x)-1))
#define EZ_IS_POW2(x)			((x) && !((x) & ((x)-1))) /*注意 0 返回 false*/

#define EZ_ROUND_DOWN(n, r) \
({ \
	_Static_assert(EZ_IS_POW2(r)); \
	((n) & -(r)); \
})

#define EZ_ROUND_UP(n, r) \
({ \
	_Static_assert(EZ_IS_POW2(r)); \
	typeof(n) _n = (n)+(r)-1; \
	(_n & -(r)); \
})

#define EZ_ROUND(n, r) \
({ \
	_Static_assert(EZ_IS_POW2(r)); \
	typeof(n) _n = (n)+((r)>>1); \
	(_n & -(r)); \
})

#endif

