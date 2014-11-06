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

int HttpServer::listen_on(int port, int backlog) {
	int sockfd; /* listen on sock_fd, new connection on new_fd */

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	struct sockaddr_in my_addr; /* my address information */
	memset (&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET; /* host byte order */
	my_addr.sin_port = htons(port); /* short, network byte order */
	my_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */

	int opt = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if (bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1) {
		perror("bind");
		exit(1);
	}

	if (listen(sockfd, backlog) == -1) {
		perror("listen");
		exit(1);
	}

	LOG_INFO("start HttpServer on port : %d", port);
	return sockfd;
}

int HttpServer::accept_socket(int sockfd) {
	int new_fd;
	struct sockaddr_in their_addr; /* connector's address information */
	socklen_t sin_size = sizeof(struct sockaddr_in);

	if ((new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size)) == -1) {
		perror("accept");
		return -1;
	}

	LOG_DEBUG("server: got connection from %s\n", inet_ntoa(their_addr.sin_addr));
	return new_fd;
}

int HttpServer::setNonblocking(int fd) {
	int flags;

	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);

}

int HttpServer::start(int port, int backlog) {

	int sockfd = this->listen_on(port, backlog);

	struct epoll_event ev;
	int epollfd = epoll_create(10);
	if (epollfd == -1) {
		perror("epoll_create");
		exit(EXIT_FAILURE);
	}

	ev.events = EPOLLIN;
	ev.data.fd = sockfd;
	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
		perror("epoll_ctl: listen_sock");
		exit(EXIT_FAILURE);
	}

	epoll_event events[10];

	while(1) {
		int fds_num = epoll_wait(epollfd, events, 10, -1);
		if(fds_num == -1) {
			perror("epoll_pwait");
			exit(EXIT_FAILURE);
		}

		for (int i = 0; i < fds_num; i++) {
			if(events[i].data.fd == sockfd) {
				int conn_sock = accept_socket(sockfd);
				setNonblocking(conn_sock);
				LOG_DEBUG("get accept socket which listen fd:%d, conn_sock_fd:%d", sockfd, conn_sock);

				epoll_event conn_sock_ev;
				conn_sock_ev.events = EPOLLIN | EPOLLET;
				conn_sock_ev.data.ptr = new HttpContext(new Request(), new Response(STATUS_OK), conn_sock);
				LOG_DEBUG("init http context success which ptr:%d, data.fd:%d", conn_sock_ev.data.ptr, ((HttpContext *)conn_sock_ev.data.ptr)->fd);

				if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &conn_sock_ev) == -1) {
				   perror("epoll_ctl: conn_sock");
				   exit(EXIT_FAILURE);
				}

			} else if(events[i].events & EPOLLIN ){ // readable
				HttpContext *hc = (HttpContext *) events[i].data.ptr;
				int fd = hc->fd;
				int buffer_size = 1024;
				char read_buffer[buffer_size];
				memset(read_buffer, 0, buffer_size);
				int read_size = 0;
				LOG_DEBUG("get readable epoll_event which data.fd:%d, data.ptr:%d", ((HttpContext *)events[i].data.ptr)->fd, events[i].data.ptr);

				if((read_size = recv(fd, read_buffer, buffer_size, 0)) > 0) {
					LOG_DEBUG("read success which read content:%s", read_buffer);

					int parse_part = PARSE_REQ_LINE;
					HttpContext *http_context = (HttpContext *) events[i].data.ptr;
					int ret = parse_request(read_buffer, buffer_size, read_size, parse_part, *(http_context->req));
					if(ret != 0) {
						LOG_WARN("parse_request error which ret:%d", ret);

						close_and_remove_epoll_events(epollfd, events[i]);
						continue;
					}
					this->handle_request(*(http_context->req), *(http_context->res));

					events[i].events = EPOLLOUT | EPOLLET;
					epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &events[i]);

				} else {
					LOG_DEBUG("read_size not normal which size:%d", read_size);
					close_and_remove_epoll_events(epollfd, events[i]);
				}
			} else if(events[i].events & EPOLLOUT) { // writeable
				HttpContext *hc = (HttpContext *) events[i].data.ptr;
				int fd = hc->fd;
				LOG_DEBUG("start write data");

				bool is_keepalive = (strcasecmp(hc->req->get_header("Connection").c_str(), "keep-alive") == 0);
				std::string content = hc->res->gen_response(hc->req->line.http_version, is_keepalive);
				int write_num = send(fd, content.c_str(), content.size(), 0);
				if(write_num < 0) {
					perror("send");
				}

				hc->print_access_log();

				if(is_keepalive) {
					events[i].events = EPOLLIN | EPOLLET;
					epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &events[i]);
				} else {
					close_and_remove_epoll_events(epollfd, events[i]);
				}
			} else {
				LOG_INFO("unkonw events :%d", events[i].events);
				continue;
			}
		}
	}

}

int HttpServer::close_and_remove_epoll_events(int &epollfd, epoll_event &epoll_event) {
	LOG_INFO("connect close");
	HttpContext *hc = (HttpContext *) epoll_event.data.ptr;
	int fd = hc->fd;
	epoll_event.events = EPOLLIN | EPOLLOUT | EPOLLET;
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &epoll_event);

	if(epoll_event.data.ptr != NULL) {
		delete (HttpContext *)epoll_event.data.ptr;
		epoll_event.data.ptr = NULL;
	}

	int ret = close(fd);
	LOG_DEBUG("connect close complete which fd:%d, ret:%d", fd, ret);
	return 0;
}

void HttpServer::add_mapping(std::string path, method_handler_ptr handler, HttpMethod method) {
	Resource resource = {method, handler};
	resource_map[path] = resource;
}

int HttpServer::handle_request(Request &req, Response &res) {
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
		res = handle(req);
	}
	LOG_DEBUG("handle response success which code:%d, msg:%s", res.code_msg.status_code, res.code_msg.msg.c_str());
	return 0;
}
