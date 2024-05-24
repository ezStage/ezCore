#include <ezCore/ezCore.h>

/************************************************************************/
/************************************************************************/

#define _GC_OWNER_LOCAL			((ezGC *)0)
#define _GC_OWNER_GLOBAL		((ezGC *)1)
#define _GC_IS_SLAVE(gc)		((uintptr_t)(gc->owner_gc) > 1)

/*ezGC要尽量小, 注意slave_head是指针.
注意sub的偏移必须满足uintptr_t大小的两倍*/
typedef struct ezGarbageCollection ezGC;
struct ezGarbageCollection {
	ezList list; /*owner_gc->slave_head 或 gc_local_head, gc_destroy_head*/
	ezList *slave_head; /*slave链表头, 双向不循环链表, 节省一个指针*/
	ezGC *owner_gc; /*0表示local,1表示global,其他表示slave*/
	ezGC_sub sub; /*flags=top_offset/level:10bit, sub_offset:8bit, is_sub:1bit, destroy:1bit, sig:12bit*/
};

/*
__sync_add_and_fetch(&(gc->ref_count), 1);
__sync_sub_and_fetch(&(gc->ref_count), 1)
*/
/*top_offset一定要在最高位*/
#define _GC_TOP_OFFSET_BITS 	10
#define _GC_TOP_OFFSET_SHIFT	22
#define _GC_TOP_OFFSET_MASK		((1<<_GC_TOP_OFFSET_BITS)-1)

/*level和top_offset复用*/
#define _GC_LEVEL_BITS		_GC_TOP_OFFSET_BITS
#define _GC_LEVEL_SHIFT		_GC_TOP_OFFSET_SHIFT
#define _GC_LEVEL_MASK		_GC_TOP_OFFSET_MASK

/*sub offset*/
#define _GC_SUB_OFFSET_BITS 	8
#define _GC_SUB_OFFSET_SHIFT	14
#define _GC_SUB_OFFSET_MASK		((1<<_GC_SUB_OFFSET_BITS)-1)

/*sub的flags高位是top_offset, top的flags高位是level*/
#define _GC_IS_SUB_MASK			0x2000

/*be destroyed*/
#define _GC_DESTROY_MASK		0x1000

/*sig一定要在最低位*/
#define _GC_SIG_MASK		0xFFF
#define _GC_SIG				0x5A3

#define _GC_GET_LEVEL(gc) ((gc->sub.flags)>>_GC_LEVEL_SHIFT)
#define _GC_SET_LEVEL(gc, level) \
do { \
	gc->sub.flags = \
		(gc->sub.flags & ~(_GC_LEVEL_MASK<<_GC_LEVEL_SHIFT))|(level<<_GC_LEVEL_SHIFT); \
} while(0)

#define _GC_IS_DESTROY(gc) \
		(gc->sub.flags & _GC_DESTROY_MASK)

#define _GET_GC(gc, sp, ...) \
do { \
	if(sp == NULL) return __VA_ARGS__; \
	uintptr_t flags = *((uintptr_t *)sp - 1); \
	if((flags & _GC_SIG_MASK) != _GC_SIG) { \
		ezLog_err("p is not a GC\n"); \
		return __VA_ARGS__; \
	} \
	if(flags & _GC_IS_SUB_MASK) \
		gc = (ezGC *)((uintptr_t *)sp - (flags>>_GC_TOP_OFFSET_SHIFT)); \
	else \
		gc = EZ_FROM_MEMBER(ezGC, sub.p, sp); \
} while(0)

/*===================================================================*/
struct ezFunc_st {
	//EZ_MUTEX *mutex_p;
	ezFuncFunc func;
	void *arg;
	void *obj;
	ezBool hold; /*回调函数不能重入，所以不需要引用计数*/
	ezBool destroy;
	ezGC_sub sub;
};

static void _gc_func_destroy(void *p)
{
	ezFunc *pf = (ezFunc *)p;
	pf->destroy = TRUE;
	ezGC_destroy_sub(pf->sub);
	return;
}

static ezGC_type EZ_FUNC_TYPE = {
	.destroy_func = _gc_func_destroy,
	.name = "ezFunc",
};

/************************************************************************/
#include "../platform/_os_mux_epoll.h"
/************************************************************************/

/* 最大嵌套运行级别 */
#define EZ_THREAD_RUN_NUM_MAX	128

typedef struct {
	EZ_MUTEX lock;
	ezList timer_idle_head; /*start但没有到期的timer, 即idle状态的timer,
				按到期时间的先后排列, 到期时从该链表中摘除进入ready链表*/
	ezList file_idle_head; /*start监视中, 但是还没有事件*/
	/*single任务没有idle_head链表*/
	ezList ready_head; /*所有ready的任务, run时从该链表摘除*/
	/*
	ezTask.thread_list根据状态在不同链表上:
		所有task的ready状态在ready_head,
		所有task的stop状态不在thread链表上, run状态不在thread链表上
		file/time start时进入idle状态, 在thread的file/time idle链表上,
		single总是start的, idle不在thread链表上, trigger进入ready状态

	注意用户要求stop并不能立即进入stop状态(如果当前是run状态)
	注意用户要求destroy并不能立即销毁(如果当前是run状态)
	*/
	_ezOS_mux os_mux;
	uint16_t run_level;/*即嵌套运行的次数 [0, EZ_THREAD_RUN_NUM_MAX], 也可以认为是run_head链表长度*/
	ezBool is_waiting;
	volatile ezBool has_signal_pending;
} my_thread;

typedef struct {
	my_thread *thread;
	ezList task_head; /*所有属于该组的task*/
	uint32_t flags;
} my_group;

/*
group是全局GC, 创建后不释放.
创建group时指定是否单独创建一个线程,或使用main_thread.
以后可以加入到其他group的线程.

线程thread不是GC, 也不释放, 创建后等待task加入,
task根据所在的group加入对应的线程,
*/

#define TASK_TYPE_FILE		0x1
#define TASK_TYPE_TIMER		0x2
#define TASK_TYPE_SINGLE	0x3

struct ezTask_st {
	my_group *group;
	my_thread *thread;
	ezList thread_list; //thread的动态链表
	ezList group_list; //group的静态链表
	int8_t task_type;
	int8_t task_status;
	ezBool set_stop;/* destroy也会设置stop */
	ezBool set_destroy;
};

typedef struct ezTaskFile_st ezTaskFile;
struct ezTaskFile_st {
	ezTask common;
	ezBTree global_fd_btree;/*保证一个fd全局只能注册一次*/
	ezMask8 type;
	ezBool added_to_mux;
	_ezOS_mux_file os_mux_file;
	/*----------------------*/
	ezMask8 ready_type;
};
#define FILE_TASK_FD(tfile) ((int)((tfile)->global_fd_btree.key))

typedef struct ezTaskTimer_st ezTaskTimer;
struct ezTaskTimer_st {
	ezTask common;
	ezTime period; /*周期*/
	/*----------------------*/
	ezTime hit_time; /*下一次到期时间点*/
};

struct single_ready_node {
	ezList list;
	uintptr_t id;
	void *data; //是GC
};
typedef struct ezTaskSingle_st ezTaskSingle;
struct ezTaskSingle_st {
	ezTask common;
	/*-----------------------*/
	int ready_node_num;/*已经触发还没有运行的节点个数*/
	ezList ready_node_head;/*已经触发还没有运行的节点链表*/
};

/********************************************************/
/*线程本地存储变量: 记录当前线程正在运行的_cur_my_thread*/
static __thread my_thread *_cur_my_thread=NULL;

/*线程本地存储变量: 记录当前线程(可以释放)的local资源*/
static __thread ezList gc_local_head={NULL,NULL};
/*记录当前线程已经销毁，等待free的GC资源*/
static __thread ezList gc_destroy_head={NULL,NULL};
static __thread uint16_t gc_local_level=0;

