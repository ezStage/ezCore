# ezCore运行时系统

## 1. 内存垃圾回收(GC)系统

内存垃圾回收系统要实现的目标是：对于内存资源（数据结构和对象），用户只需要创建，不需要主动释放。在设计过程中，会受到很多因素的干扰，如何选择正确的设计路线，
一个根本的原则就是不忘初心，牢记目标。

```c
typedef void (*ezGCDestroyFunc)(void *p);
typedef struct {
        ezGCDestroyFunc destroy_func;
        const char *name;
} ezGC_type;

/*分配一个GC资源，即一段可以自动释放的内存. clear表示内存是否清0*/
void * ezAlloc(intptr_t size, const ezGC_type *type, ezBool clear);
void ezDestroy(void *p); /*用户主动释放 local 或 global 资源*/

/*把一个 local 资源变成 global 资源*/
ezResult ezGC_Local2Global(void *p);

/*把一个 local 资源变成 slave 资源*/
ezResult ezGC_Local2Slave(void *p, void *owner);

/*把一个 slave 资源变成 local 资源*/
ezResult ezGC_Slave2Local(void *p, const ezGC_type *owner_type);
```

用 ezGC_type 来描述一种类型的名字和销毁函数，ezGC_type及其成员都应该是静态全局只读变量。ezGC_type中没有大小，即可以是变长的。name可以为NULL，表示没有名字。destroy_func也可以为NULL，表示释放该类型内存时，没有额外的动作。甚至调用 ezAlloc()时，参数type也可以为NULL。

定义了 Local，Global，Slave 三种资源状态，设计了如下几个规则：

- 所有GC资源分配后初始都是 Local 状态，Local 状态的GC资源都会在其创建后的某个时刻被自动销毁。

- Local 状态的资源可以被转换成 Global 状态，类似于全局变量，不会被自动销毁。

- Local 状态的资源可以被转换成 Slave 状态，被另一个资源领养。一个资源可以有多个slave资源，一个slave资源也可以再有自己的slave资源，这样形成一棵树，树根是 Local 状态或 Global 状态。

- Slave 资源的生命周期完全由其 owner 来控制，用户不能主动释放。owner 资源被释放时会自动**先**释放它的所有slave资源。

Local 资源被领养成Slave资源 和 Slave 资源重新变为 Local 资源，都应该是 owner 资源模块提供接口函数在模块内部完成。调用ezGC_Slave2Local ( )需要owner_type，可以把 owner_type 定义为模块内部的 static 静态变量，使得在模块外无法访问它。

Slave资源一般不能在 owner 资源模块外被长期持有，除非 owner 模块提供额外的策略保证。比如在 owner 内部为 Slave 资源增加 ref_count 引用计数，并提供 add_refount() 和 del_refcount() 函数。或者创造一个类似于智能指针的专用GC资源，作为Slave资源的代理，代理资源成为 owner 的Slave资源，删除代理会自动调用del_refcount()。

ezAlloc( ) 分配的内存可以保证最大 `2*sizeof(uintptr_t)`的对齐要求。

### Local 资源自动释放

要解决这个问题，需要先说明一下GC资源的内部实现细节。

每个线程内部都有一个 Local 资源链表和一个 thread_level 整数(初值为0)，每个GC资源中也记录了自己成为 Local 时的当前 thread_Level。`uintptr_t ezGCPoint_insert(void)`返回`++thread_level`。`void ezGCPoint_clean(uintptr_t level)`删除所有大于等于 level 的 Local 资源并使得 `thread_level = level-1`。

下面代码就会删除 ezGCPoint_insert() 和 ezGCPoint_clean() 之间产生的所有Local 资源。

```c
uintptr_t _mem_gc_point = ezGCPoint_insert();
{
    ... ...
}
ezGCPoint_clean(_mem_gc_point);
```

注意：只有这两个函数能该改变 thread_level，它们都是成对出现的，并且可以嵌套多层。即使可能有 longjmp( ) 直接跳出里层的 ezGCPoint_clean( )，也会遇到外层的 ezGCPoint_clean( )，释放里层的 Local 资源。

一般这两个函数不需要用户自己调用，下面的任务系统会在每一轮任务执行时自动调用。当然用户也可以通过插入GCPoint， 在更细的粒度上控制内存回收。

这种自动释放的机制可以保证用户创建的 Local 资源在当前函数中生命周期是明确的，不会因为调用进入了其他未知函数而影响已经创建的 Local 资源。

用户也可以主动调用ezDestroy( )，立即销毁一个 Local 资源或 Global 资源，回收内存。

资源创建时默认是自动释放的，使得漏掉领养代码的bug在正常运行时就可以暴露出来。

尽量使用ezAlloc( )来分配内存，方便管理，但也可以在GC资源模块内部使用传统的malloc方法。有时用GC资源来封装内部的malloc/realloc/free内存管理也是很好的办法。

### GC资源的嵌套类型

