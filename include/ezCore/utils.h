#ifndef _EZ_UTILS_H
#define _EZ_UTILS_H

#ifndef NULL
#define NULL (void *)0
#endif

#ifndef None
#define None 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/*boolean是无符号,因为可能用于位域*/
typedef uint8_t	ezBool;

/* bits mask */
typedef uint8_t		ezMask8;
typedef uint16_t	ezMask16;
typedef uint32_t	ezMask32;

/* Four-character-code (FOURCC), 最长4字符常量 */
typedef uint32_t ezFourCC;

/* a -> d: 内存低地址字节 -> 高地址字节 */
#ifdef EZ_ENDIAN_LITTLE
#define EZ_FOURCC(a,b,c,d) (((uint8_t)(a))|(((uint8_t)(b))<<8)|(((uint8_t)(c))<<16)|(((uint8_t)(d))<<24))
#endif

#ifdef EZ_ENDIAN_BIG
#define EZ_FOURCC(a,b,c,d) (((uint8_t)(d))|(((uint8_t)(c))<<8)|(((uint8_t)(b))<<16)|(((uint8_t)(a))<<24))
#endif

/*================================================*/

typedef enum ezResult_e {
	EZ_SUCCESS  = 1,
	EZ_TIMEOUT  = 0, /*timeout, try again*/
	EZ_FAIL     = -1, /*common error*/
	EZ_ENOMEM   = -2, /*no memory*/
	EZ_ENOIMP   = -3, /*not implemented*/
	EZ_ENOPERM = -4, /*Permission denied*/
	EZ_ENOENT   = -5, /*no exist*/
	EZ_ECLASH   = -6, /*already exist, clash*/
	EZ_EINVAL   = -7, /*Invalid argument*/
	EZ_EIO      = -8, /*Input/output error*/
} ezResult; /* int16_t */

#define ezLog_print(format, ...) fprintf(stderr, format, ##__VA_ARGS__)
#define ezLog_warn(format, ...) fprintf(stderr, "\033[1;33mwarn:\033[0m " format, ##__VA_ARGS__)
#define ezLog_err(format, ...) fprintf(stderr, "\033[0;31merror:\033[0m " format, ##__VA_ARGS__)
#define ezLog_errno(format, ...) fprintf(stderr, "\033[0;31merrno=%d(%s):\033[0m " format , errno, strerror(errno), ##__VA_ARGS__);

/*================================================*/

static inline int ez_strcmp(const char *s1, const char *s2) {
	if(s1 == s2) return 0;
	if(s1 == NULL) return -1;
	if(s2 == NULL) return 1;
	return strcmp(s1, s2);
}

static inline const char * ez_strdup(const char *s) {
	return s? strdup(s) : NULL;
}

static inline int ez_strlen(const char *s) {
	return s? strlen(s) : 0;
}

static inline void ez_free(const void *p) {
	if(p) free((void *)p);
}

/*获取以'\0'结束的字符串的hash值, rlen返回字符串的长度*/
uintptr_t ez_get_hash_rlen(const char *str, intptr_t *rlen);

/*获取长度为len的字符串的hash值(len<=0,返回0)*/
uintptr_t ez_get_hash_len(const char *str, intptr_t len);

ssize_t ez_read(int fd, void *buf, ssize_t count);

ssize_t ez_write(int fd, const void *buf, ssize_t count);

typedef struct ezString_st ezString;
ezString * ez_realpath(const char *path);

/*得到app所在文件系统中路径, 并添加相对于该路径的后缀sub_path*/
ezString *ez_get_app_path(const char *sub_path);

/*如果该app所在的pkg已安装到usr中,
返回 "usr/var/pkg_name/sub_path". 否则返回NULL*/
ezString *ez_get_app_usr_var_path(const char *sub_path);

/*如果指定pkg_name的pkg已安装到usr中,
返回 "usr/dir_name/pkg_name". 否则返回NULL*/
ezString *ez_get_pkg_usr_dir(const char *pkg_name, const char *dir_name);

/*================================================*/

/*必须用有符号整数表示时间, 单位是毫秒.
时间点的绝对值没有意义, 相对值才有意义.*/
typedef intptr_t ezTime;

/*判断两个时间点的关系:
EZ_TIME_OFFSET(a, b) 表示从b到a的矢量*/
#define EZ_TIME_OFFSET(a, b) ((intptr_t)((intptr_t)(a) - (intptr_t)(b)))

ezTime ez_get_time(void);/*得到当前时间点, 毫秒单位*/

void ez_sleep(ezTime ms);/*延迟毫秒时间*/

/*================================================*/

typedef struct ezBTree_st {
	uintptr_t key;
	struct ezBTree_st *left, *right;
	int8_t balance; /* left_height - right_height */
	int8_t no_use;
} ezBTree;

static inline void ezBTree_init(ezBTree *node, uintptr_t key)
{
	node->key = key;
	node->left = NULL;
	node->right = NULL;
	node->balance = 0;
}

ezBTree *ezBTree_find(ezBTree *root, uintptr_t key);
ezBool ezBTree_insert(ezBTree **root, ezBTree *node);
ezBool ezBTree_delete(ezBTree **root, ezBTree *node);

/*================================================*/

typedef int (*ezSortCompareFunc)(const void *d1, const void *d2, void *arg);

#endif