/*glocal链表存放global资源*/
static EZ_MUTEX gc_global_lock=EZ_MUTEX_INIT_VALUE;
static ezList gc_global_head={NULL,NULL};

void * ezAlloc(intptr_t size, const ezGC_type *type, ezBool clear)
{
	ezGC *gc;
	if(size <= 0) return NULL;

	gc = malloc(sizeof(ezGC)+size);
	if(gc == NULL) return NULL;
	memset(gc, 0, clear? sizeof(ezGC)+size : sizeof(ezGC));

	//ezList_init(&(gc->list));
	//gc->slave_head = NULL;
	//gc->owner_gc = _GC_OWNER_LOCAL;
	gc->sub.type = type;
	gc->sub.flags = (gc_local_level << _GC_LEVEL_SHIFT)|_GC_SIG;
	/*------------------------------------------------------------*/
	if(gc_local_head.next == NULL) {
		ezList_init_head(&gc_local_head);
		ezList_init_head(&gc_destroy_head);
	}
	/*------------------------------------------------------------*/
	ezList_add_tail(&(gc->list), &gc_local_head);
	return gc->sub.p;
}

ezResult ezGC_set_sub(void *p, ezGC_sub *sub, const ezGC_type *sub_type)
{
	ezGC *gc;
	_GET_GC(gc, p, EZ_FAIL);
	sub->type = sub_type;
	uintptr_t top_offset = sub->p - (uintptr_t*)gc;
	if(top_offset <= EZ_MEMBER_OFFSET(ezGC, sub.p)/sizeof(uintptr_t)) {
		ezLog_err("top_offset = %zd, small\n", top_offset);
		return EZ_FAIL;
	}
	if(top_offset > _GC_TOP_OFFSET_MASK) {
		ezLog_err("top_offset = %zd, big\n", top_offset);
		return EZ_FAIL;
	}
	uintptr_t sub_offset = sub->p - (uintptr_t*)p;
	if(sub_offset > _GC_SUB_OFFSET_MASK) {
		ezLog_err("sub_offset = %zd, big\n", sub_offset);
		return EZ_FAIL;
	}
	sub->flags = (top_offset<<_GC_TOP_OFFSET_SHIFT)|_GC_IS_SUB_MASK|_GC_SIG;

	ezGC_sub *sub2 = EZ_FROM_MEMBER(ezGC_sub, p, p);
	sub2->flags = (sub2->flags & ~(_GC_SUB_OFFSET_MASK<<_GC_SUB_OFFSET_SHIFT))|(sub_offset<<_GC_SUB_OFFSET_SHIFT);
	return EZ_SUCCESS;
}

ezBool ezGC_is_destroy(const void *p)
{
	ezGC *gc;
	_GET_GC(gc, p, FALSE);
	return (_GC_IS_DESTROY(gc) != 0);
}

static inline void * _check_type(ezGC *gc, const ezGC_type *type)
{
	uintptr_t sub_offset;
	ezGC_sub *sub = &(gc->sub);
	while(type != sub->type)
	{
		sub_offset = ((sub->flags >> _GC_SUB_OFFSET_SHIFT) & _GC_SUB_OFFSET_MASK);
		if(sub_offset == 0) {
			ezLog_err("gc is not type %s\n", type->name);
			return NULL;
		}
		sub = (ezGC_sub *)((uintptr_t *)sub + sub_offset);
	}
	return sub->p;
}
#if 0
static inline void * _check_final_type(ezGC *gc, const ezGC_type *type)
{
	uintptr_t sub_offset;
	ezGC_sub *sub = &(gc->sub);
	sub_offset = ((sub->flags >> _GC_SUB_OFFSET_SHIFT) & _GC_SUB_OFFSET_MASK);
	while(sub_offset != 0) {
		sub = (ezGC_sub *)((uintptr_t *)sub + sub_offset);
		sub_offset = ((sub->flags >> _GC_SUB_OFFSET_SHIFT) & _GC_SUB_OFFSET_MASK);
	}
	if(sub->type != type) {
		ezLog_err("gc final type mismatch\n");
		return NULL;
	}
	return sub->p;
}
#endif
void * ezGC_check_type(const void *p, const ezGC_type *type)
{
	ezGC *gc;
	_GET_GC(gc, p, NULL);

	if(_GC_IS_DESTROY(gc)) return NULL;

	return _check_type(gc, type);
}

void * ezGC_get_owner(const void *p, const ezGC_type *owner_type)
{
	ezGC *gc;
	_GET_GC(gc, p, NULL);

	if(_GC_IS_DESTROY(gc)) {
		ezLog_warn("p is destroy\n");
		return NULL;
	}

	if(!_GC_IS_SLAVE(gc)) return NULL;

	/*owner_gc没有destroy, 否则gc也会destroy*/
	return _check_type(gc->owner_gc, owner_type);
}

ezBool ezGC_check_owner(const void *p, const void *owner)
{
	ezGC *gc, *owner_gc;
	_GET_GC(gc, p, FALSE);
	_GET_GC(owner_gc, owner, FALSE);

	if(_GC_IS_DESTROY(owner_gc)) {
		ezLog_warn("owner is destroy\n");
		return EZ_FAIL;
	}

	if(_GC_IS_DESTROY(gc)) {
		ezLog_warn("p is destroy\n");
		return EZ_FAIL;
	}

	return (gc->owner_gc == owner_gc);
}

/*把一个local资源变成slave资源*/
ezResult ezGC_Local2Slave(void *p, void *owner)
{
	ezGC *gc, *owner_gc;
	_GET_GC(gc, p, EZ_FAIL);
	_GET_GC(owner_gc, owner, EZ_FAIL);

	if(_GC_IS_DESTROY(owner_gc)) {
		ezLog_warn("owner is destroy\n");
		return EZ_FAIL;
	}

	if(_GC_IS_DESTROY(gc)) {
		ezLog_warn("p is destroy\n");
		return EZ_FAIL;
	}

	if(gc->owner_gc == owner_gc)
		return EZ_SUCCESS;

	if(gc->owner_gc != _GC_OWNER_LOCAL) {
		ezLog_err("gc is not local\n");
		return EZ_FAIL;
	}

	if(gc->slave_head)
	{	/*判断是否循环slave*/
		ezGC *temp_gc = owner_gc;
		do {
			if(gc == temp_gc) {
				ezLog_err("ring\n");
				return EZ_FAIL;
			}
			if(!_GC_IS_SLAVE(temp_gc)) break;
			temp_gc = temp_gc->owner_gc;
		} while(1);
	}

	gc->owner_gc = owner_gc;

	ezList_del(&(gc->list));
/*---------------------------------*/
	if(owner_gc->slave_head != NULL) {
		gc->list.next = owner_gc->slave_head;
		//gc->list.prev = NULL;
		owner_gc->slave_head->prev = &(gc->list);
	} /*else {
		gc->list.next = NULL;
		gc->list.prev = NULL;
	}*/
	owner_gc->slave_head = &(gc->list);
/*---------------------------------*/
	return EZ_SUCCESS;
}

static inline void _Slave_to_Local(ezGC *gc)
{
	ezList *next = gc->list.next;
	ezList *prev = gc->list.prev;
	if(next != NULL) {
		next->prev = prev;
		gc->list.next = NULL;
	}
	if(prev != NULL) {
		prev->next = next;
		gc->list.prev = NULL;
	} else {
		gc->owner_gc->slave_head = next;
	}

	ezList_add_tail(&(gc->list), &gc_local_head);
	gc->owner_gc = _GC_OWNER_LOCAL;
	_GC_SET_LEVEL(gc, gc_local_level);
}

