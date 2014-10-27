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

public:

	int start(int port, int backlog = 10);

	void add_mapping(std::string path, method_handler_ptr handler, HttpMethod method = ALL_METHOD);

	int handle_request(Request &request, Response &response);
};

#endif /* HTTP_SERVER_H_ */
