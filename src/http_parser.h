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
#include <sys/time.h>

#include "json/json.h"
#include "simple_log.h"

struct CodeMsg {
	int status_code;
	std::string msg;
};

const static CodeMsg STATUS_OK = {200, "OK"};
const static CodeMsg STATUS_NOT_FOUND = {404, "Not Found"};
const static CodeMsg STATUS_METHOD_NOT_ALLOWED = {405, "Method Not Allowed"};
const static CodeMsg STATUS_RESPONSE_TOO_LARGE = {513, "Response Entry Too Large"};

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

class RequestBody {
public:
	std::map<std::string, std::string> params;

	std::map<std::string, std::string> get_params();

};

class Request {
private:
	std::map<std::string, std::string> headers;
public:
	RequestLine line;
	RequestBody body;

	std::string get_param(std::string name);

	void add_header(std::string &name, std::string &value);

	std::string get_header(std::string name);

	std::string get_request_uri();
};

class Response {
private:
	std::map<std::string, std::string> headers;

public:
	CodeMsg code_msg;
	std::string body;

	Response(CodeMsg status_code);
	Response(CodeMsg status_code, Json::Value &body);

	void set_head(std::string name, std::string &value);

	std::string gen_response(std::string &http_version, bool is_keepalive);
};

class HttpContext {
public:
	Request *req;
	Response *res;
	int fd;
	timeval start;

	HttpContext(Request *req, Response *res, int fd) {
		this->req = &(*req);
		this->res = &(*res);
		this->fd = fd;
	}

	~HttpContext() {
		if(req != NULL) {
			LOG_DEBUG("start delete request... which ptr:%d", req);
			delete req;
			req = NULL;
		}
		if(res != NULL) {
			LOG_DEBUG("start delete response... which ptr:%d", res);
			delete res;
			res = NULL;
		}
	}

	int record_start_time() {
		gettimeofday(&start, NULL);
		return 0;
	}

	int get_cost_time() {
		timeval end;
		gettimeofday(&end, NULL);
		int cost_time = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
		return cost_time;
	}

	void print_access_log() {
		std::string http_method = this->req->line.method;
		std::string request_url = this->req->line.request_url;
		int cost_time = get_cost_time();
		LOG_INFO("access_log %s %s status_code:%d cost_time:%d ms, body_size:%d", http_method.c_str(), request_url.c_str(), res->code_msg.status_code, cost_time, res->body.size());
	}
};

/**
 * query_url : name=tom&age=3
 */
int parse_query_url(std::map<std::string, std::string> &output, std::string &query_url);

/**
 * request_url : /sayhello?name=tom&age=3
 */
int parse_request_url_params(std::map<std::string, std::string> &output, std::string &request_url);

int parse_request_line(const char *line, int size, RequestLine &request_line);

int parse_request(const char *request_buffer, int buffer_size, int read_size, int &parse_part, Request &request);

void split_str(std::vector<std::string> &output, std::string &logContent, char split_char);


#endif /* HTTP_PARSER_H_ */