ezResult ezGC_Slave2Local(void *p, const ezGC_type *owner_type)
{
	ezGC *gc, *owner_gc;
	_GET_GC(gc, p, EZ_FAIL);

	if(_GC_IS_DESTROY(gc)) {
		ezLog_err("p is destroy\n");
		return EZ_FAIL;
	}

	if(!_GC_IS_SLAVE(gc)) {
		ezLog_err("gc is not slave\n");
		return EZ_FAIL;
	}

	owner_gc = gc->owner_gc;

	if(!_check_type(owner_gc, owner_type)) {
		ezLog_err("owner_gc type\n");
		return EZ_FAIL;
	}

	_Slave_to_Local(gc);
	return EZ_SUCCESS;
}

/*把一个local资源变成global资源*/
ezResult ezGC_Local2Global(void *p)
{
	ezGC *gc;
	_GET_GC(gc, p, EZ_FAIL);

	if(_GC_IS_DESTROY(gc)) {
		ezLog_err("p is destroy\n");
		return EZ_FAIL;
	}

	if(gc->owner_gc == _GC_OWNER_LOCAL)
	{
		gc->owner_gc = _GC_OWNER_GLOBAL;
		ezList_del(&(gc->list));
		EZ_MUTEX_LOCK(&gc_global_lock);
		/*------------------------------------------------------------*/
		if(gc_global_head.next == NULL)
			ezList_init_head(&gc_global_head);
		/*------------------------------------------------------------*/
		ezList_add_tail(&(gc->list), &gc_global_head);
		EZ_MUTEX_UNLOCK(&gc_global_lock);
		return EZ_SUCCESS;
	}
	if(gc->owner_gc == _GC_OWNER_GLOBAL) {
		return EZ_SUCCESS;
	}
	ezLog_err("gc is slave\n");
	return EZ_FAIL;
}

static void _destroy_gc(ezGC *gc)
{
	ezList *plist;
	ezGC *parent;
	ezList *next, *prev;

	if(gc->owner_gc == _GC_OWNER_LOCAL)
	{
		ezList_del(&(gc->list));
	}
	else if(gc->owner_gc == _GC_OWNER_GLOBAL)
	{
		EZ_MUTEX_LOCK(&gc_global_lock);
		ezList_del(&(gc->list));
		EZ_MUTEX_UNLOCK(&gc_global_lock);
	}
	else /*slave*/
	{
		next = gc->list.next;
		prev = gc->list.prev;
		if(next != NULL) {
			next->prev = prev;
		}
		if(prev != NULL) {
			prev->next = next;
		} else {
			gc->owner_gc->slave_head = next;
		}
	}

	/*
	按后根逆向遍历的顺序释放gc(即: 先销毁slave，再销毁owner)
	在owner的销毁函数中即使保留有slave的指针，也认为已经销毁了.
	当然你再次销毁slave也会忽略，因为有销毁标记，只能销毁一次.
	*/
	parent = NULL;
	plist = &(gc->list); /*上面已经摘除*/
	plist->next = (ezList *)1; /*终止条件1, 下面ezList_init()又改为NULL了*/
	while(plist != (ezList *)1)
	{
		if(plist == NULL) //parent->slave_head链表已经全部删除
		{
			gc = parent; /*下面删除当前的这个parent*/
			plist = gc->list.next; /*先取了下一个叔叔或终止条件1.*/
			parent = gc->owner_gc;
			/*-------------后根遍历中间节点------------*/
			ezList_init(&(gc->list));
 			ezList_add_tail(&(gc->list), &gc_destroy_head);
			if(gc->sub.type && gc->sub.type->destroy_func)
				gc->sub.type->destroy_func(gc->sub.p);
			/*--------------------------------------*/
			continue;
		}

		gc = EZ_FROM_MEMBER(ezGC, list, plist);

		/*---------------先根遍历节点--------------*/
		gc->sub.flags |= _GC_DESTROY_MASK; //先加上销毁标记
		//if(gc->sub.type && gc->sub.type->destroy_func)
		//	gc->sub.type->destroy_func(gc->sub.p);
		/*--------------------------------------*/

		if(gc->slave_head != NULL) {
			plist = gc->slave_head;
			parent = gc;
			continue;
		}

		plist = plist->next; /*先获取下一个兄弟或终止条件1.*/
		/*-------------后根遍历叶子节点------------*/
		ezList_init(&(gc->list));
		ezList_add_tail(&(gc->list), &gc_destroy_head);
		if(gc->sub.type && gc->sub.type->destroy_func)
			gc->sub.type->destroy_func(gc->sub.p);
		/*--------------------------------------*/
	}
}

static void _free_destroy(void)
{
	ezGC *gc;
	ezList *plist, *tmp;
	//这时肯定已经调用过ezAlloc()初始化了gc_destroy_head
	ezList_for_each_safe(plist, tmp, &gc_destroy_head)
	{
		gc = EZ_FROM_MEMBER(ezGC, list, plist);
		/*func必须等到运行完毕之后才能free*/
		if(gc->sub.type == &EZ_FUNC_TYPE)
		{
	/*ezFunc的call和destroy应该是在mutex_p的互斥保护下. 如果在call
	的unlock后的间隙时用户主动destroy_gc, 那么destroy_gc前也应该有lock,
	那么这时pf->hold的值在这里应该已经是TRUE. 这里不需要再lock/unlock(即memory barrier),
	因为观察到pf->hold变为FALSE并free不需要很及时.good*/
	/*但还是要防止pf->hold变为FALSE之后太快free, 见ezFunc_call()函数*/
			ezFunc *pf = (ezFunc *)(gc->sub.p);
			if(pf->hold) continue;
		}
		ezList_del(plist);
		free(gc);
	}
}

static void _auto_destroy_local(uint16_t level)
{
	ezGC *gc;
	ezList *plist;
	/*------------------------------------------------------------*/
	//第一次调用ezAlloc()才会初始化 gc_local_head 和 gc_destroy_head
	if((plist = gc_local_head.prev) == NULL) return; //可能还没有初始化
	/*------------------------------------------------------------*/
	for(; plist != &gc_local_head; plist=gc_local_head.prev) {
		gc = EZ_FROM_MEMBER(ezGC, list, plist);
		if(_GC_GET_LEVEL(gc) < level) break; /*>=的都销毁.*/
		_destroy_gc(gc);
	}
	_free_destroy(); /*释放内存*/
}

/*用户主动释放local或global资源*/
void ezDestroy(void *p)
{
	ezGC *gc;
	_GET_GC(gc, p);

	if(_GC_IS_DESTROY(gc)) return;

	if(_GC_IS_SLAVE(gc)) {
		ezLog_err("gc is slave\n");
		return;
	}

	_destroy_gc(gc);
	/*直接释放gc的内存*/
	_free_destroy();
	return;
}

ezResult ezGC_SlaveDestroy(void *p, const ezGC_type *owner_type)
{
	ezGC *gc;
	_GET_GC(gc, p, EZ_FAIL);

	if(_GC_IS_DESTROY(gc)) return EZ_FAIL;

	if(!_GC_IS_SLAVE(gc)) {
		ezLog_err("gc is not slave\n");
		return EZ_FAIL;
	}

	if(!_check_type(gc->owner_gc, owner_type))
		return EZ_FAIL;

	_destroy_gc(gc);
	/*直接释放gc的内存*/
	_free_destroy();
	return EZ_SUCCESS;
}

void _my_ezGC_Destroy(void *p)
{
	ezGC *gc;
	_GET_GC(gc, p);

	if(_GC_IS_DESTROY(gc)) return;

	_destroy_gc(gc);
	/*直接释放gc的内存*/
	_free_destroy();
	return;
}

void _my_ezGC_Slave2Local(void *p)
{
	ezGC *gc;
	_GET_GC(gc, p);

	if(_GC_IS_DESTROY(gc)) return;

	if(!_GC_IS_SLAVE(gc)) return;

	_Slave_to_Local(gc);
}

