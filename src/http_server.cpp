/*
 * http_server.cpp
 *
 *  Created on: Oct 26, 2014
 *      Author: liao
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <sstream>

#include "simple_log.h"
#include "sim_parser.h"
#include "http_server.h"

HttpMethod::HttpMethod(int code, std::string name) {
    _codes.insert(code);
    _names.insert(name);
}

HttpMethod& HttpMethod::operator|(HttpMethod hm) {
    std::set<int> *codes = hm.get_codes();
    _codes.insert(codes->begin(), codes->end());
    std::set<std::string> *names = hm.get_names();
    _names.insert(names->begin(), names->end());
    return *this;
}

std::set<int> *HttpMethod::get_codes() {
    return &_codes;
}

std::set<std::string> *HttpMethod::get_names() {
    return &_names;
}

std::string HttpMethod::get_names_str() {
    std::stringstream ss;
    size_t i = 0;
    for (std::set<std::string>::iterator it = _names.begin(); it != _names.end(); ++it) {
        ss << *it;
        if (i != _names.size() - 1) {
            ss << ", ";
        }
        i++;
    }
    return ss.str();
}

HttpServer::HttpServer() {
    _port = 3456; // default port 
    _backlog = 10;
    _max_events = 1000;
    _pid = 0;
    _resource_map = new std::map<std::string, Resource>();
    _http_watcher = new HttpEpollWatcher(_resource_map);
}

HttpServer::~HttpServer() {
    if (_resource_map != NULL) {
        delete _resource_map;
        _resource_map = NULL;
    }
    if (_http_watcher != NULL) {
        delete _http_watcher;
        _http_watcher = NULL;
    }
    if (!_buildin_jhs.empty()) {
        for (size_t i = 0; i < _buildin_jhs.size(); i++) {
            delete _buildin_jhs[i];
        }
    }
}

int HttpServer::start(int port, int backlog, int max_events) {
    LOG_WARN("start() method is deprecated, please use start_sync() or start_async() instead!");
    epoll_socket.set_port(port);
    epoll_socket.set_backlog(backlog);
    epoll_socket.set_max_events(max_events);
    epoll_socket.set_watcher(_http_watcher);
    return epoll_socket.start_epoll();
}

int HttpServer::stop() {
    LOG_INFO("stoping http server...");
    return epoll_socket.stop_epoll();
}


void *http_start_routine(void *ptr) {
    HttpServer *hs = (HttpServer *) ptr;
    hs->start_sync();
    return NULL;
}

int HttpServer::start_async() {
    int ret = pthread_create(&_pid, NULL, http_start_routine, this);
    if (ret != 0) {
        LOG_ERROR("HttpServer::start_async err:%d", ret);
        return ret;
    }
    return 0;
}

int HttpServer::join() {
    if (_pid == 0) {
        LOG_ERROR("HttpServer not start async!");
        return -1;
    }
    return pthread_join(_pid, NULL);
}

int HttpServer::start_sync() {
    epoll_socket.set_port(_port);
    epoll_socket.set_backlog(_backlog);
    epoll_socket.set_max_events(_max_events);
    epoll_socket.set_watcher(_http_watcher);
    return epoll_socket.start_epoll();
}

void HttpServer::add_mapping(std::string path, method_handler_ptr handler, HttpMethod method) {
    Resource res = {method, handler, NULL, NULL};
    (*_resource_map)[path] = res;
}

void HttpServer::add_mapping(std::string path, json_handler_ptr handler, HttpMethod method) {
    Resource res = {method, NULL, handler, NULL};
    (*_resource_map)[path] = res;
}
        
int HttpServer::add_mapping(const std::string &path, HttpJsonHandler *handler, HttpMethod method) {
    Resource res = {method, NULL, NULL, handler};
    (*_resource_map)[path] = res;
    return 0;
}

class BuildInGetClientHandler : public HttpJsonHandler {
    public:
        BuildInGetClientHandler(EpollSocket *so) {
            _ep_so = so;
        }

        int action(Request &req, Json::Value &root) {
            std::map<long long, EpollContext *> eclients = _ep_so->get_clients();
            std::map<long long, EpollContext *>::iterator it = eclients.begin();
            int i = 0;
            root["total"] = (int)eclients.size();
            for (; it != eclients.end(); it++) {
                root["clients"][i]["id"] = (*it).first;
                root["clients"][i]["fd"] = (*it).second->fd;
                root["clients"][i]["idle"] = (long long)time(NULL) - (*it).second->_last_interact_time;
                i++;
            }
            return 0;
        }

    private:
        EpollSocket *_ep_so;
};

int HttpServer::add_buildin_mappings() {
    HttpJsonHandler *get_clients_handler = new BuildInGetClientHandler(&epoll_socket);
    this->add_mapping("/_clients", get_clients_handler);
    _buildin_jhs.push_back(get_clients_handler);

    return 0;
}

void HttpServer::add_bind_ip(std::string ip) {
    epoll_socket.add_bind_ip(ip);
}

void HttpServer::set_thread_pool(ThreadPool *tp) {
    epoll_socket.set_thread_pool(tp);
}

void HttpServer::set_backlog(int backlog) {
    _backlog = backlog;
}

void HttpServer::set_max_events(int me) {
    _max_events = me;
}

void HttpServer::set_port(int port) {
    _port = port;
}

int HttpServer::set_client_max_idle_time(int sec) {
    return epoll_socket.set_client_max_idle_time(sec);
}

HttpEpollWatcher::HttpEpollWatcher(std::map<std::string, Resource> *resource_map) {
    _resource_map = resource_map;
}

int HttpEpollWatcher::handle_request(Request &req, Response &res) {
    std::string uri = req.get_request_uri();
    if (this->_resource_map->find(uri) == this->_resource_map->end()) { // not found
        res._code_msg = STATUS_NOT_FOUND;
        res._body = STATUS_NOT_FOUND.msg;
        LOG_INFO("page not found which uri:%s", uri.c_str());
        return 0;
    }

    Resource resource = (*this->_resource_map)[req.get_request_uri()];
    // check method
    HttpMethod method = resource.method;
    if (method.get_names()->count(req._line.get_method()) == 0) {
        res._code_msg = STATUS_METHOD_NOT_ALLOWED;
        std::string allow_names = method.get_names_str(); 
        res.set_head("Allow", allow_names);
        res._body.clear();
        LOG_INFO("not allow method, allowed:%s, request method:%s", 
            allow_names.c_str(), req._line.get_method().c_str());
        return 0;
    }

    if (resource.json_ptr != NULL) {
        Json::Value root;
        resource.json_ptr(req, root);
        res.set_body(root);
    } else if (resource.handler_ptr != NULL) {
        resource.handler_ptr(req, res);
    } else if (resource.jh != NULL) {
        Json::Value root;
        resource.jh->action(req, root);
        res.set_body(root);
    }
    LOG_DEBUG("handle response success which code:%d, msg:%s", 
        res._code_msg.status_code, res._code_msg.msg.c_str());
    return 0;
}

int HttpEpollWatcher::on_accept(EpollContext &epoll_context) {
    int conn_sock = epoll_context.fd;
    epoll_context.ptr = new HttpContext(conn_sock);
    return 0;
}

int HttpEpollWatcher::on_readable(int &epollfd, epoll_event &event) {
    EpollContext *epoll_context = (EpollContext *) event.data.ptr;
    int fd = epoll_context->fd;

    int buffer_size = SS_READ_BUFFER_SIZE;
    char read_buffer[buffer_size];
    memset(read_buffer, 0, buffer_size);

    int read_size = recv(fd, read_buffer, buffer_size, 0);
    if (read_size == -1 && errno == EINTR) {
        return READ_CONTINUE; 
    } 
    if (read_size == -1 /* io err*/|| read_size == 0 /* close */) {
        return READ_CLOSE;
    }
    LOG_DEBUG("read success which read size:%d", read_size);
    HttpContext *http_context = (HttpContext *) epoll_context->ptr;
    if (http_context->get_request()._parse_part == PARSE_REQ_LINE) {
        http_context->record_start_time();
    }

    int ret = http_context->get_request().parse_request(read_buffer, read_size);
    if (ret < 0) {
        return READ_CLOSE;
    }
    if (ret == NEED_MORE_STATUS) {
        return READ_CONTINUE;
    }
    if (ret == PARSE_LEN_REQUIRED) {
        http_context->get_res()._code_msg = STATUS_LENGTH_REQUIRED;
        http_context->get_res()._body = STATUS_LENGTH_REQUIRED.msg;
        return READ_OVER;
    } 

    http_context->get_request().set_client_ip(&epoll_context->client_ip);
    this->handle_request(http_context->get_request(), http_context->get_res());

    return READ_OVER;
}

