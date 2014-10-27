/*
 * http_parser.cpp
 *
 *  Created on: Oct 26, 2014
 *      Author: liao
 */
#include <sstream>
#include <simple_log.h>
#include <http_parser.h>

std::map<std::string, std::string> RequestBody::get_params() {
	return parse_query_url(this->req_body);
}

std::map<std::string, std::string> parse_query_url(std::string query_url) {
	std::map<std::string, std::string> result;
	std::stringstream query_ss(query_url);
	LOG_DEBUG("start parse_query_url:%s", query_url.c_str());

	while(query_ss.good()) {
		std::string key_value;
		std::getline(query_ss, key_value, '&');
		LOG_DEBUG("get key_value:%s", key_value.c_str());

		std::stringstream key_value_ss(key_value);
		while(key_value_ss.good()) {
			std::string key, value;
			std::getline(key_value_ss, key, '=');
			std::getline(key_value_ss, value, '=');
			result[key] = value;
		}
	}
	return result;
}

std::map<std::string, std::string> RequestLine::get_params() {
	std::map<std::string, std::string> result;

	std::stringstream ss(this->request_url);
	LOG_DEBUG("start parse params which request_url:%s", request_url.c_str());

	std::string uri;
	std::getline(ss, uri, '?');
	if(ss.good()) {
		std::string query_url;
		std::getline(ss, query_url, '?');

		return parse_query_url(query_url);
	}
	return result;
}

std::string RequestLine::get_request_uri() {
	std::stringstream ss(this->request_url);
	std::string uri;
	std::getline(ss, uri, '?');
	return uri;
}

std::string Request::get_param(std::string name) {
	if(request_line.method == "GET") {
		return request_line.get_params()[name];
	}
	if(request_line.method == "POST") {
		return request_body.get_params()[name];
	}
	return "";
}

std::string Request::get_request_uri() {
	return request_line.get_request_uri();
}

Response::Response(CodeMsg status_code, std::string body) {
	server = "SimpleServer/0.1";
	content_type = "text/html";
	connection = "close";
	this->code_msg = status_code;
	this->body = body;
}

std::string Response::gen_response(std::string http_version) {
	std::stringstream res;
	res << "HTTP/1.0 " << code_msg.status_code << " " << code_msg.msg << "\r\n";
	res << "Server: SimpleServer/0.1" << "\r\n";
	res << "Content-Type: text/html" << "\r\n";
	res << "Content-Length: " << body.size() << "\r\n";

	std::string con_status = "Connection: close";
	if(http_version == "HTTP/1.1") {
		con_status = "Connection: Keep-Alive";
	}

	res << con_status << "\r\n";
	res << "\r\n";
	res << body;
	return res.str();
}

int parse_request_line(const char *line, int size, RequestLine &request_line) {
	std::stringstream ss(std::string(line, size));

	std::getline(ss, request_line.method, ' ');
	if(!ss.good()) {
		return -1;
	}
	std::getline(ss, request_line.request_url, ' ');
	if(!ss.good()) {
		return -1;
	}
	std::getline(ss, request_line.http_version, ' ');

	return 0;
}

int parse_request_head(const char *line, int size, RequestHead &head) {
	std::stringstream ss(std::string(line, size));
	std::string head_name;
	std::getline(ss, head_name, ':');
	if(!ss.good()) {
		return -1;
	}

	if(head_name == "Accept") {
		std::getline(ss, head.accept, ':');
	}
	if(head_name == "User-Agent") {
		std::getline(ss, head.user_agent, ':');
	}
}

int parse_request(const char *read_buffer, int buffer_size, int read_size, Request &request) {
	if(read_buffer[read_size - 2] != '\r' || read_buffer[read_size - 1] != '\n') {
		LOG_DEBUG("NOT VALID DATA! single request max size is %d", buffer_size);
		return -1;
	}

	std::string req_str(read_buffer, buffer_size);
	LOG_DEBUG("read from client: size:%d, content:%s", read_size, req_str.c_str());

	std::stringstream ss(req_str);
	std::string line;
	int ret = 0;
	int line_num = 0;

	while(ss.good() && line != "\r" /* the last line in head */) {
		std::getline(ss, line, '\n');
		line_num++;

		// parse request line like  "GET /index.jsp HTTP/1.1"
		if(line_num == 1) {
			RequestLine req_line;
			ret = parse_request_line(line.c_str(), line.size() - 1, req_line);
			if(ret == 0) {
				request.request_line = req_line;
				LOG_DEBUG("parse_request_line success which method:%s, url:%s, http_version:%s", req_line.method.c_str(), req_line.request_url.c_str(), req_line.http_version.c_str());
			} else {
				LOG_INFO("parse request line error!");
				return -1;
			}
		}
	}
	return ret;
}
