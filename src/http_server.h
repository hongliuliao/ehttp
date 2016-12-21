/*
 * http_server.h
 *
 *  Created on: Oct 26, 2014
 *      Author: liao
 */

#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#include <string>
#include <map>
#include <set>
#include "sys/epoll.h"
#include "json/json.h"
#include "epoll_socket.h"
#include "sim_parser.h"

class HttpMethod {
public:
    HttpMethod() {};
    HttpMethod(int code, std::string name);
    HttpMethod& operator|(HttpMethod hm);
    std::set<int> *get_codes();
    std::set<std::string> *get_names();
private:
    std::set<int> _codes;
    std::set<std::string> _names;
};

static HttpMethod GET_METHOD = HttpMethod(1, "GET");
static HttpMethod POST_METHOD = HttpMethod(2, "POST");

typedef void (*method_handler_ptr)(Request& request, Response &response);
typedef void (*json_handler_ptr)(Request& request, Json::Value &response);

struct Resource {
    HttpMethod method;
    method_handler_ptr handler_ptr;
    json_handler_ptr json_ptr;
};

class HttpEpollWatcher : public EpollSocketWatcher {
    private:
        std::map<std::string, Resource> resource_map;
    public:
        virtual ~HttpEpollWatcher() {}

        void add_mapping(std::string path, method_handler_ptr handler, HttpMethod method = GET_METHOD);

        void add_mapping(std::string path, json_handler_ptr handler, HttpMethod method = GET_METHOD);

        int handle_request(Request &request, Response &response);

        virtual int on_accept(EpollContext &epoll_context) ;

        virtual int on_readable(int &epollfd, epoll_event &event) ;

        virtual int on_writeable(EpollContext &epoll_context) ;

        virtual int on_close(EpollContext &epoll_context) ;
};


class HttpServer {
    public:
        HttpServer();

        void add_mapping(std::string path, method_handler_ptr handler, HttpMethod method = GET_METHOD);

        void add_mapping(std::string path, json_handler_ptr handler, HttpMethod method = GET_METHOD);

        void add_bind_ip(std::string ip);

        int start(int port, int backlog = 10, int max_events = 1000);
        
        int start_async();

        int stop();

        int join();
        
        int start_sync();

        void set_thread_pool(ThreadPool *tp);

        void set_backlog(int backlog);
        
        void set_max_events(int me);
        
        void set_port(int port);
    private:
        HttpEpollWatcher http_handler;
        EpollSocket epoll_socket;
        int _backlog;
        int _max_events;
        int _port;
        pthread_t _pid; // when start async
};

#endif /* HTTP_SERVER_H_ */