一个GC资源可以有多种ezGC_type类型，这些类型的嵌套方式类似于面向对象中的单继承，比如有一个父类型A，类型A必须在设计时就允许自己可以被继承，A的定义和创建函数为

```c
typedef struct {
    ... //A类型的属性
    ezGC_sub sub; //必须放在最后，子类型
} type_A;

static void _gc_A_destroy(void *p) {
    type_A *pa = (type_A *)p;
    ...
    ezGC_destroy_sub(pa->sub);
    ...
}

static ezGC_type _gc_A_type = {
    .destroy_func = _gc_A_destroy,
    .name = "type_A"
};

//sub_size为子类型的大小，sub_type为子类型，省略的参数表示A类型的属性
void * typeA_create(int sub_size, const ezGC_type *sub_type, ...)
{
    type_A *pa = ezAlloc(sub_size+sizeof(type_A), &_gc_A_type, TRUE);
    ...
    ezGC_set_sub(pa, &(pa->sub), sub_type);
    return pa->sub.p; //返回sub类型的指针，同时也具有type_A的类型
}
```

A的创建函数返回的内存指针就同时具有 typeA 和 sub_type 。

如果一个类型B想继承A，它的创建函数可以为：

```c
typedef struct { ... } type_B;
static void _gc_B_destroy(void *p) { ... }
static ezGC_type _gc_B_type = { ... };

//省略的参数表示类型B的属性
void * typeB_create(...)
{
    type_B *pb = typeA_create(sizeof(type_B), &_gc_B_type, TRUE);
    ...
    return pb;
}
```

如果类型B在设计时也允许自己可以被继承，那么它可以仿照上面类型A在自己的定义和创建函数中也提供sub相关成员。这样可以实现多级嵌套或多级单继承。

父类型的创建函数一般返回的是子类型的对象指针，通过下面的函数可以在嵌套类型对象的不同类型指针之间转换。

```c
void * ezGC_check_type(const void *p, const ezGC_type *type);

type_A *p = ezGC_check_type(pb, &_gc_A_type);//从typeB的指针得到typeA的指针
```

如果某种类型的GC资源指针不具有指定类型，返回 NULL 。

嵌套类型在销毁时先调用父类型的销毁函数，在父类型的销毁函数中会调用子类型的销毁函数。

## 2. 任务(task)系统

支持三种任务类型。

### 文件任务

```c
//回调函数类型，ezFunc是封装了回调函数的GC资源类型
typedef void (*ezFuncFunc)(ezFunc *p, intptr_t id, void *data);

//创建文件类型的任务，ezTask也是GC资源类型，并且是ezFunc的子类型
ezTask * ezTask_add_file(const char *group, int fd,
    ezFuncFunc func, void *arg);

void ezTaskFile_start(ezTask *file, ezMask8 type);
void ezTaskFile_stop(ezTask *file);
```

ezTask_add_file( )参数中的 group 是为了把多个任务分组，方便组织任务在多个线程中运行。group 为 NULL 表示在主线程中运行。fd 就是需要监控的文件句柄，arg 是回调函数的一个静态参数，可以在回调函数中通过`void * ezFunc_get_arg(const void *p)`获取。

ezTaskFile_start( )开始监控文件句柄 fd 是否具有指定 type 的事件，如果有，就自动调用回调函数 func 来处理该事件。回调函数的参数`ezFunc *p`是当前文件任务，同时也是ezFunc类型。参数 id 就是文件句柄 fd，`((ezMask8)data)`就是当前具有的事件类型。当前支持如下事件类型，这些事件类型都是掩码，通过 | 运算支持多种。

| 文件事件类型      | 说明                |
| ----------- | ----------------- |
| EZ_FILE_IN  | 文件有数据可读           |
| EZ_FILE_OUT | 文件输出缓冲器有空闲可写      |
| EZ_FILE_PRI | 网络文件有带外数据(紧急数据)可读 |
| EZ_FILE_ERR | 文件出错              |
| EZ_FILE_HUP | 网络文件断开            |

一个文件任务如果已经 start 了, 再次调用 start 可以修改监控的事件类型 type。

注意回调函数不会重入，只有回调函数返回后才再次监控( EPOLLONESHORT)。

### 定时器任务

```c
ezTask * ezTask_add_timer(const char *group, ezFuncFunc func, void *arg);

//ezTme时间以毫秒为单位
//first_delay <= 0表示立即执行；period <= 0表示只执行一次就stop
void ezTaskTimer_start(ezTask *timer, ezTime first_delay, ezTime period);
void ezTaskTimer_stop(ezTask *timer);
```

ezTaskTimer_start( )中，first_delay表示第一次的到期时间间隔，period表示后面的时间间隔。首先根据 first_delay 设定第一次的到期时间，以后就是在时间到期时，根据period设定下一次的到期时间。

一个定时器任务如果已经 start 了, 再次调用 start 可以重置时间。

注意回调函数不会重入，只有回调函数返回后才能根据当前到期时间判断是否执行回调函数。

