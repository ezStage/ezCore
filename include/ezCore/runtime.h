#ifndef	_EZ_RUNTIME_H
#define	_EZ_RUNTIME_H

/*******************************************************************************/
/* 运行时包括 一个内存垃圾回收(GC)系统 + 一个任务(Task)系统 + 信号处理(Signal) */
/*******************************************************************************/

typedef void (*ezGCDestroyFunc)(void *p);
typedef struct {
	ezGCDestroyFunc destroy_func;
	const char *name;
} ezGC_type;

/*分配一个GC资源，即一段可以自动回收的内存. clear表示内存是否清0*/
void * ezAlloc(intptr_t size, const ezGC_type *type, ezBool clear);

/*用户主动释放GC资源，只能释放 local 或 global 的资源*/
void ezDestroy(void *p);

/*用于模块内部主动释放一个 slave 资源*/
ezResult ezGC_SlaveDestroy(void *p, const ezGC_type *owner_type);

/*把一个 local 资源变成 global 资源*/
ezResult ezGC_Local2Global(void *p);

/*把一个 local 资源变成 slave 资源*/
ezResult ezGC_Local2Slave(void *p, void *owner);

/*把一个 slave 资源变成 local 资源*/
ezResult ezGC_Slave2Local(void *p, const ezGC_type *owner_type);

/*返回GC资源指针, 匹配GC资源类型*/
void * ezGC_check_type(const void *p, const ezGC_type *type);

ezBool ezGC_check_owner(const void *p, const void *owner);

void * ezGC_get_owner(const void *p, const ezGC_type *owner_type);

ezBool ezGC_is_destroy(const void *p);

/*不明白不要用*/
typedef struct EZ_ATTR_ALIGN(EZ_INTP_BYTE*2) {
	const ezGC_type *type;
	uintptr_t flags;
	uintptr_t p[0]; //保证p的对齐是uintptr_t大小的两倍
} ezGC_sub;

ezResult ezGC_set_sub(void *p, ezGC_sub *sub, const ezGC_type *sub_type);

#define ezGC_destroy_sub(sub) \
do { \
	if((sub).type && (sub).type->destroy_func) \
		(sub).type->destroy_func((sub).p); \
} while(0)

void ezGC_exit(int status); /*释放所有gc资源, 然后exit(status)*/

uintptr_t ezGCPoint_insert(void);
void ezGCPoint_clean(uintptr_t level);

/*##############################################################*/
/*
如果是在不同的线程中访问和释放GC，应该由用户通过互斥锁
来保证不会出现释放后访问。

但是ezFunc是一个例外, 因为函数在回调前要解锁,
即使回调函数开始就再加锁也会有一个短暂的时间间隙,
可能有另一个线程释放该ezFunc.
并且回调函数后可能要访问ezFunc.
所以ezFunc在GC释放时特殊处理!

当用户主动调用ezDestroy或ezGC_SlaveDestroy删除GC时，
由用户自己负责保证不会在释放后访问!
*/
typedef struct ezFunc_st ezFunc;
typedef void (*ezFuncFunc)(ezFunc *p, intptr_t id, void *data);

/*在file task回调函数中获取文件fd和IO类型 */
#define EZ_FUNC_FILE_FD(id)		((int)id)
#define EZ_FUNC_FILE_TYPE(data)	((ezMask8)(uintptr_t)data)

/*在timer task回调函数中获取period*/
#define EZ_FUNC_TIMER_PERIOD(id)	((int)id)

/*返回sub类型*/
void * ezFunc_create(int size, const ezGC_type *sub_type, ezFuncFunc func, void *arg);
/*返回ezFunc是否被destroy*/
ezBool ezFunc_call(void *p, intptr_t id, void *data, EZ_MUTEX *mutex_p);
void * ezFunc_get_arg(const void *p);
//void * ezFunc_get_obj(const void *p);
//void ezFunc_set_obj(void *p, void *obj);

ezBool ezFunc_check_equal(const void *p, ezFuncFunc func, void *arg);

/*##############################################################*/

