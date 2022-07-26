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
#include <sstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <signal.h>

#include "json/json.h"

#include "simple_log.h"
#include "epoll_socket.h"

static long long g_client_id = 0;

EpollContext::EpollContext() {
    fd = -1;
    _last_interact_time = 0;
    _ctx_status = 0;
    _id = -1;
}

std::string EpollContext::to_string() {
    Json::Value root;
    root["fd"] = fd;
    root["idle"] = (long long)time(NULL) - _last_interact_time;
    Json::FastWriter writer;
    return writer.write(root);
}

EpollSocket::EpollSocket() {
    _thread_pool = NULL;
    _use_default_tp = true;
    _status = S_INIT;
    _epollfd = -1;
    _clients = 0;
    _max_idle_sec = 0;
    _watcher = NULL;
    pthread_mutex_init(&_client_lock, NULL);
}

EpollSocket::~EpollSocket() {
    if (_thread_pool != NULL && _use_default_tp) {
        delete _thread_pool;
        _thread_pool = NULL;
    }
}

int EpollSocket::get_epfd(){
    return _epollfd;
}

EpollSocketWatcher *EpollSocket::get_watcher(){ 
    return _watcher;
}

void write_func(void *data) {
    TaskData *td = (TaskData *) data;
    EpollSocketWatcher *watcher = td->es->get_watcher();
    int ep_fd = td->es->get_epfd();
    td->es->handle_writeable_event(ep_fd, td->event, *watcher);

    EpollContext *hc = (EpollContext *) td->event.data.ptr;
    if (hc != NULL) {
        hc->_ctx_status = CONTEXT_WRITE_OVER;
    }

    delete td;
}

int EpollSocket::multi_thread_handle_write_event(epoll_event &e) {
    EpollContext *hc = (EpollContext *) e.data.ptr;
    if (hc == NULL) {
        LOG_WARN("Get epoll context fail in write event!");
        return -1;
    }
    hc->_ctx_status = CONTEXT_WRITING;
    
    update_interact_time(hc, time(NULL));

    TaskData *tdata = new TaskData();
    tdata->event = e;
    tdata->es = this;

    Task *task = new Task(write_func, tdata);
    int ret = _thread_pool->add_task(task);
    if (ret != 0) {
        close_and_release(e);
        delete tdata;
        delete task;
    }

    return 0;
}

int EpollSocket::set_nonblocking(int fd) {
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

int EpollSocket::add_client(EpollContext *ctx) {
    pthread_mutex_lock(&_client_lock);
    _clients++;
    _eclients[ctx->_id] = ctx;
    pthread_mutex_unlock(&_client_lock);
    return 0;
}

EpollContext *EpollSocket::create_client(int conn_sock, const std::string &client_ip) {
    EpollContext *epoll_context = new EpollContext();
    epoll_context->fd = conn_sock;
    epoll_context->_client_ip = client_ip;
    epoll_context->_last_interact_time = time(NULL);
    g_client_id++;
    epoll_context->_id = g_client_id;
    return epoll_context;
}

int EpollSocket::handle_accept_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_handler) {
    int sockfd = event.data.fd;

    std::string client_ip;
    int conn_sock = accept_socket(sockfd, client_ip);
    if (conn_sock == -1) {
        return -1;
    }
    set_nonblocking(conn_sock);
    EpollContext *epoll_context = create_client(conn_sock, client_ip);
    
    LOG_DEBUG("get accept socket which listen fd:%d, conn_sock_fd:%d", sockfd, conn_sock);

    add_client(epoll_context);
    socket_handler.on_accept(*epoll_context);

    struct epoll_event conn_sock_ev;
    conn_sock_ev.events = EPOLLIN | EPOLLONESHOT;
    conn_sock_ev.data.ptr = epoll_context;

    if (epoll_ctl(_epollfd, EPOLL_CTL_ADD, conn_sock, &conn_sock_ev) == -1) {
        LOG_ERROR("epoll_ctl: conn_sock:%s", strerror(errno));
        close_and_release(conn_sock_ev);
        return -1;
    }
    return 0;
}