uintptr_t ezGCPoint_insert(void)
{
	if(gc_local_level >= _GC_LEVEL_MASK-1) {
		ezLog_err("gc point nest level too many\n");
		return 0;
	}
	return (++gc_local_level);
}

void ezGCPoint_clean(uintptr_t level)
{
	if((level == 0)||(level > _GC_LEVEL_MASK-1)) {
		ezLog_err("gc point level error %zd\n", level);
		return;
	}
	_auto_destroy_local(level);
	if(gc_local_level >= level)
		gc_local_level = level - 1;
	return;
}

void ezGC_exit(int status)
{
	ezGC *gc;
	ezList *plist;

	/*先删除local变量*/
	_auto_destroy_local(0);

	/*再删除global变量*/
	if((plist = gc_global_head.prev) != NULL)
	{
		for(; plist != &gc_global_head; plist=gc_global_head.prev) {
			gc = EZ_FROM_MEMBER(ezGC, list, plist);
			_destroy_gc(gc);
		}
		_free_destroy();
	}

	exit(status);
}

/************************************************************************/

void * ezFunc_create(int size, const ezGC_type *sub_type, ezFuncFunc func, void *arg)
{
	if((func == NULL)||(size < 0)) return NULL;

	ezFunc *pf = ezAlloc(sizeof(ezFunc)+size, &EZ_FUNC_TYPE, TRUE);
	if(pf == NULL) return NULL;

	pf->func = func;
	pf->arg = arg;
	ezGC_set_sub(pf, &(pf->sub), sub_type);
	return pf->sub.p;
}

/*ezFunc_call和destroy应该是在mutex_p的互斥保护下*/
ezBool ezFunc_call(void *p, intptr_t id, void *data, EZ_MUTEX *mutex_p)
{
	ezFunc *pf = ezGC_check_type(p, &EZ_FUNC_TYPE);
	if(!pf || pf->hold) {
		if(pf) ezLog_err("function call renter\n");
		ez_sleep(100);
		return FALSE;
	}
	pf->hold = TRUE;
	if(mutex_p) {
		//pf->mutex_p = mutex_p;
		EZ_MUTEX_UNLOCK(mutex_p);
		pf->func(pf, id, data);
		EZ_MUTEX_LOCK(mutex_p);
	} else {
		pf->func(pf, id, data);
	}
	/*一定要先取pf->destroy, 再赋值pf->hold为FALSE,
	否则可能出现pf->hold变为FALSE之后该线程被抢占,
	导致pf被free后, 再访问pf->destroy就会segment fault*/
	ezBool ret = pf->destroy;
	__sync_synchronize(); //防止优化乱序执行
	pf->hold = FALSE;
	return ret;
}

void * ezFunc_get_arg(const void *p)
{
	ezFunc *pf = ezGC_check_type(p, &EZ_FUNC_TYPE);
	return pf? pf->arg : NULL;
}

void * ezFunc_get_obj(const void *p)
{
	ezFunc *pf = ezGC_check_type(p, &EZ_FUNC_TYPE);
	return pf? pf->obj : NULL;
}

void ezFunc_set_obj(void *p, void *obj)
{
	ezFunc *pf = ezGC_check_type(p, &EZ_FUNC_TYPE);
	if(pf && (pf->obj == NULL)) pf->obj = obj;
	return;
}

ezBool ezFunc_check_equal(const void *p, ezFuncFunc func, void *arg)
{
	ezFunc *pf = ezGC_check_type(p, &EZ_FUNC_TYPE);
	if(pf && (pf->func == func) && (pf->arg == arg)) return TRUE;
	return FALSE;
}

/************************************************************************/
#include "../platform/_os_mux_epoll.c"
/************************************************************************/

static my_thread main_thread; /*不释放*/
static my_group main_group; /*不释放*/

/*为了防止死锁, global_group_lock不能嵌套加thread->lock*/
static EZ_MUTEX global_group_lock=EZ_MUTEX_INIT_VALUE;
static ezMap *group_map=NULL;

static EZ_MUTEX global_fd_lock=EZ_MUTEX_INIT_VALUE;
static ezBTree *global_fd_root=NULL;

static ezList child_signal_head;

static ezResult my_thread_init(my_thread *pt)
{
	//memset(pt, 0, sizeof(my_thread));
	if(!_ezOS_mux_init(&(pt->os_mux))) {
		ezLog_err("_ezOS_mux_init\n");
		return EZ_FAIL;
	}

	ez_mutex_init(&(pt->lock));
	ezList_init_head(&(pt->timer_idle_head));
	ezList_init_head(&(pt->file_idle_head));
	ezList_init_head(&(pt->ready_head));
	//pt->run_level = 0;
	//pt->is_waiting = FALSE;
	//pt->has_signal_pending = FALSE;
	return EZ_SUCCESS;
}

/*目前线程创建后不退出*/
static void my_thread_uinit(my_thread *pt)
{
	if(pt->run_level != 0) {
		/*同时包括了loop正在条件等待的情况*/
		ezLog_err("the thread is in running loop\n");
		return;
	}

	/*检查各个链表是否empty*/
	//ezLog_err("the thread has task\n"); return;

	_ezOS_mux_uinit(&(pt->os_mux));

	ez_mutex_destroy(&(pt->lock));
	return;
}

static void my_thread_loop(my_thread *pt, ezTime timeout);
static void * my_thread_run(void *arg)
{
	my_thread *pt = (my_thread *)arg;
	_cur_my_thread = pt;

	for(;;) {
		my_thread_loop(pt, -1);
	}

	my_thread_uinit(pt);
	free(pt);
	return NULL;
}

static void _runtime_init(void)
{
	if(group_map != NULL) return;
	EZ_MUTEX_LOCK(&global_group_lock);
	if(group_map == NULL)
	{
		group_map = ezMap_create(sizeof(my_group), NULL, FALSE);
		ezGC_Local2Global(group_map);
		if(my_thread_init(&main_thread) != EZ_SUCCESS) {
			EZ_MUTEX_UNLOCK(&global_group_lock);
			ezLog_err("init fail\n");
			exit(-1);
		}
		main_group.thread = &main_thread;
		ezList_init_head(&(main_group.task_head));
		main_group.flags = 1;

		ezList_init_head(&child_signal_head);
	}
	EZ_MUTEX_UNLOCK(&global_group_lock);
}

#define CHECK_RUNTIME_INIT \
do {\
	if(!group_map) _runtime_init();\
} while(0)

/*目前group创建后不销毁, group单独创建的线程也不退出*/
ezResult ezTask_create_groud(const char *group, uint32_t flags)
{
	CHECK_RUNTIME_INIT;

	if(group == NULL) return EZ_SUCCESS;

	EZ_MUTEX_LOCK(&global_group_lock);

	my_group *pg = ezMap_add_node_string(group_map, group);
	if(pg->thread != NULL) {
		EZ_MUTEX_UNLOCK(&global_group_lock);
		ezLog_warn("group %s has exist\n", group);
		return EZ_SUCCESS;
	}

	ezList_init_head(&(pg->task_head));

	my_thread *pt = &main_thread;

	if(flags == 1)
	{
#if defined(EZ_THREAD_NO)
		ezLog_warn("no thread support, group %s no thread\n", group);
		flags = 0;
#else
		pt = malloc(sizeof(my_thread));
		if(pt == NULL) {
			ezLog_err("group %s malloc thread error\n", group);
			EZ_MUTEX_UNLOCK(&global_group_lock);
			return EZ_ENOMEM;
		}
		if(my_thread_init(pt) != EZ_SUCCESS) {
			ezLog_err("group %s init thread fail\n", group);
			ezMapNode_delete(pg);
			EZ_MUTEX_UNLOCK(&global_group_lock);
			return EZ_FAIL;
		}
		EZ_THREAD_ID thread_id;
		if(0 != pthread_create(&thread_id, NULL, my_thread_run, pt)) {
			ezLog_err("group %s create thread fail\n", group);
			ezMapNode_delete(pg);
			EZ_MUTEX_UNLOCK(&global_group_lock);
			return EZ_FAIL;
		}
#endif
	}
	pg->thread = pt;
	pg->flags = flags;

	EZ_MUTEX_UNLOCK(&global_group_lock);
	return EZ_SUCCESS;
}

