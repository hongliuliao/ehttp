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
#include "json/json.h"
#include "epoll_socket.h"
#include "http_parser.h"

struct HttpMethod {
	int code;
	std::string name;
};

const static HttpMethod GET_METHOD = {1, "GET"};

typedef void (*method_handler_ptr)(Request& request, Response &response);
typedef void (*json_handler_ptr)(Request& request, Json::Value &response);

struct Resource {
	HttpMethod method;
	method_handler_ptr handler_ptr;
	json_handler_ptr json_ptr;
};

class HttpEpollWatcher : public EpollSocketWatcher {
private:
	std::map<std::string, Resource> resource_map;
public:
	virtual ~HttpEpollWatcher() {}

	void add_mapping(std::string path, method_handler_ptr handler, HttpMethod method = GET_METHOD);

	void add_mapping(std::string path, json_handler_ptr handler, HttpMethod method = GET_METHOD);

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

	void add_mapping(std::string path, method_handler_ptr handler, HttpMethod method = GET_METHOD);

	void add_mapping(std::string path, json_handler_ptr handler, HttpMethod method = GET_METHOD);

	int start(int port, int backlog = 10, int max_events = 1000);

};

#endif /* HTTP_SERVER_H_ */
