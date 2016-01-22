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

int HttpServer::start(int port, int backlog, int max_events) {
    return epoll_socket.start_epoll(port, http_handler, backlog, max_events);
}

void HttpServer::add_mapping(std::string path, method_handler_ptr handler, HttpMethod method) {
    http_handler.add_mapping(path, handler, method);
}

void HttpServer::add_mapping(std::string path, json_handler_ptr handler, HttpMethod method) {
    http_handler.add_mapping(path, handler, method);
}

void HttpServer::add_bind_ip(std::string ip) {
    epoll_socket.add_bind_ip(ip);
}

void HttpServer::set_thread_pool(ThreadPool *tp) {
    epoll_socket.set_thread_pool(tp);
}

void HttpEpollWatcher::add_mapping(std::string path, method_handler_ptr handler, HttpMethod method) {
    Resource resource = {method, handler, NULL};
    resource_map[path] = resource;
}

void HttpEpollWatcher::add_mapping(std::string path, json_handler_ptr handler, HttpMethod method) {
    Resource resource = {method, NULL, handler};
    resource_map[path] = resource;
}

int HttpEpollWatcher::handle_request(Request &req, Response &res) {
    std::string uri = req.get_request_uri();
    if (this->resource_map.find(uri) == this->resource_map.end()) { // not found
        res._code_msg = STATUS_NOT_FOUND;
        res._body = STATUS_NOT_FOUND.msg;
        LOG_INFO("page not found which uri:%s", uri.c_str());
        return 0;
    }

    Resource resource = this->resource_map[req.get_request_uri()];
    // check method
    HttpMethod method = resource.method;
    if (method.name != req._line.get_method()) {
        res._code_msg = STATUS_METHOD_NOT_ALLOWED;
        res.set_head("Allow", method.name);
        res._body.clear();
        LOG_INFO("not allow method, allowed:%s, request method:%s", 
            method.name.c_str(), req._line.get_method().c_str());
        return 0;
    }

    if (resource.json_ptr != NULL) {
        Json::Value root;
        resource.json_ptr(req, root);
        res.set_body(root);
    } else if (resource.handler_ptr != NULL) {
        resource.handler_ptr(req, res);
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
    if (http_context->get_requset()._parse_part == PARSE_REQ_LINE) {
        http_context->record_start_time();
    }

    int ret = http_context->get_requset().parse_request(read_buffer, read_size);
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

    this->handle_request(http_context->get_requset(), http_context->get_res());

    return READ_OVER;
}

int HttpEpollWatcher::on_writeable(EpollContext &epoll_context) {
    int fd = epoll_context.fd;
    HttpContext *hc = (HttpContext *) epoll_context.ptr;
    Response &res = hc->get_res();
    bool is_keepalive = (strcasecmp(hc->get_requset().get_header("Connection").c_str(), "keep-alive") == 0);

    if (!res._is_writed) {
        std::string http_version = hc->get_requset()._line.get_http_version();
        res.gen_response(http_version, is_keepalive);
        res._is_writed = true;
    }

    char buffer[SS_WRITE_BUFFER_SIZE];
    bzero(buffer, SS_WRITE_BUFFER_SIZE);
    int read_size = 0;

    // 1. read some response bytes
    int ret = res.readsome(buffer, SS_WRITE_BUFFER_SIZE, read_size);
    // 2. write bytes to socket
    int nwrite = send(fd, buffer, read_size, 0);
    if (nwrite < 0) {
        perror("send fail!");
        return WRITE_CONN_CLOSE;
    }
    // 3. when not write all buffer, we will rollback write index
    if (nwrite < read_size) {
        res.rollback(read_size - nwrite);
    }
    LOG_DEBUG("send complete which write_num:%d, read_size:%d", nwrite, read_size);

    if (ret == 1) {/* not send over*/
        LOG_DEBUG("has big response, we will send part first and send other part later ...");
        return WRITE_CONN_CONTINUE;
    }

    if (ret == 0 && nwrite == read_size) {
        hc->print_access_log(epoll_context.client_ip);
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
