/*
 * tcp_epoll.cpp
 *
 *  Created on: Nov 10, 2014
 *      Author: liao
 */
#include <cstdlib>
#include <climits>
#include <cstdio>
#include <cerrno>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>

#include "simple_log.h"
#include "epoll_socket.h"

int EpollSocket::setNonblocking(int fd) {
    int flags;

    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
};

int EpollSocket::bind_on(unsigned int ip) {
    /* listen on sock_fd, new connection on new_fd */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) {
        LOG_ERROR("socket error:%s", strerror(errno));
        return -1;
    }
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in my_addr; /* my address information */
    memset (&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET; /* host byte order */
    my_addr.sin_port = htons(_port); /* short, network byte order */
    my_addr.sin_addr.s_addr = ip;    

    if (bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1) {
        LOG_ERROR("bind error:%s", strerror(errno));
        return -1;
    }
    if (listen(sockfd, _backlog) == -1) {
        LOG_ERROR("listen error:%s", strerror(errno));
        return -1;
    }
    _listen_sockets.insert(sockfd);
    return 0;
}

int EpollSocket::listen_on() {
    if (_bind_ips.empty()) {
        int ret = bind_on(INADDR_ANY); /* auto-fill with my IP */
        if (ret != 0) {
            return ret;
        }
        LOG_INFO("bind for all ip (0.0.0.0)!");
    } else {
        for (size_t i = 0; i < _bind_ips.size(); i++) {
            unsigned ip = inet_addr(_bind_ips[i].c_str()); 
            int ret = bind_on(ip);
            if (ret != 0) {
                return ret;
            }
            LOG_INFO("bind for ip:%s success", _bind_ips[i].c_str());
        }
    }

    LOG_INFO("start Server Socket on port : %d", _port);
    return 0;
}

int EpollSocket::accept_socket(int sockfd, std::string &client_ip) {
    int new_fd;
    struct sockaddr_in their_addr; /* connector's address information */
    socklen_t sin_size = sizeof(struct sockaddr_in);

    if ((new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size)) == -1) {
        LOG_ERROR("accept error:%s", strerror(errno));
        return -1;
    }

    client_ip = inet_ntoa(their_addr.sin_addr);
    LOG_DEBUG("server: got connection from %s\n", client_ip.c_str());
    return new_fd;
}

int EpollSocket::handle_accept_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_handler) {
    int sockfd = event.data.fd;

    std::string client_ip;
    int conn_sock = accept_socket(sockfd, client_ip);
    if (conn_sock == -1) {
        return -1;
    }
    setNonblocking(conn_sock);
    LOG_DEBUG("get accept socket which listen fd:%d, conn_sock_fd:%d", sockfd, conn_sock);

    EpollContext *epoll_context = new EpollContext();
    epoll_context->fd = conn_sock;
    epoll_context->client_ip = client_ip;

    socket_handler.on_accept(*epoll_context);

    struct epoll_event conn_sock_ev;
    conn_sock_ev.events = EPOLLIN;
    conn_sock_ev.data.ptr = epoll_context;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &conn_sock_ev) == -1) {
        LOG_ERROR("epoll_ctl: conn_sock:%s", strerror(errno));
        return -1;
    }

    return 0;
}

void read_func(void *data) {
    TaskData *tdata = (TaskData *)data;
    int epollfd = tdata->epollfd;
    epoll_event event = (tdata->event);
    EpollSocketWatcher &socket_handler = *(tdata->watcher);

    EpollContext *epoll_context = (EpollContext *) event.data.ptr;
    int fd = epoll_context->fd;

    int ret = socket_handler.on_readable(epollfd, event);
    if (ret == READ_CLOSE) {
        close_and_release(epollfd, event, socket_handler);
        delete tdata;
        return;
    }
    if (ret == READ_CONTINUE) {
        event.events = EPOLLIN;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    } else if (ret == READ_OVER) { // READ_OVER
        event.events = EPOLLOUT;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    } else {
        LOG_ERROR("unkonw ret!");
    }
    delete tdata;
}

int EpollSocket::handle_readable_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_handler) {
    EpollContext *epoll_context = (EpollContext *) event.data.ptr;
    int fd = epoll_context->fd;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &event); // TODO remove event or use EPOLLET?

    TaskData *tdata = new TaskData();
    tdata->epollfd = epollfd;
    tdata->event = event;
    tdata->watcher = &socket_handler;

    Task *task = new Task(read_func, tdata);
    int ret = _thread_pool->add_task(task);
    if (ret != 0) {
        LOG_WARN("add read task fail:%d, we will close connect.", ret);
        close_and_release(epollfd, event, socket_handler);
    }
    return ret;
}

