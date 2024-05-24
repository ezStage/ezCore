#ifndef _EZ_OS_H_
#define _EZ_OS_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/shm.h>
#include <sys/wait.h>

/*信号处理*/
#include <signal.h>

/*********************************************************
 * 定义多线程的相关信息
 *********************************************************/
#include <pthread.h>

#define EZ_THREAD_ID		pthread_t
#define EZ_THREAD_ID_SELF	pthread_self()
#define EZ_THREAD_ID_NONE	0

/*===================================*/

#define EZ_MUTEX	pthread_mutex_t

static inline void ez_mutex_init(EZ_MUTEX *mp) {
	pthread_mutexattr_t mattr;
	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE_NP);
	pthread_mutex_init(mp, &mattr);
	pthread_mutexattr_destroy(&mattr);
}

static inline void ez_mutex_destroy(EZ_MUTEX *mp) {
	pthread_mutex_destroy(mp);
}

#define EZ_MUTEX_INIT_VALUE	PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP

#define EZ_MUTEX_LOCK(mutex_p) pthread_mutex_lock(mutex_p)
#define EZ_MUTEX_UNLOCK(mutex_p) pthread_mutex_unlock(mutex_p)

#endif

