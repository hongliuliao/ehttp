/*
 * http_parser.h
 *
 *  Created on: Oct 26, 2014
 *      Author: liao
 */

#ifndef HTTP_PARSER_H_
#define HTTP_PARSER_H_

#include <string>
#include <map>

class RequestLine {
public:
	std::string method;       // like GET/POST
	std::string request_url;  // like /hello?name=aaa
	std::string http_version; // like HTTP/1.1

	std::map<std::string, std::string> get_params();
	std::string get_request_uri();
};

struct RequestHead {
	std::string accept;
	std::string user_agent;
	int content_length;
};

class RequestBody {
public:
	std::string req_body;

	std::map<std::string, std::string> get_params();
};

class Request {
public:
	std::string request_url;

	RequestLine request_line;
	RequestHead request_head;
	RequestBody request_body;

	std::string get_param_by_name(std::string name);

	std::string get_request_uri();
};

class Response {
private:
	std::string server;
	std::string content_type;
	std::string connection;
public:
	int status_code;
	std::string body;

	Response(int status_code, std::string body);
	std::string gen_response();
};

std::map<std::string, std::string> parse_query_url(std::string query_url);

int parse_request_line(const char *line, int size, RequestLine &request_line);

int parse_request_head(const char *line, int size, RequestHead &head);


#endif /* HTTP_PARSER_H_ */
