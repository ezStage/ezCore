#include <ezCore/ezCore.h>

static ezTask *file, *timer, *single;
static int pipe_fd[2]; //fd[0] for read, fd[1] for write

/*task回调函数内部创建的local资源, 在回调函数返回后会自动销毁*/
static int timer_n;
static void _timer_func(ezFunc *p, intptr_t id, void *data)
{
	char buf[100];
	int n = sprintf(buf, "timer send pipe %d", ++timer_n);
	ez_write(pipe_fd[1], buf, n);

	if(timer_n == 3) {
		ezTaskSingle_trigger(single, timer_n, NULL);
	}
}

static void _pipe_read_func(ezFunc *p, intptr_t id, void *data)
{
	int fd = EZ_FUNC_FILE_FD(id); //pipe_fd[0]
	char buf[100];
	ssize_t n = read(fd, buf, 100);
	buf[n] = '\0';
	printf("file read len=%zd: %s\n", n, buf);
}

static void _single_func(ezFunc *p, intptr_t id, void *data)
{
	printf("single trigger id=%zd, data=%p\n", id, data);

	/*自动释放所有local和global资源, 并exit()*/
	ezGC_exit(0);
}

int main(int argc, char *argv[])
{
	if(pipe(pipe_fd) == 0) {
		/*task file最好是无阻塞的*/
		fcntl(pipe_fd[0], F_SETFL, O_NONBLOCK);
		fcntl(pipe_fd[1], F_SETFL, O_NONBLOCK);
	} else {
		ezLog_err("pipe\n");
		return FALSE;
	}

	timer = ezTask_add_timer(NULL, _timer_func, NULL);
	file = ezTask_add_file(NULL, pipe_fd[0], _pipe_read_func, NULL);
	single = ezTask_add_single(NULL, _single_func, NULL);

	//开始监控file是否可读, 如果可读调用_pipe_read_func()
	ezTaskFile_start(file, EZ_FILE_IN);

	//开始启动timer, 第一次到时0.5s, 后面周期1s
	ezTaskTimer_start(timer, 500, 1000);

	//进入事件循环
	//注意该函数之前创建的local资源不会自动释放,
	//比如上面的file, timer, single创建时默认都是local资源.
	ezTask_run();
	return 0;
}

