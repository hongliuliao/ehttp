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
#include <vector>

#include "json/json.h"

struct CodeMsg {
	int status_code;
	std::string msg;
};

const static CodeMsg STATUS_OK = {200, "OK"};
const static CodeMsg STATUS_NOT_FOUND = {404, "Not Found"};
const static CodeMsg STATUS_METHOD_NOT_ALLOWED = {405, "Method Not Allowed"};

const static int PARSE_REQ_LINE = 0;
const static int PARSE_REQ_HEAD = 1;
const static int PARSE_REQ_BODY = 2;
const static int PARSE_REQ_OVER = 3;

class RequestLine {

public:
	std::string method;       // like GET/POST
	std::string request_url;  // like /hello?name=aaa
	std::string http_version; // like HTTP/1.1
	std::map<std::string, std::string> params;

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
	std::map<std::string, std::string> params;

	std::map<std::string, std::string> get_params();

};

class Request {
public:
	RequestLine line;
	RequestHead head;
	RequestBody body;

	std::string get_param(std::string name);

	std::string get_request_uri();
};

class Response {
private:
	std::map<std::string, std::string> headers;

public:
	CodeMsg code_msg;
	std::string body;

	Response(CodeMsg status_code, Json::Value body);

	void set_head(std::string name, std::string value);

	std::string gen_response(std::string http_version);
};

/**
 * query_url : name=tom&age=3
 */
std::map<std::string, std::string> parse_query_url(std::string query_url);

/**
 * request_url : /sayhello?name=tom&age=3
 */
std::map<std::string, std::string> parse_request_url_params(std::string request_url);

int parse_request_line(const char *line, int size, RequestLine &request_line);

int parse_request_head(const char *line, int size, RequestHead &head);

int parse_request(const char *request_buffer, int buffer_size, int read_size, int &parse_part, Request &request);

std::vector<std::string> split_str(std::string &logContent, char split_char);


#endif /* HTTP_PARSER_H_ */
