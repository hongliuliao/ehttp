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
#include <sys/sysinfo.h>
#include <unistd.h>

#include "simple_log.h"
#include "epoll_socket.h"

EpollSocket::EpollSocket() {
    _thread_pool = NULL;
    _use_default_tp = true;
    _status = S_RUN;
    _epollfd = -1;
    _clients = 0;
    pthread_mutex_init(&_client_lock, NULL);
}

EpollSocket::~EpollSocket() {
    if (_thread_pool != NULL && _use_default_tp) {
        delete _thread_pool;
        _thread_pool = NULL;
    }
}

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

    pthread_mutex_lock(&_client_lock);
    _clients++;
    pthread_mutex_unlock(&_client_lock);
    LOG_DEBUG("get accept socket which listen fd:%d, conn_sock_fd:%d", sockfd, conn_sock);

    EpollContext *epoll_context = new EpollContext();
    epoll_context->fd = conn_sock;
    epoll_context->client_ip = client_ip;

    socket_handler.on_accept(*epoll_context);

    struct epoll_event conn_sock_ev;
    conn_sock_ev.events = EPOLLIN | EPOLLONESHOT;
    conn_sock_ev.data.ptr = epoll_context;

    if (epoll_ctl(_epollfd, EPOLL_CTL_ADD, conn_sock, &conn_sock_ev) == -1) {
        LOG_ERROR("epoll_ctl: conn_sock:%s", strerror(errno));
        close_and_release(event);
        return -1;
    }
    return 0;
}

void read_func(void *data) {
    TaskData *td = (TaskData *) data;
    td->es->handle_readable_event(td->event);
    delete td;
}

int EpollSocket::handle_readable_event(epoll_event &event) {
    EpollContext *epoll_context = (EpollContext *) event.data.ptr;
    int fd = epoll_context->fd;

    int ret = _watcher->on_readable(_epollfd, event);
    if (ret == READ_CLOSE) {
        return close_and_release(event);
    }
    if (ret == READ_CONTINUE) {
        event.events = EPOLLIN | EPOLLONESHOT;
        epoll_ctl(_epollfd, EPOLL_CTL_MOD, fd, &event);
    } else if (ret == READ_OVER) { // READ_OVER
        event.events = EPOLLOUT | EPOLLONESHOT;
        epoll_ctl(_epollfd, EPOLL_CTL_MOD, fd, &event);
    } else {
        LOG_ERROR("unkonw read ret:%d", ret);
    }
    return 0;
}

int EpollSocket::handle_writeable_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_handler) {
    EpollContext *epoll_context = (EpollContext *) event.data.ptr;
    int fd = epoll_context->fd;
    LOG_DEBUG("start write data");

    int ret = socket_handler.on_writeable(*epoll_context);
    if(ret == WRITE_CONN_CLOSE) {
        return close_and_release(event);
    }

    if (ret == WRITE_CONN_CONTINUE) {
        event.events = EPOLLOUT | EPOLLONESHOT;
    } else if (ret == WRITE_CONN_ALIVE) {
        if (_status == S_REJECT_CONN) {
            return close_and_release(event);
        }
        // wait for next request
        event.events = EPOLLIN | EPOLLONESHOT;
    } else {
        LOG_ERROR("unkonw write ret:%d", ret);
    }
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);

    return 0;
}

void EpollSocket::set_thread_pool(ThreadPool *tp) {
    this->_thread_pool = tp;
    _use_default_tp = false;
}

void EpollSocket::set_port(int port) {
    _port = port;
}

void EpollSocket::set_watcher(EpollSocketWatcher *w) {
    _watcher = w;
}

void EpollSocket::set_backlog(int backlog) {
    _backlog = backlog;
}

void EpollSocket::set_max_events(int me) {
    _max_events = me;
}

int EpollSocket::init_default_tp() {
    int core_size = get_nprocs();
    LOG_INFO("thread pool not set, we will build for core size:%d", core_size);
    _thread_pool = new ThreadPool();
    _thread_pool->set_pool_size(core_size);

    return 0;
}

