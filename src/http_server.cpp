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


int HttpServer::start(int port, int backlog) {
	int sockfd, new_fd; /* listen on sock_fd, new connection on new_fd */

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

	while (1) { /* main accept() loop */
		struct sockaddr_in their_addr; /* connector's address information */
		socklen_t sin_size = sizeof(struct sockaddr_in);

		if ((new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size)) == -1) {
			perror("accept");
			continue;
		}

		LOG_DEBUG("server: got connection from %s\n", inet_ntoa(their_addr.sin_addr));

		int buffer_size = 1024;
		char read_buffer[buffer_size];
		memset(read_buffer, 0, buffer_size);

		int read_size;
		int line_num = 0;
		RequestLine req_line;
		while((read_size = recv(new_fd, read_buffer, buffer_size, 0)) > 0) {

			if(read_buffer[read_size - 2] != '\r' || read_buffer[read_size - 1] != '\n') {
				LOG_DEBUG("NOT VALID DATA!");
				break;
			}
			std::string req_str(read_buffer, buffer_size);
			LOG_DEBUG("read from client: size:%d, content:%s", read_size, req_str.c_str());

			std::stringstream ss(req_str);
			std::string line;
			int ret = 0;
			while(ss.good() && line != "\r") {
				std::getline(ss, line, '\n');
				line_num++;

				// parse request line like  "GET /index.jsp HTTP/1.1"
				if(line_num == 1) {
					ret = parse_request_line(line.c_str(), line.size() - 1, req_line);
					if(ret == 0) {
						LOG_DEBUG("parse_request_line success which method:%s, url:%s, http_version:%s", req_line.method.c_str(), req_line.request_url.c_str(), req_line.http_version.c_str());
					} else {
						LOG_INFO("parse request line error!");
						break;
					}
				}
			}

			if(ret != 0) {
				break;
			}

			if(line == "\r" && req_line.method == "GET") {
				Request req;
				req.request_line = req_line;

				// default http response
				Response res(200, "hello");

				method_handler_ptr handle = this->resource_map[req.get_request_uri()];
				if(handle != NULL) {
					res = handle(req);
				}

				std::string res_content = res.gen_response();
				if (send(new_fd, res_content.c_str(), res_content.size(), 0) == -1) {
					perror("send");
				}
				line_num = 0;
				// http 1.0 close socket by server, 1.1 close by client
				if(req_line.http_version == "HTTP/1.0") {
					break;
				}
			}

			memset(read_buffer, 0, buffer_size);
		}

		LOG_DEBUG("connect close!");
		close(new_fd);
	}

	return 0;
}

void HttpServer::add_mapping(std::string path, method_handler_ptr handler) {
	resource_map[path] = handler;
}
