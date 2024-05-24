
#define EPOLL_EVENT_NUM 60
static ezBool _ezOS_mux_init(_ezOS_mux *pm)
{
	int waken_pipe[2];

	if(pipe(waken_pipe) == 0) {
		fcntl(waken_pipe[0], F_SETFL, O_NONBLOCK);
		fcntl(waken_pipe[1], F_SETFL, O_NONBLOCK);
	} else {
		ezLog_err("pipe\n");
		return FALSE;
	}

	pm->epoll_fd = epoll_create(EPOLL_EVENT_NUM);
	if(pm->epoll_fd == -1) {
		ezLog_err("epoll_create\n");
		close(waken_pipe[0]);
		close(waken_pipe[1]);
		return FALSE;
	}

	struct epoll_event epe;
	epe.events = EPOLLIN; /*没有EPOLLONESHOT, 后面不需要active*/
	epe.data.ptr = NULL;
	epoll_ctl(pm->epoll_fd, EPOLL_CTL_ADD, waken_pipe[0], &epe);

	pm->waken_pipe[0] = waken_pipe[0];
	pm->waken_pipe[1] = waken_pipe[1];
	return TRUE;
}

static void _ezOS_mux_uinit(_ezOS_mux *pm)
{
	epoll_ctl(pm->epoll_fd, EPOLL_CTL_DEL, pm->waken_pipe[0], NULL);
	close(pm->waken_pipe[0]);
	close(pm->waken_pipe[1]);
	close(pm->epoll_fd);
}

static void _ezOS_mux_add_file(_ezOS_mux *pm, ezTaskFile *pfile, int fd, ezMask8 type)
{
	/*HUP: 远程挂断*/
	uint32_t etype = EPOLLHUP | EPOLLERR | EPOLLONESHOT;
	if(type&EZ_FILE_PRI) etype |= EPOLLPRI;
	if(type&EZ_FILE_IN) etype |= EPOLLIN;
	if(type&EZ_FILE_OUT) etype |= EPOLLOUT;

	struct epoll_event epe;
	epe.events = etype;
	epe.data.ptr = pfile;
	epoll_ctl(pm->epoll_fd, EPOLL_CTL_ADD, fd, &epe);

	pfile->os_mux_file.fd = fd;
	pfile->os_mux_file.epoll_type = etype;
}

/*EPOLLONESHOT, 每次激活都需要*/
static void _ezOS_mux_active_file(_ezOS_mux *pm, ezTaskFile *pfile)
{
	int fd = pfile->os_mux_file.fd;
	uint32_t etype = pfile->os_mux_file.epoll_type;

	struct epoll_event epe;
	epe.events = etype;
	epe.data.ptr = pfile;
	epoll_ctl(pm->epoll_fd, EPOLL_CTL_MOD, fd, &epe);
}

static void _ezOS_mux_del_file(_ezOS_mux *pm, ezTaskFile *pfile)
{
	int fd = pfile->os_mux_file.fd;
	epoll_ctl(pm->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

/*该函数会在信号处理中调用*/
static void _ezOS_mux_waken(_ezOS_mux *pm)
{
	char w = 'w';
	write(pm->waken_pipe[1], &w, 1);
}

static void _ezOS_mux_check_file(my_thread *pt, ezTime timeout)
{
	struct epoll_event epe[EPOLL_EVENT_NUM];
	int e, n;
	int epoll_timeout;
	ezTaskFile *pfile;

	/*timeout表示等待的时间段*/
	/*epoll_timeout: -1 is forever, 0 is at once */
	if(timeout < 0)
		epoll_timeout = -1; /* forever */
	/*如果ezTime 整数类型范围大于int, 如果溢出*/
	else if(timeout > INT_MAX)
		epoll_timeout = INT_MAX;
	else
		epoll_timeout = timeout;

	/*-----------------------------------------------------------*/
	EZ_MUTEX_UNLOCK(&(pt->lock));
	errno = 0;
	e = epoll_wait(pt->os_mux.epoll_fd, epe, EPOLL_EVENT_NUM, epoll_timeout);
	EZ_MUTEX_LOCK(&(pt->lock));
	/*-----------------------------------------------------------*/

	if((e<0)&&(errno != EINTR)&&(errno != EAGAIN))
	{
		ezLog_errno("epoll_wait\n");
		ez_sleep(100);
		return;
	}

	for(n=0; n<e; ++n)
	{
		pfile = (ezTaskFile *)(epe[n].data.ptr);
		if(pfile == NULL)
		{
			char tempc[4];
			while(read(pt->os_mux.waken_pipe[0], tempc, 4) == 4)
				;
			/*waken_pipe不需要active*/
			continue;
		}

		if(pfile->common.task_status == EZ_TASK_IDLE)
		{
			ezMask8 type = 0;
			uint32_t etype = epe[n].events;
			if(etype&EPOLLPRI) type |= EZ_FILE_PRI;
			if(etype&EPOLLIN) type |= EZ_FILE_IN;
			if(etype&EPOLLOUT) type |= EZ_FILE_OUT;
			if(etype&EPOLLHUP) type |= EZ_FILE_HUP;
			if(etype&EPOLLERR) type |= EZ_FILE_ERR;

			pfile->ready_type = type;
			pfile->common.task_status = EZ_TASK_READY;
			ezList_del(&(pfile->common.thread_list));
			ezList_add_tail(&(pfile->common.thread_list), &(pt->ready_head));
		}
	}
	return;	
}

