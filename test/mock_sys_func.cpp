#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>

#include "simple_log.h"

ssize_t read(int fd, void *buf, size_t count) {
    LOG_INFO("mock for read func");
    return 0;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    LOG_INFO("mock for recv func");
    return -1;
}
 
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    LOG_INFO("mock for epoll_ctl func");
    return 0;
}

int close(int fd) {
    LOG_INFO("mock for close, fd:%d", fd);
    return 0;
}

int epoll_wait(int epfd, struct epoll_event *events,
        int maxevents, int timeout) {
    LOG_INFO("mock for epoll_wait, epfd:%d", epfd);
    errno = EBADF;
    return -1;
}

