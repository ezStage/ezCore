#include <ezCore/ezCore.h>

/* str==NULL或len<=0, 返回0*/
uintptr_t ez_get_hash_len(const char *str, intptr_t len)
{
	uintptr_t h = 5381;
	uint8_t ch;
	if((len > 0) && str)
	{
		const char *end = str+len;
		do {
			ch = (uint8_t)(*str++);
			h += (h << 5) + ch;
		} while(str < end);

		return h;
	}
	return 0;
}

/* str==NULL或"\0", 返回0. rlen返回字符串长度strlen(str)*/
uintptr_t ez_get_hash_rlen(const char *str, intptr_t *rlen)
{
	const uint8_t *s = (const uint8_t *)str;
	uintptr_t h = 5381;
	uint8_t ch;
	if(s && (ch=*s))
	{
		do {
			h += (h << 5) + ch;
		} while((ch = *++s));
			
		if(rlen) *rlen = s - (const uint8_t *)str;
		return h;
	}
	if(rlen) *rlen = 0;
	return 0;
}

ssize_t ez_read(int fd, void *buf, ssize_t count)
{
	uint8_t *p=(uint8_t *)buf;
	while(count > 0)
	{
		ssize_t n = read(fd, p, count);
		if(n > 0) {
			p += n;
			count -= n;
			continue;
		}
		if((n == 0) || /*n < 0*/
			((errno != EINTR) &&
			(errno != EWOULDBLOCK) &&
			(errno != EAGAIN)))
		{
			break;
		}
	}
	return (p - (uint8_t *)buf);
}

ssize_t ez_write(int fd, const void *buf, ssize_t count)
{
	const uint8_t *p=(const uint8_t *)buf;
	while(count > 0)
	{
		ssize_t n = write(fd, p, count);
		if(n > 0) {
			p += n;
			count -= n;
			continue;
		}
		if((n == 0) || /*n < 0*/
			((errno != EINTR) &&
			(errno != EWOULDBLOCK) &&
			(errno != EAGAIN)))
		{
			break;
		}
	}
	return (p - (uint8_t *)buf);
}

/**********************************************************/
#define MY_PATH_MAX	2000
extern ezString *os_get_app_dir(void);

static ezString *app_dir; //包括结尾的'/'
static ezString * _get_app_dir(void)
{
	if(app_dir == NULL) {
		app_dir = os_get_app_dir();
		ezGC_Local2Global(app_dir);
	}
	return app_dir;
}

ezString *ez_get_app_path(const char *sub_path) {
	return ezString_append(_get_app_dir(), sub_path);
}

/*----------------------------------------------------*/

static ezString *app_pkg_name;
static ezString *_get_app_pkg_name(void)
{
 	if(app_pkg_name) return app_pkg_name;
 
	int32_t n, s, e;

	const char *p;
	ezString *ins, *real;

	_get_app_dir();
	/*app_dir是app所在目录的realpath,
	如果该app是在安装到/usr中的某个pkg目录中运行时,
	那么app_dir中的某个目录就是pkg_name,
	并且realpath("/usr/install/pkg_name")就是
	app_dir的pkg_name目录path*/

	const char *usr_dir = getenv("EZ_USR_PATH");
	if(usr_dir && (*usr_dir == '/'))
	{
		e = ezStrLen(app_dir)-1; //e指向结尾的'/'
		for(; e>0; e=s)
		{
			s = ezString_rindex(app_dir, e, '/'); //搜索前一个'/'
			if(s <= 0) break; //没找到或直接是根目录下的目录

			//如果pkg_name中含有., 只取.前面的内容为真正的name
			//和usr安装规则一致
			p = ezStr(app_dir);
			for(n=s+1; n<e; ++n) {
				if(p[n] == '.') break;
			} //p[n]等于'.'或'/', 结束位置
			if((n -= s+1) <= 0) continue;

			ins = ezString_printf("%s/install/%.*s", usr_dir, (int)n, p+s+1);
			real = ez_realpath(ezStr(ins));
			ezDestroy(ins);
			if(real == NULL) continue;

			if(ezString_beginwith(app_dir, ezStr(real))
				&& (ezStrLen(real) == e))
			{
				app_pkg_name = ezString_create(p+s+1, n);
				ezGC_Local2Global(app_dir);
				ezDestroy(real);
				return app_pkg_name;
			}
			ezDestroy(real);
		}
		ezLog_err("app is not install in usr\n");
	}
	else
	{
		if(usr_dir == NULL) {
			ezLog_err("not set EZ_USR_PATH\n");
		} else {
			ezLog_err("EZ_USR_PATH \"%s\" is not abs path\n", usr_dir);
		}
	}

	//app没有安装到usr中
	app_pkg_name = ezString_create("", 0);
	ezGC_Local2Global(app_pkg_name);
	return app_pkg_name;
}

ezString *ez_get_app_usr_var_path(const char *sub_path)
{
	ezString *pkg_name = _get_app_pkg_name();
	if(ezStrLen(pkg_name) == 0) return NULL;

	const char *usr_dir = getenv("EZ_USR_PATH");
	return ezString_printf("%s/var/%s", usr_dir, ezStr(pkg_name));
}

ezString *ez_get_pkg_usr_dir(const char *pkg_name, const char *dir_name)
{
	const char *usr_dir = getenv("EZ_USR_PATH");
	if(usr_dir && (*usr_dir == '/'))
	{
		ezString *ins = ezString_printf("%s/install/%s", usr_dir, pkg_name);
		ezString *real = ez_realpath(ezStr(ins));
		ezDestroy(ins);
		if(real != NULL) {
			ezDestroy(real);
			return ezString_printf("%s/%s/%s/", usr_dir, dir_name, pkg_name);
		} else {
			ezLog_err("pkg %s in not install in usr\n", pkg_name);
		}
	}
	else
	{
		if(usr_dir == NULL) {
			ezLog_err("not set EZ_USR_PATH\n");
		} else {
			ezLog_err("EZ_USR_PATH \"%s\" is not abs path\n", usr_dir);
		}
	}
	return NULL;
}

