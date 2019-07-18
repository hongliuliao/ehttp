#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

int fcntl(int fd, int cmd, ... /* arg */ ) {
    LOG_INFO("mock for fcntl:%d", fd);
    return 0;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    LOG_INFO("mock for accept:%d", sockfd);
    return 0;
}

char *inet_ntoa(struct in_addr in) {
    LOG_INFO("mock for inet_ntoa");
    size_t ip_size = 20;
    char *local = new char[ip_size];
    memset(local, 0, ip_size);
    snprintf(local, ip_size, "%s", "127.0.0.1");
    return local;
}