#define EZ_TASK_STOP		0x0	/*任务停止*/
#define EZ_TASK_IDLE		0x1	/*任务没有满足运行条件, 等待*/
#define EZ_TASK_READY		0x2	/*任务满足运行条件, 就绪*/
#define EZ_TASK_RUN			0x3	/*任务正在运行*/
#define EZ_TASK_DESTROY		0x4	/*任务销毁*/

/*参考 EPOLLIN/PRI/OUT/ERR/HUP 定义*/
#define EZ_FILE_IN		0x01	/*文件有数据可读*/
#define EZ_FILE_OUT		0x02	/*文件输出缓冲器有空闲可写*/
#define EZ_FILE_PRI		0x04	/*网络文件有带外数据(紧急数据)可读*/
#define EZ_FILE_ERR		0x08	/*文件出错*/
#define EZ_FILE_HUP		0x10	/*网络文件断开*/
#define EZ_FILE_MASK	0x1f

typedef struct ezTask_st ezTask;

uint8_t ezTask_get_status(ezTask * task);

/*file/timer task创建时是stop状态, 需要用户start进入idle状态*/
ezTask * ezTask_add_file(const char *group, int fd, ezFuncFunc func, void *arg);
ezTask * ezTask_add_timer(const char *group, ezFuncFunc func, void *arg);
/*single task创建就是idle状态, 没有start/stop. 用户主动触发进入ready状态*/
ezTask * ezTask_add_single(const char *group, ezFuncFunc func, void *arg);

/*如果已经start了, 再次调用start可以修改type.*/
void ezTaskFile_start(ezTask *file, ezMask8 type);
void ezTaskFile_stop(ezTask *file);
int ezTaskFile_get_fd(ezTask *file);

/*如果已经start了, 再次调用start有重置时间的作用.
first_delay > 0, 如果first_delay<=0表示立即执行;
period > 0, 如果period<=0表示只执行first就stop */
void ezTaskTimer_start(ezTask *timer, ezTime first_delay, ezTime period);
void ezTaskTimer_stop(ezTask *timer);

/*data可以为NULL, 不为NULL时必须是GC资源, 自动成为single task的slave*/
/*触发一次，执行一次, 可以多次触发*/
ezResult ezTaskSingle_trigger(ezTask *single, intptr_t id, void *data);
int ezTaskSingle_get_ReadyNum(ezTask *single);

/*创建一个任务组, 同组的task保证在同一个线程中执行。
flags等于1表示创建一个线程来执行该组任务。
flags等于0表示在主线程中执行该组任务。
目前对于任务组，没有其他控制*/
ezResult ezTask_create_groud(const char *group, uint32_t flags);

/*进入事件循环，该函数不退出*/
void ezTask_run(void);

/*该函数是和其他的事件循环系统配合使用的,
不阻塞的检测是否有task ready, 并执行完所有就绪任务*/
void ezTask_try_run(void);

/*====================================================*/

typedef struct ezCond_t ezCond;

ezCond * ezTaskCond_create(void);
void ezTaskCond_wait(ezCond *pcond, EZ_MUTEX *pmutex, ezTime timeout); /*timeout: >0 ; <=0 forever*/
void ezTaskCond_waken(ezCond *pcond, ezBool broadcast);
/*
ezCond_wait必须在ezTask_run的loop中运行.
	ezCondWait在该loop上运行等待,
	即等待过程中仍然会检测并运行该loop的task(已经运行的task不会重入),

ezCond总是和一个user_mutex搭配用, 即ezCond内部没有锁
*/

/*====================================================*/

/*Linux的信号处理函数可能在任意时刻进入, 会导致所有的函数都有可能重入,
所以操作系统规定了在信号处理函数中只能调用很少的信号安全的函数.
我们这里封装了signal, 转换成在main线程中和上面的task串行调用,
并且singal_handle本身也不会重入, 和普通的task回调函数一样*/
typedef void (*ezSignalFunc)(int sig, void *arg);
void ezSignal(int sig, ezSignalFunc handler, void *arg);
/*handler为NULL, 等价于signal(,SIG_IGN); 为-1, 等价于signal(,SIG_DFL)*/

typedef void (*ezChildSignalFunc)(pid_t pid, int status, void *arg);
void ezSignal_child(pid_t pid, ezChildSignalFunc handler, void *arg);

#endif

