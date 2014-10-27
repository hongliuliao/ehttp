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

typedef Response (*method_handler_ptr)(Request& request );

class HttpServer {
private:
	std::map<std::string, method_handler_ptr> resource_map;

	int listen_on(int port, int backlog);

	int accept_socket(int sockfd);

public:

	int start(int port, int backlog = 10);

	void add_mapping(std::string path, method_handler_ptr handler);

	int handle_request(Request &request, Response &response);
};

#endif /* HTTP_SERVER_H_ */
