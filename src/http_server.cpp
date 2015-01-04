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
#include "http_parser.h"
#include "http_server.h"

int HttpServer::start(int port, int backlog) {

	EpollSocket epoll_socket;
	epoll_socket.start_epoll(port, http_handler, backlog);
}

void HttpServer::add_mapping(std::string path, method_handler_ptr handler, HttpMethod method) {
	http_handler.add_mapping(path, handler, method);
}

void HttpEpollWatcher::add_mapping(std::string path, method_handler_ptr handler, HttpMethod method) {
	Resource resource = {method, handler};
	resource_map[path] = resource;
}

int HttpEpollWatcher::handle_request(Request &req, Response &res) {
	std::string uri = req.get_request_uri();
	if(this->resource_map.find(uri) == this->resource_map.end()) { // not found
		res.code_msg = STATUS_NOT_FOUND;
		res.body = STATUS_NOT_FOUND.msg;
		LOG_INFO("page not found which uri:%s", uri.c_str());
		return 0;
	}

	Resource resource = this->resource_map[req.get_request_uri()];
	// check method
	HttpMethod method = resource.method;
	if(method.code != ALL_METHOD.code && method.name != req.line.method) {
		res.code_msg = STATUS_METHOD_NOT_ALLOWED;
		res.set_head("Allow", method.name);
		res.body.clear();
		LOG_INFO("not allow method, allowed:%s, request method:%s", method.name.c_str(), req.line.method.c_str());
		return 0;
	}
	method_handler_ptr handle = resource.handler_ptr;
	if(handle != NULL) {
		handle(req, res);
	}
	LOG_DEBUG("handle response success which code:%d, msg:%s", res.code_msg.status_code, res.code_msg.msg.c_str());
	return 0;
}

int HttpEpollWatcher::on_accept(EpollContext &epoll_context) {
	int conn_sock = epoll_context.fd;
	epoll_context.ptr = new HttpContext(conn_sock);
	return 0;
}

int HttpEpollWatcher::on_readable(EpollContext &epoll_context, char *read_buffer, int buffer_size, int read_size) {
	HttpContext *http_context = (HttpContext *) epoll_context.ptr;
	http_context->record_start_time();

	if(read_size == buffer_size) {
        LOG_WARN("NOT VALID DATA! single line max size is %d", buffer_size);
        return -1;
    }

	int ret = http_context->req.parse_request(read_buffer, read_size);
	if(ret != 0) {
		LOG_WARN("parse_request error which ret:%d", ret);
		return -1;
	}
	this->handle_request(http_context->req, http_context->get_res());

	return 0;
}

int HttpEpollWatcher::on_writeable(EpollContext &epoll_context) {
	int fd = epoll_context.fd;
	HttpContext *hc = (HttpContext *) epoll_context.ptr;
	bool is_keepalive = (strcasecmp(hc->req.get_header("Connection").c_str(), "keep-alive") == 0);

	char buffer[WRITE_RES_BUFFER_SIZE];
	bzero(buffer, WRITE_RES_BUFFER_SIZE);

	int read_size = 0;
	int ret = hc->get_res().get_some_response(buffer, WRITE_RES_BUFFER_SIZE, read_size, hc->req.line.http_version, is_keepalive);

	int nwrite = send(fd, buffer, read_size, 0);
	if(nwrite < 0) {
		perror("send");
	}

	LOG_DEBUG("send complete which write_num:%d, read_size:%d", nwrite, read_size);

	bool print_access_log = true;

	if (ret == 1) {/* not send over*/
	    print_access_log = false;
	    LOG_DEBUG("has big response, we will send part first and send other part later ...");
	    return 2;
	}

	if (print_access_log) {
        hc->print_access_log();
	}

	if(is_keepalive && nwrite > 0) {
	    hc->clear();
		return 0;
	}
	return 1;
}

int HttpEpollWatcher::on_close(EpollContext &epoll_context) {
    if(epoll_context.ptr == NULL) {
        return 0;
    }
	HttpContext *hc = (HttpContext *) epoll_context.ptr;
	if(hc != NULL) {
		delete hc;
		hc = NULL;
	}
	return 0;
}