void ezTask_run(void)
{
	CHECK_RUNTIME_INIT;

	my_thread *pt = &main_thread;
	_cur_my_thread = pt;

	/*不自动释放该函数之前创建的local GC.
	道理上也多余, 并且容易引起误会:
	怎么ezTask_run之前创建的task不运行!
	怎么ezTask_run之前创建的资源不在了!
	哦, 忘记把他们变成global了*/
	//_auto_destroy_local(0);

	for(;;) { //main_thread不可能退出
		my_thread_loop(pt, -1);
	}

	my_thread_uinit(pt);
	return;
}

/*该函数是和其他的事件循环系统配合使用的, 当有一个file task时,
也通过fd来创建一个其他循环系统的相应对象, 当其他循环系统通知
我们有数据可读时, 我们就调用try_run来处理:
my_thread_loop(,0)也会发现file task可读, 然后add ready,处理.
如果之前还遗留有ready task要处理, my_thread_loop(,0)不会
检测file task是否可读, 所以至少需要两次my_thread_loop(,0) */
void ezTask_try_run(void)
{
	CHECK_RUNTIME_INIT;

	my_thread *pt = &main_thread;
	_cur_my_thread = pt;

	my_thread_loop(pt, 0);
	do {
		my_thread_loop(pt, 0);
	} while(pt->ready_head.next != &(pt->ready_head));

	_cur_my_thread = NULL;
	return;
}

/*=======================================================*/

static void _task_destroy(ezTask *task)
{
	my_thread *pt = task->thread;
	ezTaskFile *file;
	ezTaskSingle *single;
	ezList *plist;
	struct single_ready_node *node;

	if(task->set_destroy) return;
	task->set_destroy = TRUE;
	task->set_stop = TRUE;

	switch(task->task_type)
	{
	case TASK_TYPE_FILE:
		file = (ezTaskFile *)task;
		EZ_MUTEX_LOCK(&global_fd_lock);
		ezBTree_delete(&global_fd_root, &(file->global_fd_btree));
		EZ_MUTEX_UNLOCK(&global_fd_lock);
		_ezOS_mux_del_file(&(pt->os_mux), file);
		break;

	case TASK_TYPE_SINGLE:
		single = (ezTaskSingle *)task;
		while((plist=single->ready_node_head.next) != &(single->ready_node_head))
		{
			ezList_del(plist);
			node = EZ_FROM_MEMBER(struct single_ready_node, list, plist);
			if(node->data) _my_ezGC_Destroy(node->data);
			free(node);
		}
		break;

	case TASK_TYPE_TIMER:
		break;
	}

	/*这里表明global_group_lock是最底层的锁, 不能嵌套thread->lock*/
	EZ_MUTEX_LOCK(&global_group_lock);
	ezList_del(&(task->group_list));
	EZ_MUTEX_UNLOCK(&global_group_lock);

	//如果task正在run, 就不需要执行 list_del
	ezList_del(&(task->thread_list));
	return;
}

static void _gc_task_destroy(void *p)
{
	ezTask *task = (ezTask *)p;
	my_thread *pt = task->thread;
	EZ_MUTEX_LOCK(&(pt->lock));
	_task_destroy(task);
	EZ_MUTEX_UNLOCK(&(pt->lock));
	return;
}

static ezGC_type EZ_TASK_TYPE = {
	.destroy_func = _gc_task_destroy,
	.name = "ezTask",
};

static ezBool _add_task(ezTask *task, const char *group)
{
	my_group *pg;
	EZ_MUTEX_LOCK(&global_group_lock);
	if(group) {
		pg = ezMap_find_node_string(group_map, group);
		if(pg == NULL) {
			EZ_MUTEX_UNLOCK(&global_group_lock);
			task->set_destroy = TRUE;
			task->set_stop = TRUE;
			ezLog_err("not found group %s\n", group);
			return FALSE;
		}
	} else {
		pg = &main_group;
	}
	task->group = pg;
	task->thread = pg->thread;
	ezList_add_tail(&(task->group_list), &(pg->task_head));
	EZ_MUTEX_UNLOCK(&global_group_lock);
	return TRUE;
}

ezTask * ezTask_add_single(const char *group, ezFuncFunc func, void *arg)
{
	ezTaskSingle *ret;

	CHECK_RUNTIME_INIT;

	ret = (ezTaskSingle *)ezFunc_create(sizeof(ezTaskSingle), &EZ_TASK_TYPE, func, arg);
	if(ret == NULL) return NULL;

	//memset(ret, 0, sizeof(ezTaskSingle));
	//ezList_init(&(ret->common.thread_list));
	//ezList_init(&(ret->common.task_list));
	//ret->common.thread = NULL;
	ret->common.task_type = TASK_TYPE_SINGLE;
	ret->common.task_status = EZ_TASK_IDLE;
	//ret->common.set_stop = FALSE;
	//ret->ready_node_num = 0;
	ezList_init_head(&(ret->ready_node_head));

	if(_add_task((ezTask *)ret, group))
		return (ezTask *)ret;

	_my_ezGC_Destroy(ret); //_add_task()里面已经set_destroy了.
	return NULL;
}

ezTask * ezTask_add_timer(const char *group, ezFuncFunc func, void *arg)
{
	ezTaskTimer *ret;

	CHECK_RUNTIME_INIT;

	ret = (ezTaskTimer *)ezFunc_create(sizeof(ezTaskTimer), &EZ_TASK_TYPE, func, arg);
	if(ret == NULL) return NULL;

	//memset(ret, 0, sizeof(ezTaskTimer));
	//ezList_init(&(ret->common.thread_list));
	//ezList_init(&(ret->common.task_list));
	//ret->common.thread = NULL;
	ret->common.task_type = TASK_TYPE_TIMER;
	ret->common.task_status = EZ_TASK_STOP;
	ret->common.set_stop = TRUE;
	//ret->period = 0;
	//ret->hit_time = 0;

	if(_add_task((ezTask *)ret, group))
		return (ezTask *)ret;

	_my_ezGC_Destroy(ret); //_add_task()里面已经set_destroy了.
	return NULL;
}

ezTask * ezTask_add_file(const char *group, int fd, ezFuncFunc func, void *arg)
{
	ezTaskFile *ret;

	if(fd == -1) return NULL;

	CHECK_RUNTIME_INIT;

	ret = (ezTaskFile *)ezFunc_create(sizeof(ezTaskFile), &EZ_TASK_TYPE, func, arg);
	if(ret == NULL) return NULL;

	//memset(ret, 0, sizeof(ezTaskFile));
	//ezList_init(&(ret->common.thread_list));
	//ezList_init(&(ret->common.task_list));
	//ret->common.thread = NULL;
	ret->common.task_type = TASK_TYPE_FILE;
	ret->common.task_status = EZ_TASK_STOP;
	ret->common.set_stop = TRUE;
	//ret->type = 0;
	//ret->epoll_add = FALSE;

	ezBTree_init(&(ret->global_fd_btree), fd);
	EZ_MUTEX_LOCK(&global_fd_lock);
	if(ezBTree_find(global_fd_root, fd) != NULL) {
		EZ_MUTEX_UNLOCK(&global_fd_lock);
		ezLog_err("already add the same fd\n");
		return NULL;
	}
	ezBTree_insert(&global_fd_root, &(ret->global_fd_btree));
	EZ_MUTEX_UNLOCK(&global_fd_lock);

	if(_add_task((ezTask *)ret, group))
		return (ezTask *)ret;

	EZ_MUTEX_LOCK(&global_fd_lock);
	ezBTree_delete(&global_fd_root, &(ret->global_fd_btree));
	EZ_MUTEX_UNLOCK(&global_fd_lock);

	_my_ezGC_Destroy(ret); //_add_task()里面已经set_destroy了.
	return NULL;
}

