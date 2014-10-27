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

	if (bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1) {
		perror("bind");
		exit(1);
	}

	if (listen(sockfd, backlog) == -1) {
		perror("listen");
		exit(1);
	}
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

int HttpServer::start(int port, int backlog) {

	int sockfd = this->listen_on(port, backlog);

	while (1) { /* main accept() loop */

		int new_fd = this->accept_socket(sockfd);
		if(new_fd == -1) {
			continue;
		}

		int buffer_size = 1024;
		char read_buffer[buffer_size];
		memset(read_buffer, 0, buffer_size);

		int read_size;
		Request req;
		int parse_part = PARSE_REQ_LINE;
		while((read_size = recv(new_fd, read_buffer, buffer_size, 0)) > 0) {
			// 1. parse request
			int ret = parse_request(read_buffer, buffer_size, read_size, parse_part, req);
			if(ret < 0) {
				break;
			}
			if(ret == 1) {
				continue;
			}

			// 2. handle the request and gen response
			std::string http_version = req.request_line.http_version;
			Response res(STATUS_OK, "it works"); // default http response
			ret = this->handle_request(req, res);

			// reset req obj and parse_part for next request
			req = Request();
			parse_part = PARSE_REQ_LINE;

			if(ret != 0) {
				LOG_INFO("handle req error which ret:%d", ret);
				break;
			}

			// 3. send response to client
			std::string res_content = res.gen_response(http_version);
			if (send(new_fd, res_content.c_str(), res_content.size(), 0) == -1) {
				perror("send");
			}

			// 4. http 1.0 close socket by server, 1.1 close by client
			if(http_version == "HTTP/1.0") {
				break;
			}

			memset(read_buffer, 0, buffer_size); // ready for next request to "Keep-Alive"
		}

		LOG_DEBUG("connect close!");
		close(new_fd);
	}

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
	if(method.code != ALL_METHOD.code && method.name != req.request_line.method) {
		res.code_msg = STATUS_METHOD_NOT_ALLOWED;
		res.set_head("Allow", method.name);
		res.body.clear();
		LOG_INFO("not allow method, allowed:%s, request method:%s", method.name.c_str(), req.request_line.method.c_str());
		return 0;
	}
	method_handler_ptr handle = resource.handler_ptr;
	if(handle != NULL) {
		res = handle(req);
	}
	return 0;
}
