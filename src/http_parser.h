/*
 * http_parser.h
 *
 *  Created on: Oct 26, 2014
 *      Author: liao
 */

#ifndef HTTP_PARSER_H_
#define HTTP_PARSER_H_

#include <string>
#include <sstream>
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

const static int PARSE_REQ_LINE = 0;
const static int PARSE_REQ_HEAD = 1;
const static int PARSE_REQ_BODY = 2;
const static int PARSE_REQ_OVER = 3;

class RequestParam {
private:
    std::multimap<std::string, std::string> params;
public:

    std::string get_param(std::string &name);

    void get_params(std::string &name, std::vector<std::string> &params);

    /**
     * query_url : name=tom&age=3
     */
    int parse_query_url(std::string &query_url);
};

class RequestLine {

public:
	std::string method;       // like GET/POST
	std::string request_url;  // like /hello?name=aaa
	std::string http_version; // like HTTP/1.1

	std::string get_request_uri();

    int parse_request_line(const char *line, int size);

    RequestParam &get_request_param() {
        return param;
    }
private:
    RequestParam param;
    /**
     * request_url : /sayhello?name=tom&age=3
     */
    int parse_request_url_params();
};


#define RequestBody RequestParam

class Request {
private:
	std::map<std::string, std::string> headers;
public:
	RequestLine line;
	RequestBody body;

	std::string get_param(std::string name);

	std::string get_unescape_param(std::string name);

	void get_params(std::string &name, std::vector<std::string> &params);

	void add_header(std::string &name, std::string &value);

	std::string get_header(std::string name);

	std::string get_request_uri();

	int parse_request(const char *read_buffer, int read_size);
};

class Response {
private:
	std::map<std::string, std::string> headers;
	std::stringstream res_bytes;

public:
	bool is_writed;
	CodeMsg code_msg;
	std::string body;

	Response(CodeMsg status_code = STATUS_OK);
	Response(CodeMsg status_code, Json::Value &body);

	void set_head(std::string name, std::string &value);

	void set_body(Json::Value &body);

	int gen_response(std::string &http_version, bool is_keepalive);

	/**
	 * return 0: read part, 1: read over, -1:read error
	 */
	int readsome(char *buffer, int buffer_size, int &read_size);

	/**
	 * rollback num bytes in response bytes
	 */
	int rollback(int num);

};

class HttpContext {
private:
	Response *res;
public:
	Request req;
	int fd;
	timeval start;

	HttpContext(int fd) {
		this->fd = fd;
		res = new Response();
	}

	~HttpContext() {
	    clear();
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
		std::string http_method = this->req.line.method;
		std::string request_url = this->req.line.request_url;
		int cost_time = get_cost_time();
		LOG_INFO("access_log %s %s status_code:%d cost_time:%d ms, body_size:%d", http_method.c_str(), request_url.c_str(), res->code_msg.status_code, cost_time, res->body.size());
	}

	void clear() {
	    req = Request();
	    if (res != NULL) {
	        delete res;
	    }
        res = new Response();
	}

	Response &get_res() {
	    return *res;
	}

};

void split_str(std::vector<std::string> &output, std::string &logContent, char split_char);


#endif /* HTTP_PARSER_H_ */
