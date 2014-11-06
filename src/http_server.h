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
#include "sys/epoll.h"
#include "http_parser.h"

struct HttpMethod {
	int code;
	std::string name;
};

const static HttpMethod GET_METHOD = {1, "GET"};
const static HttpMethod POST_METHOD = {2, "POST"};
const static HttpMethod ALL_METHOD = {3, "ALL"};

typedef Response (*method_handler_ptr)(Request& request );

struct Resource {
	HttpMethod method;
	method_handler_ptr handler_ptr;
};


class HttpServer {
private:
	std::map<std::string, Resource> resource_map;

	int listen_on(int port, int backlog);

	int accept_socket(int sockfd);

	int setNonblocking(int fd);

	void do_use_fd(int fd, int events);
public:

	int start(int port, int backlog = 10);

	int close_and_remove_epoll_events(int &epollfd, epoll_event &epoll_event);

	void add_mapping(std::string path, method_handler_ptr handler, HttpMethod method = ALL_METHOD);

	int handle_request(Request &request, Response &response);
};

#endif /* HTTP_SERVER_H_ */
