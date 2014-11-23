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
#include "epoll_socket.h"
#include "http_parser.h"

struct HttpMethod {
	int code;
	std::string name;
};

const static HttpMethod GET_METHOD = {1, "GET"};
const static HttpMethod POST_METHOD = {2, "POST"};
const static HttpMethod ALL_METHOD = {3, "ALL"};

const static int MAX_RES_SIZE = 65536;

typedef Response (*method_handler_ptr)(Request& request );

struct Resource {
	HttpMethod method;
	method_handler_ptr handler_ptr;
};

class HttpEpollWatcher : public EpollSocketWatcher {
private:
	std::map<std::string, Resource> resource_map;
public:
	void add_mapping(std::string path, method_handler_ptr handler, HttpMethod method = ALL_METHOD);

	int handle_request(Request &request, Response &response);

	virtual int on_accept(EpollContext &epoll_context) ;

	virtual int on_readable(EpollContext &epoll_context, char *read_buffer, int buffer_size, int read_size) ;

	virtual int on_writeable(EpollContext &epoll_context) ;

	virtual int on_close(EpollContext &epoll_context) ;
};


class HttpServer {

private:
	HttpEpollWatcher http_handler;

public:

	void add_mapping(std::string path, method_handler_ptr handler, HttpMethod method = ALL_METHOD);

	int start(int port, int backlog = 10);

};

#endif /* HTTP_SERVER_H_ */