void read_func(void *data) {
    TaskData *td = (TaskData *) data;
    td->es->handle_readable_event(td->event);

    EpollContext *hc = (EpollContext *) td->event.data.ptr;
    if (hc != NULL) {
        hc->_ctx_status = CONTEXT_READ_OVER;
    }
    delete td;
}

int EpollSocket::handle_readable_event(epoll_event &event) {
    EpollContext *epoll_context = (EpollContext *) event.data.ptr;
    if (epoll_context == NULL) {
        LOG_WARN("Get context from read event fail!");
        return -1;
    }
    int fd = epoll_context->fd;

    int ret = _watcher->on_readable(_epollfd, event);
    if (ret == READ_CLOSE) {
        return close_and_release(event);
    }

    if (ret == READ_CONTINUE) {
        event.events = EPOLLIN | EPOLLONESHOT;
        ret = epoll_ctl(_epollfd, EPOLL_CTL_MOD, fd, &event);
    } else if (ret == READ_OVER) { // READ_OVER
        event.events = EPOLLOUT | EPOLLONESHOT;
        ret = epoll_ctl(_epollfd, EPOLL_CTL_MOD, fd, &event);
    } else {
        LOG_ERROR("unkonw read ret:%d", ret);
    }
    return ret;
}

int EpollSocket::handle_writeable_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_handler) {
    EpollContext *epoll_context = (EpollContext *) event.data.ptr;
    if (epoll_context == NULL) {
        LOG_WARN("Get epoll context fail in write event!");
        return -1;
    }
    int fd = epoll_context->fd;
    LOG_DEBUG("start write data");

    int ret = socket_handler.on_writeable(*epoll_context);
    if (ret == WRITE_CONN_CLOSE) {
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

int EpollSocket::update_interact_time(EpollContext *ctx, time_t t) {
    pthread_mutex_lock(&_client_lock);
    ctx->_last_interact_time = t;
    pthread_mutex_unlock(&_client_lock);
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

int EpollSocket::set_client_max_idle_time(int sec) {
    _max_idle_sec = sec;
    return 0;
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

int EpollSocket::multi_thread_handle_read_event(epoll_event &e) {
    // handle readable async
    LOG_DEBUG("start handle readable event");
    EpollContext *hc = (EpollContext *) e.data.ptr;
    if (hc == NULL) {
        LOG_WARN("Get epoll context fail in read event!");
        return -1;
    }
    hc->_ctx_status = CONTEXT_READING;

    update_interact_time(hc, time(NULL));

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
    return 0;
}

int EpollSocket::handle_event(epoll_event &e) {
    int ret = 0;
    if (_listen_sockets.count(e.data.fd)) {
        if (_status != S_RUN) {
            LOG_WARN("current status:%d, not accept new connect", _status);
            return -1;
        } 
        // accept connection
        ret = this->handle_accept_event(_epollfd, e, *_watcher);
    } else if (e.events & EPOLLIN) {
        // readable
        ret = this->multi_thread_handle_read_event(e);
    } else if (e.events & EPOLLOUT) {
        // writeable
        if (_watcher != NULL) {
            ret = multi_thread_handle_write_event(e);
        }
    } else {
        LOG_INFO("unkonw events :%d", e.events);
        ret = -1;
    }
    return ret;
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
    int ret = _thread_pool->start_threadpool();
    return ret;
}

int EpollSocket::clear_idle_clients() {
    if (_max_idle_sec <= 0) {
        return 0;
    }
    if (_eclients.empty()) {
        return 0;
    }
    time_t timeout_ts = time(NULL) - _max_idle_sec;

    std::vector<epoll_event> remove_evs;
    pthread_mutex_lock(&_client_lock);
    std::map<long long, EpollContext *>::iterator it = _eclients.begin();
    for (; it != _eclients.end(); it++) {
         //long long id = it->first;
         EpollContext *ctx = it->second;
         if (ctx->_last_interact_time <= timeout_ts && 
                 ctx->_ctx_status != CONTEXT_READING && ctx->_ctx_status != CONTEXT_WRITING) {
             LOG_DEBUG("find idle client fd:%d", ctx->fd);
             epoll_event e;
             memset(&e, 0, sizeof(e));
             e.data.ptr = ctx;
             remove_evs.push_back(e);
         } else {
             LOG_DEBUG("this client is using, skip it!");
         }
    }
    pthread_mutex_unlock(&_client_lock);

    int cnt = 0;
    for (size_t i = 0; i < remove_evs.size(); i++) {
        close_and_release(remove_evs[i]);
        cnt++;
    }
    if (cnt > 0) {
        LOG_INFO("find idle clients cnt:%d", cnt);
    }
    return cnt;
}

int EpollSocket::get_clients_info(std::stringstream &ss) {
    Json::FastWriter writer;
    Json::Value root;
    std::map<long long, EpollContext *>::iterator it = _eclients.begin();
    int i = 0;
    for (; it != _eclients.end(); it++) {
        root[i]["fd"] = (*it).second->fd;
        root[i]["last_interact_time"] = (long long)(*it).second->_last_interact_time;
        i++;
    }
    ss << writer.write(root);
    return 0;
}

int EpollSocket::start_event_loop() {
    int timeout_ms = 200;
    epoll_event *events = new epoll_event[_max_events];
    int ret = 0;

    _status = S_RUN;
    while (_status != S_STOP) {
        int fds_num = epoll_wait(_epollfd, events, _max_events, timeout_ms);
        if (fds_num == -1) {
            if (errno == EINTR) { /*The call was interrupted by a signal handler*/
                continue;
            }
            LOG_ERROR("epoll_wait error:%s", strerror(errno));
            ret = -1;
            break;
        }
        for (int i = 0; i < fds_num; i++) {
            this->handle_event(events[i]);
        }
        clear_idle_clients();

        pthread_mutex_lock(&_client_lock);
        if (_clients == 0 && _status == S_REJECT_CONN) {
            _status = S_STOP;
            LOG_INFO("client is empty and ready for stop server!");
        }
        pthread_mutex_unlock(&_client_lock);
    }
    LOG_INFO("epoll wait loop stop ...");
    if (events != NULL) {
        delete[] events;
        events = NULL;
    }
    return ret;
}

int EpollSocket::start_epoll() {
    /* Ignore broken pipe signal. */
    signal(SIGPIPE, SIG_IGN);

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

bool EpollSocket::is_run() {
    return _status == S_RUN;
}

int EpollSocket::stop_epoll() {
    _status = S_REJECT_CONN;
    LOG_INFO("stop epoll , current clients:%u", _clients);
    _thread_pool->destroy_threadpool();
    return 0;
}

int EpollSocket::remove_client(EpollContext *ctx) {
    pthread_mutex_lock(&_client_lock);
    _clients--;
    _eclients.erase(ctx->_id);
    pthread_mutex_unlock(&_client_lock);
    return 0;
}

int EpollSocket::close_and_release(epoll_event &epoll_event) {
    if (epoll_event.data.ptr == NULL) {
        return 0;
    }
    LOG_DEBUG("connect close");

    EpollContext *hc = (EpollContext *) epoll_event.data.ptr;
    if (_watcher) {
        _watcher->on_close(*hc);
    }

    int fd = hc->fd;
    epoll_event.events = EPOLLIN | EPOLLOUT;
    if (_epollfd > 0) {
        epoll_ctl(_epollfd, EPOLL_CTL_DEL, fd, &epoll_event);
    }
    
    remove_client(hc);
    if (_clients == 0 && _status == S_REJECT_CONN) {
        _status = S_STOP;
        LOG_INFO("client is empty and ready for stop server!");
    }

    delete (EpollContext *) epoll_event.data.ptr;
    epoll_event.data.ptr = NULL;

    int ret = 0;
    if (fd > 0) {
        ret = close(fd);
    }

    LOG_DEBUG("connect close complete which fd:%d, ret:%d", fd, ret);
    return ret;
}

void EpollSocket::add_bind_ip(std::string ip) {
    _bind_ips.push_back(ip);
}

std::map<long long, EpollContext *> EpollSocket::get_clients() {
    std::map<long long, EpollContext *> ret;
    pthread_mutex_lock(&_client_lock);
    ret = _eclients;
    pthread_mutex_unlock(&_client_lock);
    return ret;
}