int HttpEpollWatcher::on_writeable(EpollContext &epoll_context) {
    int fd = epoll_context.fd;
    HttpContext *hc = (HttpContext *) epoll_context.ptr;
    Request &req = hc->get_request();
    Response &res = hc->get_res();
    bool is_keepalive = (strcasecmp(req.get_header("Connection").c_str(), "keep-alive") == 0);

    if (!res._is_writed) {
        std::string http_version = req._line.get_http_version();
        res.gen_response(http_version, is_keepalive);
        res._is_writed = true;
    }

    char buffer[SS_WRITE_BUFFER_SIZE];
    bzero(buffer, SS_WRITE_BUFFER_SIZE);
    int read_size = 0;

    // 1. read some response bytes
    int ret = res.readsome(buffer, SS_WRITE_BUFFER_SIZE, read_size);
    if (read_size == 0) {
        LOG_WARN("response read size is zero, close connect");
        return WRITE_CONN_CLOSE;
    }
    // 2. write bytes to socket
    int nwrite = send(fd, buffer, read_size, 0);
    if (nwrite < 0) {
        LOG_ERROR("send fail, err:%s", strerror(errno));
        return WRITE_CONN_CLOSE;
    }
    // 3. when not write all buffer, we will rollback write index
    if (nwrite < read_size) {
        res.rollback(read_size - nwrite);
    }
    LOG_DEBUG("send complete which write_num:%d, read_size:%d", nwrite, read_size);

    if (ret == NEED_MORE_STATUS) {/* not send over*/
        LOG_DEBUG("has big response, we will send part first and send other part later ...");
        return WRITE_CONN_CONTINUE;
    }

    if (ret == 0 && nwrite == read_size) {
        std::string http_method = req._line.get_method();
        std::string request_url = req._line.get_request_url();
        int cost_time = hc->get_cost_time();
        LOG_INFO("access_log %s %s status_code:%d cost_time:%d us, body_size:%d, client_ip:%s",
                http_method.c_str(), request_url.c_str(), res._code_msg.status_code,
                cost_time, res._body.size(), epoll_context.client_ip.c_str());
    }

    if (is_keepalive && nwrite > 0) {
        hc->clear();
        return WRITE_CONN_ALIVE;
    }
    return WRITE_CONN_CLOSE;
}

int HttpEpollWatcher::on_close(EpollContext &epoll_context) {
    if (epoll_context.ptr == NULL) {
        return 0;
    }
    HttpContext *hc = (HttpContext *) epoll_context.ptr;
    if (hc != NULL) {
        delete hc;
        hc = NULL;
    }
    return 0;
}