uint8_t ezTask_get_status(ezTask * task)
{
	uint8_t ret;
	if(task == NULL) return EZ_TASK_DESTROY;
	my_thread *pt = task->thread;
	EZ_MUTEX_LOCK(&(pt->lock));
	if(task->set_destroy)
		ret = EZ_TASK_DESTROY;
	else if(task->set_stop)
		ret = EZ_TASK_STOP;
	else
		ret = task->task_status;
	EZ_MUTEX_UNLOCK(&(pt->lock));
	return ret;
}

/*****************************************************************************/
/*已经ready的task仍然是之前的ready_type*/
void ezTaskFile_start(ezTask *task, ezMask8 type)
{
	my_thread *pt;
	ezTaskFile *file;

	if((task == NULL)
		||(task->task_type != TASK_TYPE_FILE)
		||(type == 0)
		||(type&(~EZ_FILE_MASK)))
		return;

	pt = task->thread;
	file = (ezTaskFile *)task;

	EZ_MUTEX_LOCK(&(pt->lock));
	if(task->set_destroy ||
		(file->type == type && !task->set_stop))
	{
		EZ_MUTEX_UNLOCK(&(pt->lock));
		return;
	}

	file->type = type;
	if((task->task_status != EZ_TASK_RUN) &&
		(task->task_status != EZ_TASK_READY))
	{
		if(file->added_to_mux) {//已经添加,但是还没有事件
			_ezOS_mux_del_file(&(pt->os_mux), file);
		} else {//没有添加
			file->added_to_mux = TRUE;
		}
		_ezOS_mux_add_file(&(pt->os_mux), file, FILE_TASK_FD(file), type);
		if(pt->is_waiting) _ezOS_mux_waken(&(pt->os_mux));
	} /*如果是run或ready状态, 什么都不做, 留到run之后做*/

	if(task->set_stop) {
		ezList_add_tail(&(task->thread_list), &(pt->file_idle_head));
		task->set_stop = FALSE;
		task->task_status = EZ_TASK_IDLE;
	}

	EZ_MUTEX_UNLOCK(&(pt->lock));
	return;
}

void ezTaskFile_stop(ezTask *task)
{
	my_thread *pt;
	ezTaskFile *file;

	if((task == NULL) ||
		(task->task_type != TASK_TYPE_FILE))
		return;
	
	pt = task->thread;
	file = (ezTaskFile *)task;

	EZ_MUTEX_LOCK(&(pt->lock));
	if(task->set_stop) {
		EZ_MUTEX_UNLOCK(&(pt->lock));
		return;
	}

	if(file->added_to_mux) {
		file->added_to_mux = FALSE;
		_ezOS_mux_del_file(&(pt->os_mux), file);
	}

	task->set_stop = TRUE;
	if(task->task_status != EZ_TASK_RUN) {
		ezList_del(&(task->thread_list));
		task->task_status = EZ_TASK_STOP;
	}

	EZ_MUTEX_UNLOCK(&(pt->lock));
	return;
}

int ezTaskFile_get_fd(ezTask *task)
{
	if((task == NULL) ||
		(task->task_type != TASK_TYPE_FILE))
		return -1;

	ezTaskFile *file = (ezTaskFile *)task;
	return FILE_TASK_FD(file);
}

static void _timer_insert_enable_list(my_thread *pt, ezTaskTimer *ptimer)
{
	ezTaskTimer *p2;
	ezTime ht = ptimer->hit_time;
	ezList *plist;
	ezList_for_each(plist, &(pt->timer_idle_head))
	{
		p2 = (ezTaskTimer *)EZ_FROM_MEMBER(ezTask, thread_list, plist);
		if(EZ_TIME_OFFSET(ht, p2->hit_time) < 0) break;
	}
	ezList_add(&(ptimer->common.thread_list), plist->prev, plist);
	return;
}

void ezTaskTimer_start(ezTask *task, ezTime first_delay, ezTime period)
{
	my_thread *pt;
	ezTaskTimer * timer;

	if((task == NULL) ||
		(task->task_type != TASK_TYPE_TIMER))
		return;

	if(first_delay < 0) first_delay = 0; //第一次立即触发

	if(period < 0) period = 0; //只触发一次

	pt = task->thread;
	timer = (ezTaskTimer *)task;

	EZ_MUTEX_LOCK(&(pt->lock));
	if(task->set_destroy) {
		EZ_MUTEX_UNLOCK(&(pt->lock));
		return;
	}

	timer->period = period;
	timer->hit_time = ez_get_time() + first_delay;

	if((task->task_status != EZ_TASK_RUN) &&
		(task->task_status != EZ_TASK_READY))
	{
		ezList_del(&(timer->common.thread_list));
		_timer_insert_enable_list(pt, timer);
		task->task_status = EZ_TASK_IDLE;
		if(pt->is_waiting) _ezOS_mux_waken(&(pt->os_mux));
	}
	task->set_stop = FALSE;

	EZ_MUTEX_UNLOCK(&(pt->lock));
	return;
}

void ezTaskTimer_stop(ezTask *task)
{
	my_thread *pt;
	ezTaskTimer * timer;

	if((task == NULL) ||
		(task->task_type != TASK_TYPE_TIMER))
		return;

	pt = task->thread;
	timer = (ezTaskTimer *)task;

	EZ_MUTEX_LOCK(&(pt->lock));
	if(task->set_stop) {
		EZ_MUTEX_UNLOCK(&(pt->lock));
		return;
	}

	task->set_stop = TRUE;
	if(task->task_status != EZ_TASK_RUN) {
		ezList_del(&(timer->common.thread_list));
		task->task_status = EZ_TASK_STOP;
	}

	EZ_MUTEX_UNLOCK(&(pt->lock));
	return;
}

ezResult ezTaskSingle_trigger(ezTask *task, intptr_t id, void *data)
{
	my_thread *pt;
	ezTaskSingle *single;
	struct single_ready_node *node;

	if((task == NULL) ||
		(task->task_type != TASK_TYPE_SINGLE))
		return EZ_ENOENT;

	pt = task->thread;
	single = (ezTaskSingle *)task;

	EZ_MUTEX_LOCK(&(pt->lock));
	if(task->set_destroy) {
		EZ_MUTEX_UNLOCK(&(pt->lock));
		return EZ_FAIL;
	}

	node = (struct single_ready_node *)malloc(sizeof(struct single_ready_node));
	if(node == NULL) {
		EZ_MUTEX_UNLOCK(&(pt->lock));
		ezLog_err("alloc ready node fail\n");
		return EZ_ENOMEM;
	}

	if(data != NULL)
	{
		if(ezGC_check_owner(data, task)) {
			EZ_MUTEX_UNLOCK(&(pt->lock));
			free(node);
			ezLog_err("data is already owned by the task\n");
			return EZ_FAIL;
		}
		if(ezGC_Local2Slave(data, task) != EZ_SUCCESS) {
			EZ_MUTEX_UNLOCK(&(pt->lock));
			free(node);
			return EZ_FAIL;
		}
	}

	memset(node, 0, sizeof(struct single_ready_node));
	//ezList_init(&(node->list));
	node->id = id;
	node->data = data;
	ezList_add_tail(&(node->list), &(single->ready_node_head));
	++single->ready_node_num;

	if(task->task_status == EZ_TASK_IDLE) {
		single->common.task_status = EZ_TASK_READY;
		ezList_add_tail(&(single->common.thread_list), &(pt->ready_head));
		if(pt->is_waiting) _ezOS_mux_waken(&(pt->os_mux));
	}

	EZ_MUTEX_UNLOCK(&(pt->lock));
	return EZ_SUCCESS;
}

