/*
 * tcp_epoll.h
 *
 *  Created on: Nov 10, 2014
 *      Author: liao
 */

#ifndef TCP_EPOLL_H_
#define TCP_EPOLL_H_

#include "sys/epoll.h"
#include <vector>
#include <set>
#include <string>
#include <map>
#include "threadpool.h"

#define SS_WRITE_BUFFER_SIZE 4096
#define SS_READ_BUFFER_SIZE 4096

#define WRITE_CONN_ALIVE 0
#define WRITE_CONN_CLOSE 1
#define WRITE_CONN_CONTINUE 2

#define READ_OVER 0
#define READ_CONTINUE 1
#define READ_CLOSE -1

#define CONTEXT_READING 1
#define CONTEXT_READ_OVER 2

#define CONTEXT_WRITING 1
#define CONTEXT_WRITE_OVER 2

class EpollContext {
    public:
        EpollContext();
        std::string to_string();

        long long _id;
        void *ptr;
        int fd;
        time_t _last_interact_time; // unit is second
        std::string _client_ip;
        int _ctx_status;
};

typedef void (*ScheduleHandlerPtr)();

class EpollSocketWatcher {
    public:
        virtual int on_accept(EpollContext &epoll_context) = 0;

        virtual int on_readable(int &epollfd, epoll_event &event) = 0;

        /**
         * return :
         * if return value == 1, we will close the connection
         * if return value == 2, we will continue to write
         */
        virtual int on_writeable(EpollContext &epoll_context) = 0;

        virtual int on_close(EpollContext &epoll_context) = 0;

};

enum EpollSocketStatus {
    S_INIT = 0,
    S_RUN = 1,
    S_REJECT_CONN = 2,
    S_STOP = 3
};

class EpollSocket {
    private:
        int set_nonblocking(int fd);

        int accept_socket(int sockfd, std::string &client_ip);

        int bind_on(unsigned int ip);

        int listen_on();

        int close_and_release(epoll_event &event);

        int init_default_tp();

        int create_epoll();

        int add_listen_sock_to_epoll();

        int start_event_loop();

        std::vector<std::string> _bind_ips;
        int _backlog;
        int _max_events;
        int _port;
        int _epollfd;
        std::set<int> _listen_sockets;
        pthread_mutex_t _client_lock;
        volatile int _clients;
        std::map<long long, EpollContext *> _eclients;
        int _max_idle_sec;
        
        ThreadPool *_thread_pool;
        bool _use_default_tp;
        volatile int _status;
        EpollSocketWatcher *_watcher;
    public:
        EpollSocket();
       
         ~EpollSocket();
        
        int multi_thread_handle_read_event(epoll_event &e);
        int multi_thread_handle_write_event(epoll_event &e);
        int handle_readable_event(epoll_event &event);
        int handle_accept_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_watcher);
        int handle_writeable_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_watcher);
        int handle_event(epoll_event &e);

        int init_tp();
        int start_epoll();
        int stop_epoll();
        bool is_run();
        void set_thread_pool(ThreadPool *tp);
        void set_port(int port);
        void set_watcher(EpollSocketWatcher *w);
        void set_backlog(int backlog);
        void set_max_events(int max_events);

        //void set_schedule(ScheduleHandlerPtr h);
        int set_client_max_idle_time(int sec);

        void add_bind_ip(std::string ip);
        int get_clients_info(std::stringstream &ss);

        std::map<long long, EpollContext *> get_clients();
        EpollContext *create_client(int conn_sock, const std::string &client_ip);
        int add_client(EpollContext *ctx);
        int remove_client(EpollContext *ctx);
        int update_interact_time(EpollContext *ctx, time_t t);
        int clear_idle_clients();

        int get_epfd();
        EpollSocketWatcher *get_watcher();
};

struct TaskData {
    epoll_event event;
    EpollSocket *es;
};


#endif /* TCP_EPOLL_H_ */