int EpollSocket::add_listen_sock_to_epoll() {
    for (std::set<int>::iterator i = _listen_sockets.begin(); i != _listen_sockets.end(); i++) {
        int sockfd = *i;
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = sockfd;
        if (epoll_ctl(_epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
            LOG_ERROR("epoll_ctl: listen_sock:%s", strerror(errno));
            return -1;
        }
    }
    return 0;
}

int EpollSocket::handle_event(epoll_event &e) {
    if (_listen_sockets.count(e.data.fd)) {
        // accept connection
        if (_status == S_RUN) {
            this->handle_accept_event(_epollfd, e, *_watcher);
        } else {
            LOG_INFO("current status:%d, not accept new connect", _status);
            pthread_mutex_lock(&_client_lock);
            if (_clients == 0 && _status == S_REJECT_CONN) {
                _status = S_STOP;
                LOG_INFO("client is empty and ready for stop server!");
            }
            pthread_mutex_unlock(&_client_lock);
        }
    } else if (e.events & EPOLLIN) {
        // handle readable async
        LOG_DEBUG("start handle readable event");
        TaskData *tdata = new TaskData();
        tdata->event = e;
        tdata->es = this;

        Task *task = new Task(read_func, tdata);
        int ret = _thread_pool->add_task(task);
        if (ret != 0) {
            LOG_WARN("add read task fail:%d, we will close connect.", ret);
            close_and_release(e);
            delete tdata;
            delete task;
        }
    } else if (e.events & EPOLLOUT) {
        // writeable
        this->handle_writeable_event(_epollfd, e, *_watcher);
    } else {
        LOG_INFO("unkonw events :%d", e.events);
    }
    return 0;
}

int EpollSocket::create_epoll() {
    // The "size" parameter is a hint specifying the number of file
    // descriptors to be associated with the new instance.
    _epollfd = epoll_create(1024);
    if (_epollfd == -1) {
        LOG_ERROR("epoll_create:%s", strerror(errno));
        return -1;
    }
    return 0;
}

int EpollSocket::init_tp() {
    if (_thread_pool == NULL) {
        init_default_tp();
    }
    int ret = _thread_pool->start();
    return ret;
}

int EpollSocket::start_event_loop() {
    epoll_event *events = new epoll_event[_max_events];
    while (_status != S_STOP) {
        int fds_num = epoll_wait(_epollfd, events, _max_events, -1);
        if (fds_num == -1) {
            if (errno == EINTR) { /*The call was interrupted by a signal handler*/
                continue;
            }
            LOG_ERROR("epoll_wait error:%s", strerror(errno));
            break;
        }

        for (int i = 0; i < fds_num; i++) {
            this->handle_event(events[i]);
        }
    }
    LOG_INFO("epoll wait loop stop ...");
    if (events != NULL) {
        delete[] events;
        events = NULL;
    }
    return 0;
}

int EpollSocket::start_epoll() {
    int ret = init_tp();
    CHECK_RET(ret, "thread pool start error:%d", ret);

    ret = this->listen_on();
    CHECK_RET(ret, "listen err:%d", ret);

    ret = this->create_epoll();
    CHECK_RET(ret, "create epoll err:%d", ret);

    ret = this->add_listen_sock_to_epoll();
    CHECK_RET(ret , "add listen sock to epoll fail:%d", ret);

    return start_event_loop();
}

int EpollSocket::stop_epoll() {
    _status = S_REJECT_CONN;
    LOG_INFO("stop epoll , current clients:%u", _clients);
    return 0;
}

int EpollSocket::close_and_release(epoll_event &epoll_event) {
    if (epoll_event.data.ptr == NULL) {
        return 0;
    }
    LOG_DEBUG("connect close");

    EpollContext *hc = (EpollContext *) epoll_event.data.ptr;
    _watcher->on_close(*hc);

    int fd = hc->fd;
    epoll_event.events = EPOLLIN | EPOLLOUT;
    epoll_ctl(_epollfd, EPOLL_CTL_DEL, fd, &epoll_event);

    delete (EpollContext *) epoll_event.data.ptr;
    epoll_event.data.ptr = NULL;

    int ret = close(fd);

    pthread_mutex_lock(&_client_lock);
    _clients--;
    if (_clients == 0 && _status == S_REJECT_CONN) {
        _status = S_STOP;
        LOG_INFO("client is empty and ready for stop server!");
    }
    pthread_mutex_unlock(&_client_lock);

    LOG_DEBUG("connect close complete which fd:%d, ret:%d", fd, ret);
    return ret;
}

void EpollSocket::add_bind_ip(std::string ip) {
    _bind_ips.push_back(ip);
}