int ezTaskSingle_get_ReadyNum(ezTask *task)
{
	int n;
	my_thread *pt;
	ezTaskSingle *single;

	if((task == NULL) ||
		(task->task_type != TASK_TYPE_SINGLE))
		return 0;

	pt = task->thread;
	single = (ezTaskSingle *)task;
	EZ_MUTEX_LOCK(&(pt->lock));
	n = single->ready_node_num;
	EZ_MUTEX_UNLOCK(&(pt->lock));
	return n;
}

/*****************************************************************************/
/*返回0表示没有可执行任务*/
static int _do_task(my_thread *pt)
{
	ezList *plist;
	ezTask *task;
	ezTaskTimer *timer;
	ezTaskFile *file;
	ezTaskSingle *single;
	ezList *plist2;
	ezBool destroy;
	int n=0;
	struct single_ready_node * node;

	while((plist=pt->ready_head.next) != &(pt->ready_head))
	{
		ezList_del(plist);
		task = EZ_FROM_MEMBER(ezTask, thread_list, plist);
		task->task_status = EZ_TASK_RUN;
		switch(task->task_type)
		{
		case TASK_TYPE_FILE:
			file = (ezTaskFile *)task;
			destroy = ezFunc_call(file, FILE_TASK_FD(file), (void *)(uintptr_t)(file->ready_type), &(pt->lock));
			if(destroy) continue;

			if(file->ready_type&(EZ_FILE_ERR|EZ_FILE_HUP)) {
				ezLog_err("file abnormal, destroy\n");
				_task_destroy(task); /*RUN状态不会释放内存*/
				continue;
			}

			if(!task->set_stop)
			{
				if(file->added_to_mux) {
					_ezOS_mux_active_file(&(pt->os_mux), file);
				} else {
					file->added_to_mux = TRUE;
					_ezOS_mux_add_file(&(pt->os_mux), file, FILE_TASK_FD(file), file->type);
				}
				ezList_add_tail(plist, &(pt->file_idle_head));
				task->task_status = EZ_TASK_IDLE;
			} else {
				task->task_status = EZ_TASK_STOP;
			}
			break;
		case TASK_TYPE_TIMER:
			timer = (ezTaskTimer *)task;
			destroy = ezFunc_call(timer, timer->period, NULL, &(pt->lock));
			if(destroy) continue;

			if(!task->set_stop)
			{
				if(timer->period > 0) {
					task->task_status = EZ_TASK_IDLE;
					_timer_insert_enable_list(pt, timer);
				} else {
					task->task_status = EZ_TASK_STOP;
					task->set_stop = TRUE;
				}
			} else {
				task->task_status = EZ_TASK_STOP;
			}
			break;
		case TASK_TYPE_SINGLE:
			single = (ezTaskSingle *)task;
			--single->ready_node_num;

			plist2 = single->ready_node_head.next;
			ezList_del(plist2);

			node = EZ_FROM_MEMBER(struct single_ready_node, list, plist2);
			if(node->data) _my_ezGC_Slave2Local(node->data);
			destroy = ezFunc_call(single, node->id, node->data, &(pt->lock));
			//node->data是Local状态, 后面会自动释放.
			free(node);

			if(destroy) continue;

			if(!task->set_stop)
			{
				if(single->ready_node_num > 0) {
					/*每次只做一个, 挂到ready链表尾部, 更公平*/
					ezList_add_tail(plist, &(pt->ready_head));
				} else {
					task->task_status = EZ_TASK_IDLE;
				}
			} else {
				task->task_status = EZ_TASK_STOP;
			}
			break;
		}
		n = 1;
		/*---------------------------------------------------------*/
	}
	return n;
}

/*timeout是用户设定的最大等待时间:<0 永远, =0 不等待*/
static inline ezTime _check_timeout(my_thread *pt, ezTime timeout)
{
	ezTime now, temp;
	ezList *plist;
	ezTaskTimer *ptimer;

	now = ez_get_time();
	plist = pt->timer_idle_head.next;
	if(plist != &(pt->timer_idle_head))
	{
		ptimer = (ezTaskTimer *)EZ_FROM_MEMBER(ezTask, thread_list, plist);
		temp = EZ_TIME_OFFSET(ptimer->hit_time, now);
		if(temp <= 0)
			timeout = 0;
		else if((timeout < 0)||(timeout > temp))
			timeout = temp;
	}
	return timeout;
}

static inline void _check_timer(my_thread *rt)
{
	ezTime now;
	ezList *plist, *pn;
	ezTaskTimer *ptimer;

	now = ez_get_time();
	ezList_for_each_safe(plist, pn, &(rt->timer_idle_head))
	{
		ptimer = (ezTaskTimer *)EZ_FROM_MEMBER(ezTask, thread_list, plist);
		if(EZ_TIME_OFFSET(ptimer->hit_time, now) > 0) break;

		ptimer->hit_time = now + ptimer->period;/*这时计算更精确, period==0在do_task中考虑*/
		ptimer->common.task_status = EZ_TASK_READY;
		ezList_del(&(ptimer->common.thread_list));
		ezList_add_tail(&(ptimer->common.thread_list), &(rt->ready_head));
	}
}

static int _do_signal(my_thread *pt);
static void my_thread_loop(my_thread *pt, ezTime timeout)
{
	uintptr_t gc_point;

	EZ_MUTEX_LOCK(&(pt->lock));

	if(pt->run_level >= EZ_THREAD_RUN_NUM_MAX) {
		EZ_MUTEX_UNLOCK(&(pt->lock));
		ezLog_err("runtime nest running too many levels %d>=%d, skip\n",
			pt->run_level, EZ_THREAD_RUN_NUM_MAX);
		ez_sleep(timeout>0? timeout : 100);
		return;
	}

	gc_point = ezGCPoint_insert();

	++pt->run_level;

	if(_do_signal(pt) + _do_task(pt) == 0) {
		timeout = _check_timeout(pt, timeout);
		/*timeout等于0表示不阻塞检测事件, <0表示永远等待*/
		pt->is_waiting = TRUE;
		_ezOS_mux_check_file(pt, timeout);
		pt->is_waiting = FALSE;
		_check_timer(pt);
	}

	--pt->run_level;

	EZ_MUTEX_UNLOCK(&(pt->lock));

	ezGCPoint_clean(gc_point);
	return;
}

/************************************************************************/

/*总是在main_thread中执行信号处理*/
static struct {
	my_thread *thread;
	ezSignalFunc handler;
	void *arg;
	volatile ezBool happen;
} signal_info[33]; /* 0不用, (0, 32] */

typedef struct {
	ezList list;
	ezChildSignalFunc handler;
	void *arg;
	pid_t pid;
} child_signal_info;

static inline void _do_child_signal(my_thread *pt)
{
	pid_t pid;
	ezList *plist;
	child_signal_info *pinfo;
	int status;
	do {
		pid = waitpid(-1, &status, WNOHANG);
		if((pid == -1)||(pid == 0)) break;
		ezList_for_each(plist, &child_signal_head)
		{
			pinfo = EZ_FROM_MEMBER(child_signal_info, list, plist);
			if(pinfo->pid == pid) {
				ezList_del(plist);
				EZ_MUTEX_UNLOCK(&(pt->lock));
				pinfo->handler(pid, status, pinfo->arg);
				free(pinfo);
				EZ_MUTEX_LOCK(&(pt->lock));
				break;
			}
		}
	} while(1);
	return;
}