void write_func(void *data) {
    TaskData *tdata = (TaskData *)data;
    int epollfd = tdata->epollfd;
    epoll_event event = (tdata->event);
    EpollSocketWatcher &socket_handler = *(tdata->watcher);

    EpollContext *epoll_context = (EpollContext *) event.data.ptr;
    int fd = epoll_context->fd;
    LOG_DEBUG("start write data");

    int ret = socket_handler.on_writeable(*epoll_context);
    if(ret == WRITE_CONN_CLOSE) {
        close_and_release(epollfd, event, socket_handler);
        delete tdata;
        return;
    }

    if (ret == WRITE_CONN_CONTINUE) {
        event.events = EPOLLOUT;
    } else {
        event.events = EPOLLIN;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    delete tdata;
}

int EpollSocket::handle_writeable_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_handler) {
    EpollContext *epoll_context = (EpollContext *) event.data.ptr;
    int fd = epoll_context->fd;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &event);

    TaskData *tdata = new TaskData();
    tdata->epollfd = epollfd;
    tdata->event = event;
    tdata->watcher = &socket_handler;

    Task *task = new Task(write_func, tdata);
    int ret = _thread_pool->add_task(task);
    if (ret != 0) {
        LOG_WARN("add write task fail:%d, we will close connect.", ret);
        close_and_release(epollfd, event, socket_handler);
    }
    return ret;
}

EpollSocket::EpollSocket() {}

void EpollSocket::set_thread_pool(ThreadPool *tp) {
    this->_thread_pool = tp;
}

int EpollSocket::start_epoll(int port, EpollSocketWatcher &socket_handler, int backlog, int max_events) {
    _backlog = backlog;
    _port = port;

    if (_thread_pool == NULL) {
        LOG_ERROR("Please set thread pool first .");
        return -1;
    }
    int ret = this->listen_on();
    if (ret != 0) {
        return ret;
    }

    // The "size" parameter is a hint specifying the number of file
    // descriptors to be associated with the new instance. 
    int epollfd = epoll_create(1024);
    if (epollfd == -1) {
        LOG_ERROR("epoll_create:%s", strerror(errno));
        return -1;
    }

    for (std::set<int>::iterator i = _listen_sockets.begin(); i != _listen_sockets.end(); i++) {
        int sockfd = *i;
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = sockfd;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
            LOG_ERROR("epoll_ctl: listen_sock:%s", strerror(errno));
            return -1;
        }
    }

    epoll_event *events = new epoll_event[max_events];
    while (1) {
        int fds_num = epoll_wait(epollfd, events, max_events, -1);
        if (fds_num == -1) {
            if (errno == EINTR) { /*The call was interrupted by a signal handler*/
                continue;
            }
            LOG_ERROR("epoll_pwait:%s", strerror(errno));
            break;
        }

        for (int i = 0; i < fds_num; i++) {
            if(_listen_sockets.count(events[i].data.fd)) {
                // accept connection
                this->handle_accept_event(epollfd, events[i], socket_handler);
            } else if(events[i].events & EPOLLIN ){
                // readable
                this->handle_readable_event(epollfd, events[i], socket_handler);
            } else if(events[i].events & EPOLLOUT) {
                // writeable
                this->handle_writeable_event(epollfd, events[i], socket_handler);
            } else {
                LOG_INFO("unkonw events :%d", events[i].events);
            }
        }
    }
    if (events != NULL) {
        delete[] events;
        events = NULL;
    }
    return -1;
}

int close_and_release(int &epollfd, epoll_event &epoll_event, EpollSocketWatcher &socket_handler) {
    if (epoll_event.data.ptr == NULL) {
        return 0;
    }
    LOG_DEBUG("connect close");

    EpollContext *hc = (EpollContext *) epoll_event.data.ptr;
    socket_handler.on_close(*hc);

    int fd = hc->fd;
    epoll_event.events = EPOLLIN | EPOLLOUT;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &epoll_event);

    delete (EpollContext *) epoll_event.data.ptr;
    epoll_event.data.ptr = NULL;

    int ret = close(fd);
    LOG_DEBUG("connect close complete which fd:%d, ret:%d", fd, ret);
    return ret;
}

void EpollSocket::add_bind_ip(std::string ip) {
    _bind_ips.push_back(ip);
}