### 单一任务

```c
ezTask * ezTask_add_single(const char *group, ezFuncFunc func, void *arg);

ezResult ezTaskSingle_trigger(ezTask *single, intptr_t id, void *data);
int ezTaskSingle_get_ReadyNum(ezTask *single);
```

单一任务比较简单，由用户主动触发执行。

ezTaskSingle_trigger( )触发执行一次回调函数，id 和 data 直接就是回调函数的参数。注意data如果不为NULL，那么必须是一个 Local 状态的GC资源。该函数会把 data 转换成自己的Slave 资源。然后在执行回调函数之前再把 data 转换成 Local 状态，之后就不管了。

注意回调函数不会重入，即如果调用ezTaskSingle_trigger( )触发单一任务事件，该任务内部有一个链表保存 id 和 data，在上一次回调函数返回之后才会再次执行。

### 任务状态

以上三种任务有如下几种状态：

| 状态              | 说明           | 文件任务            | 定时器任务           |
| --------------- | ------------ | --------------- | --------------- |
| EZ_TASK_STOP    | 停止           | 创建时<br>stop( )时 | 创建时<br>stop( )时 |
| EZ_TASK_IDLE    | 没有满足运行条件, 等待 | start( )        | start( )        |
| EZ_TASK_READY   | 满足运行条件, 就绪   | 文件有事件           | 到期时间            |
| EZ_TASK_RUN     | 正在运行         | 执行回调函数          | 执行回调函数          |
| EZ_TASK_DESTROY | 被销毁          | GC资源被销毁         | GC资源被销毁         |

注意：单一任务没有 STOP 状态，也没有对应的stop( )函数，创建后就是 IDLE 状态。

调用下面的函数进入任务的事件循环：

```c
/*创建一个任务组, 同组的任务保证在同一个线程中执行。
flags等于1表示创建一个线程来执行该组任务。
flags等于0表示在主线程中执行该组任务*/
ezResult ezTask_create_groud(const char *group, uint32_t flags);

/*主线程调用，进入事件循环，该函数不返回*/
void ezTask_run(void);
```

当调用ezTask_create_groud( )创建一个任务组时，flags等于1会导致内部新创建一个线程，该线程会自动进入事件循环，不退出。

## 3. 多任务协作

多个任务之间可以有同步和互斥的关系，互斥就是如果多个任务在不同的线程中运行，那么它们不能同时访问相同的内存资源。互斥通过 EZ_MUTEX 来实现，EZ_MUTEX 就是 pthread_mutex_t 的封装。

同步就是一个任务运行到某个时间点，必须阻塞等待某个条件被满足，才能继续往下运行。这个条件往往是另外一些任务来完成的。任务通过 ezCond 来实现，ezCond 实现原理类似于 pthread_cond_t，但是完全没有用到 pthread_cond_t。

```c
ezCond * ezTaskCond_create(void);
//timeout以毫秒为单位，timeout <=0 表示永远等待
void ezTaskCond_wait(ezCond *pcond, EZ_MUTEX *pmutex, ezTime timeout);
void ezTaskCond_waken(ezCond *pcond, ezBool broadcast);
```

ezTaskCond_wait( )必须在任务代码中被调用，并且总是和一个 EZ_MUTEX 搭配使用，ezCond 内部没有自己的锁。ezTaskCond_waken( ) 也应该在 EZ_MUTEX 解锁前调用。

ezTaskCond_wait( )在阻塞等待过程中仍然会运行事件循环，这里没有用到协程，唯一的缺点就是：当嵌套调用ezTaskCond_wait( )时，即使外层的条件已经满足，也必须等待内层的条件满足并执行完，外层的任务才能继续执行。以后有时间再用协程实现吧。

目前可以使用单一任务来分解复杂任务，尽量避免使用 ezCond。

## 4. 信号(signal)系统

这里的信号 signal 就是Linux系统中的信号，类似于用户态的软中断。Linux系统中的信号处理函数可能在任意时刻进入，会导致它调用的所有函数都可能重入，所以操作系统规定了在信号处理函数中只能调用很少的信号安全的函数。

我们这里封装了操作系统的信号，转换成在主线程的事件循环中和任务串行调用，信号处理函数中没有调用函数限制，并且也不会重入。

```c
typedef void (*ezSignalFunc)(int sig, void *arg);
void ezSignal(int sig, ezSignalFunc handler, void *arg);

typedef void (*ezChildSignalFunc)(pid_t pid, int status, void *arg);
void ezSignal_child(pid_t pid, ezChildSignalFunc handler, void *arg);
```

sig 就是操作系统的信号，handler 是函数指针，arg是信号处理函数的参数。

当 handler 为 NULL 时，等价于 `signal(sig, SIG_IGN)`。

当 handler 为 -1 时，等价于 `signal(sig，SIG_DFL)`。

ezSignal( ) 不支持 SIGCHLD 信号。ezSignal_child( ) 只支持 SIGCHLD 信号。