static int _do_signal(my_thread *pt)
{
	int i, n=0;
	while(pt->has_signal_pending == 1)
	{
		pt->has_signal_pending = 0;
		for(i=1; i<=32; i++)
		{
			if((signal_info[i].happen == TRUE) &&
				(signal_info[i].thread == pt))
			{
				signal_info[i].happen = FALSE;
				if(i == SIGCHLD) {
					_do_child_signal(pt);
				} else {
					ezSignalFunc handler = signal_info[i].handler;
					void *arg = signal_info[i].arg;
					EZ_MUTEX_UNLOCK(&(pt->lock));
					if(handler) handler(i, arg);
					EZ_MUTEX_LOCK(&(pt->lock));
				}
				n = 1;
			}
		}
	}
	return n;
}

static void _default_signal_handler(int sig)
{
	my_thread *pt = signal_info[sig].thread;
	if((sig > 0)&&(sig <= 32)&&(pt != NULL)) {
		signal_info[sig].happen = 1;
		signal_info[sig].thread->has_signal_pending = 1;
		_ezOS_mux_waken(&(pt->os_mux));
	}
	return;
}

void ezSignal(int sig, ezSignalFunc handler, void *arg)
{
	struct sigaction act;

	if((sig <= 0)||(sig > 32)||(sig == SIGCHLD)) {
		ezLog_err("only support sig in (0, 32], and not SIGCHLD\n");
		return;
	}

	CHECK_RUNTIME_INIT;

	my_thread *pt = &main_thread;

	EZ_MUTEX_LOCK(&(pt->lock));
	if(handler == NULL) {
		signal_info[sig].thread = NULL;
		signal_info[sig].handler = NULL;
		act.sa_handler = SIG_IGN;
	} else if(handler == (ezSignalFunc)-1) {
		signal_info[sig].thread = NULL;
		signal_info[sig].handler = NULL;
		act.sa_handler = SIG_DFL;
	} else {
		signal_info[sig].thread = pt;
		signal_info[sig].handler = handler;
		act.sa_handler = _default_signal_handler;
	}
	EZ_MUTEX_UNLOCK(&(pt->lock));

	act.sa_flags = SA_NODEFER;
	sigemptyset(&(act.sa_mask));
	sigaction(sig, &act, NULL);
	return;
}

void ezSignal_child(pid_t pid, ezChildSignalFunc handler, void *arg)
{
	ezList *plist;
	child_signal_info *pinfo;

	if(pid < 2) {
		ezLog_err("pid=%d\n", pid);
		return;
	}

	if(handler == (ezChildSignalFunc)-1)
		handler = NULL; //默认处理就是忽略

	CHECK_RUNTIME_INIT;

	my_thread *pt = &main_thread;

	EZ_MUTEX_LOCK(&(pt->lock));
	ezList_for_each(plist, &child_signal_head)
	{
		pinfo = EZ_FROM_MEMBER(child_signal_info, list, plist);
		if(pinfo->pid == pid)
		{
			if(handler == NULL) {
				ezList_del(plist);
				free(pinfo);
			} else {
				ezLog_err("same pid\n");
			}
			EZ_MUTEX_UNLOCK(&(pt->lock));
			return;
		}
	}

	if(handler != NULL)
	{
		pinfo = malloc(sizeof(child_signal_info));
		if(pinfo == NULL) {
			ezLog_err("malloc fail\n");
			EZ_MUTEX_UNLOCK(&(pt->lock));
			return;
		}
		memset(pinfo, 0, sizeof(child_signal_info));
		//ezList_init(&(pinfo->list));
		ezList_add_tail(&(pinfo->list), &child_signal_head);
		pinfo->handler = handler;
		pinfo->arg = arg;
		pinfo->pid = pid;
	}

	if(signal_info[SIGCHLD].thread == NULL)
	{
		signal_info[SIGCHLD].thread = pt;
		EZ_MUTEX_UNLOCK(&(pt->lock));

		struct sigaction act;
		act.sa_handler = _default_signal_handler;
		act.sa_flags = SA_NODEFER;
		sigemptyset(&(act.sa_mask));
		sigaction(SIGCHLD, &act, NULL);
		return;
	}

	EZ_MUTEX_UNLOCK(&(pt->lock));
	return;
}

/************************************************************************/

//等待队列节点
struct cond_wait_node {
	ezList list;
	my_thread *thread;
};
/*cond不需要自己的锁, 因为要求在用户锁中.*/
struct ezCond_t {
	EZ_MUTEX *user_mutex;
	volatile uint32_t waken_seq;
	ezList wait_node_head; /*等待队列头*/
};

static void _cond_destroy_func(void *p)
{
	ezCond *cond = (ezCond *)p;
	/*cond在wait中不会被destroy, 除非程序有bug*/
	if(!ezList_is_empty(&(cond->wait_node_head))) {
		ezLog_err("some runtime loop is waiting on the Cond\n");
	}
	return;
}

static ezGC_type EZ_COND_TYPE = {
	.destroy_func = _cond_destroy_func,
	.name = "ezCond",
};
ezCond * ezTaskCond_create(void)
{
	ezCond *ret;
	ret = (ezCond *)ezAlloc(sizeof(ezCond), &EZ_COND_TYPE, TRUE);
	if(ret != NULL) {
		//ret->user_mutex = NULL;
		//ret->waken_seq = 0;
		ezList_init_head(&(ret->wait_node_head));
	}
	return ret;
}

void ezTaskCond_wait(ezCond *cond, EZ_MUTEX *mutex_p, ezTime timeout)
{
	uint32_t cur_waken_seq;
	struct cond_wait_node node;

	if(cond == NULL) return;

	if(mutex_p == NULL) {
		ezLog_err("pmutex is NULL\n");
		return;
	}

	/*一个ezTaskCond 和EZ_MUTEX固定搭配了, 不能改*/
	if((cond->user_mutex != NULL)&&(cond->user_mutex != mutex_p)) {
		ezLog_err("the mutex is not same\n");
		return;
	}

	if(cond->user_mutex == NULL)
		cond->user_mutex = mutex_p;

	node.thread = _cur_my_thread;

	/*必须在unlock(*pmutex)之前取waken_seq*/
	cur_waken_seq = cond->waken_seq;

	ezList_init(&(node.list));
	ezList_add_tail(&(node.list), &(cond->wait_node_head));

	EZ_MUTEX_UNLOCK(mutex_p);

	if(timeout <= 0)
	{
		do {
			my_thread_loop(node.thread, -1);
			__sync_synchronize(); //rmb, 保证在这之后读出waken_seq
		} while(cur_waken_seq == cond->waken_seq);
	}
	else
	{
		ezTime check_tick, end_tick, tt;
		tt = timeout;
		end_tick = ez_get_time()+timeout;
		do {
			my_thread_loop(node.thread, tt);
			check_tick = ez_get_time();
			tt = EZ_TIME_OFFSET(end_tick, check_tick);
			__sync_synchronize(); //rmb, 保证在这之后读出waken_seq
		} while((cur_waken_seq == cond->waken_seq)&&(tt > 0));
	}

	EZ_MUTEX_LOCK(mutex_p);

	ezList_del(&(node.list));
	return;
}

void ezTaskCond_waken(ezCond *cond, ezBool broadcast)
{
	ezList *plist;
	struct cond_wait_node *node;
	my_thread *pt;

	if(cond == NULL) return;

	if(!ezList_is_empty(&(cond->wait_node_head)))
	{
		++cond->waken_seq;
		__sync_synchronize(); //wmb, 保证在这之前写入waken_seq
		ezList_for_each(plist, &(cond->wait_node_head))
		{
			node = EZ_FROM_MEMBER(struct cond_wait_node, list, plist);
			pt = node->thread;
			EZ_MUTEX_LOCK(&(pt->lock));
			if(pt->is_waiting) _ezOS_mux_waken(&(pt->os_mux));
			EZ_MUTEX_UNLOCK(&(pt->lock));

			if(!broadcast) break;
		}
	}
	return;
}

