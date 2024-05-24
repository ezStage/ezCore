#include <ezCore/ezCore.h>

void ez_sleep(ezTime ms)
{
	struct timeval tv;
	ezTime to = ms;
	ezTime _end = ez_get_time() + to;
	while(to > 0) {
		tv.tv_sec = (to >> 10);
		tv.tv_usec = ((to & 0x3ff) << 10);
		select(0, NULL, NULL, NULL, &tv);
		to = _end - ez_get_time();
	}
	return;
}

ezTime ez_get_time(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (ts.tv_sec*1000 + ts.tv_nsec/1000000);
}

ezString * ez_realpath(const char *path)
{
	char rpath[PATH_MAX+1];
	if((path == NULL)||(realpath(path, rpath) == NULL)) return NULL;
	rpath[PATH_MAX] = '\0';
	return ezString_create(rpath, -1);
}

/*得到当前运行进程的app所在目录的realpath*/
#define _SMALL_PATH 600
ezString *os_get_app_dir(void)
{
	ssize_t len;
	char rpath[_SMALL_PATH];
	char name[64];

	char *pn = "/proc/self/exe";
	len = readlink(pn, rpath, _SMALL_PATH);
	if(len < 0)
	{/* older kernels don't have /proc/self */
		pn = name;
		sprintf(pn, "/proc/%u/exe", getpid());
		len = readlink(pn, rpath, _SMALL_PATH);
		if(len < 0) {
			ezLog_errno("readlink /proc/xxx/exe\n");
			return NULL;
		}
	}

	if(len < _SMALL_PATH)
	{
		while(--len >= 0)
		{
			if(rpath[len] == '/') { //包括结尾的'/'
				return ezString_create(rpath, len+1);
			}
		}
		return NULL; //不会运行到这
	}

/*---------------------------------------------------------*/
	char *pr = malloc(PATH_MAX);
	if(pr == NULL) {
		ezLog_err("malloc PATH_MAX(%d) fail\n", PATH_MAX);
		return NULL;
	}
	ezString *ret = NULL;
	len = readlink(pn, pr, PATH_MAX);
	if(len < PATH_MAX)
	{
		while(--len >= 0)
		{
			if(pr[len] == '/') { //包括结尾的'/'
				ret = ezString_create(pr, len+1);
				break;
			}
		}
	} else {
		ezLog_err("app dir beyond PATH_MAX(%d)\n", PATH_MAX);
	}

	free(pr);
	return ret;
}

