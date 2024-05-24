
#include <sys/epoll.h>

typedef struct {
	int epoll_fd;
	int waken_pipe[2]; /*pipe[0]:read, [1]:write*/
} _ezOS_mux; /*操作系统多路复用: ezCore内部接口*/

typedef struct {
	int fd;
	uint32_t epoll_type;
} _ezOS_mux_file;

