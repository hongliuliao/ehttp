/*
 * http_parser.cpp
 *
 *  Created on: Oct 26, 2014
 *      Author: liao
 */
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include "simple_log.h"
#include "sim_parser.h"
#include "http_parser.h"

#define MAX_REQ_SIZE 10485760

std::string RequestParam::get_param(std::string &name) {
    std::multimap<std::string, std::string>::iterator i = this->params.find(name);
    if (i == params.end()) {
        return std::string();
    }
    return i->second;
}

void RequestParam::get_params(std::string &name, std::vector<std::string> &params) {
    std::pair<std::multimap<std::string, std::string>::iterator, std::multimap<std::string, std::string>::iterator> ret = this->params.equal_range(name);
    for (std::multimap<std::string, std::string>::iterator it=ret.first; it!=ret.second; ++it) {
        params.push_back(it->second);
    }
}

int RequestParam::parse_query_url(std::string &query_url) {
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
            params.insert(std::pair<std::string, std::string>(key, value));
        }
    }
    return 0;
}


std::string RequestLine::get_request_uri() {
	std::stringstream ss(this->request_url);
	std::string uri;
	std::getline(ss, uri, '?');
	return uri;
}

int RequestLine::parse_request_url_params() {
    std::stringstream ss(request_url);
    LOG_DEBUG("start parse params which request_url:%s", request_url.c_str());

    std::string uri;
    std::getline(ss, uri, '?');
    if(ss.good()) {
        std::string query_url;
        std::getline(ss, query_url, '?');

        param.parse_query_url(query_url);
    }
    return 0;
}

std::string RequestBody::get_param(std::string name) {
   return _req_params.get_param(name);
}

void RequestBody::get_params(std::string &name, std::vector<std::string> &params) {
   return _req_params.get_params(name, params);
}

std::string *RequestBody::get_raw_string() {
    return &_raw_string;
}

RequestParam *RequestBody::get_req_params() {
    return &_req_params;
}

std::string Request::get_param(std::string name) {
	if (line.method == "GET") {
		return line.get_request_param().get_param(name);
	}
	if (line.method == "POST") {
		return _body.get_param(name);
	}
	return "";
}

#define ishex(in) ((in >= 'a' && in <= 'f') || \
                   (in >= 'A' && in <= 'F') || \
                   (in >= '0' && in <= '9'))


int unescape(std::string &param, std::string &unescape_param) {
    int write_index = 0;
    for (unsigned i = 0; i < param.size(); i++) {
        if (('%' == param[i]) && ishex(param[i+1]) && ishex(param[i+2])) {
            std::string temp;
            temp += param[i+1];
            temp += param[i+2];
            char *ptr;
            unescape_param[write_index] = (unsigned char) strtol(temp.c_str(), &ptr, 16);
            i += 2;
        } else {
            unescape_param[write_index] = param[i];
        }
        write_index++;
    }
    return 0;
}

std::string Request::get_unescape_param(std::string name) {
    std::string param = this->get_param(name);
    if (param.empty()) {
        return param;
    }
    std::string unescape_param;
    unescape(param, unescape_param);
    return unescape_param;
}

void Request::get_params(std::string &name, std::vector<std::string> &params) {
    if (line.method == "GET") {
        line.get_request_param().get_params(name, params);
    }
    if (line.method == "POST") {
        _body.get_params(name, params);
    }
}

void Request::add_header(std::string &name, std::string &value) {
	this->headers[name] = value;
}

std::string Request::get_header(std::string name) {
	return this->headers[name];
}

std::string Request::get_request_uri() {
	return line.get_request_uri();
}

int ss_on_url(http_parser *p, const char *buf, size_t len) {
    Request *req = (Request *) p->data;
    std::string url;
    url.assign(buf, len);
    req->line.request_url += url;
    return 0;
}

int ss_on_header_field(http_parser *p, const char *buf, size_t len) {
    Request *req = (Request *) p->data;
    int parse_part = req->parse_part;
    if (parse_part == PARSE_REQ_LINE) {
        if (p->method == 1) {
            req->line.method = "GET";
        }
        if (p->method == 3) {
            req->line.method = "POST";
        }
        int ret = req->line.parse_request_url_params();
        if (ret != 0) {
            return ret; 
        }
        if (p->http_major == 1 && p->http_minor == 0) {
            req->line.http_version = "HTTP/1.0";
        }
        if (p->http_major == 1 && p->http_minor == 1) {
            req->line.http_version = "HTTP/1.1";
        }
        parse_part = PARSE_REQ_HEAD;
    }

    std::string field;
    field.assign(buf, len);
    if (req->last_was_value) {
        req->header_fields.push_back(field);
        req->last_was_value = false;
    } else {
        req->header_fields[req->header_fields.size() - 1] += field;
    }
    LOG_DEBUG("GET field:%s", field.c_str());
    return 0;
}

int ss_on_header_value(http_parser *p, const char *buf, size_t len) {
    Request *req = (Request *) p->data;
    
    std::string value;
    value.assign(buf, len);
    if (!req->last_was_value) {
        req->header_values.push_back(value); 
    } else {
        req->header_values[req->header_values.size() - 1] += value; 
    } 
    LOG_DEBUG("GET value:%s", value.c_str());
    req->last_was_value = true;
    return 0;
}

int ss_on_headers_complete(http_parser *p) {
    Request *req = (Request *) p->data;
    if (req->header_fields.size() != req->header_values.size()) {
        LOG_ERROR("header field size:%u != value size:%u",
            req->header_fields.size(), req->header_values.size());
        return -1;
    }
    for (size_t i = 0; i < req->header_fields.size(); i++) {
        req->add_header(req->header_fields[i], req->header_values[i]);    
    }
    req->parse_part = PARSE_REQ_HEAD_OVER;
    LOG_DEBUG("HEADERS COMPLETE! which field size:%u, value size:%u",
        req->header_fields.size(), req->header_values.size());
    return 0;
}

int ss_on_body(http_parser *p, const char *buf, size_t len) {
    Request *req = (Request *) p->data;
    req->get_body()->get_raw_string()->append(buf, len);
    req->parse_part = PARSE_REQ_BODY;
    LOG_DEBUG("GET body len:%d", len);
    return 0;
}

int ss_on_message_complete(http_parser *p) {
    Request *req = (Request *) p->data;
    // parse body params
    if (req->get_header("Content-Type") == "application/x-www-form-urlencoded") {
        std::string *raw_str = req->get_body()->get_raw_string();
        if (raw_str->size()) {
            req->get_body()->get_req_params()->parse_query_url(*raw_str);
        }
    }
    req->parse_part = PARSE_REQ_OVER;
    LOG_DEBUG("msg COMPLETE!");
    return 0;
}

Request::Request() {
    parse_part = PARSE_REQ_LINE;
    total_req_size = 0;
    last_was_value = true; // add new field for first

    http_parser_settings_init(&_settings);
    _settings.on_url = ss_on_url;
    _settings.on_header_field = ss_on_header_field;
    _settings.on_header_value = ss_on_header_value;
    _settings.on_headers_complete = ss_on_headers_complete;
    _settings.on_body = ss_on_body;
    _settings.on_message_complete = ss_on_message_complete;

    http_parser_init(&_parser, HTTP_REQUEST);
    _parser.data = this;
}

Request::~Request() {}

int Request::parse_request(const char *read_buffer, int read_size) {
    total_req_size += read_size;
    if (total_req_size > MAX_REQ_SIZE) {
        LOG_INFO("TOO BIG REQUEST WE WILL REFUSE IT!");
        return -1;
    }
    ssize_t nparsed = http_parser_execute(&_parser, &_settings, read_buffer, read_size);
    if (nparsed != read_size) {
        std::string err_msg = "unkonw";
        if (_parser.http_errno) {
            err_msg = http_errno_description(HTTP_PARSER_ERRNO(&_parser));
        }
        LOG_ERROR("parse request error! msg:%s", err_msg.c_str());
        return -1;
    }
    LOG_DEBUG("read from client: size:%d, content:%s", read_size, read_buffer);

    if (parse_part != PARSE_REQ_OVER) {
        return NEED_MORE_STATUS;
    }
    return 0;
}

RequestBody *Request::get_body() {
    return &_body;
}

Response::Response(CodeMsg status_code) {
	this->code_msg = status_code;
	this->is_writed = 0;
}

void Response::set_head(std::string name, std::string &value) {
	this->headers[name] = value;
}

void Response::set_body(Json::Value &body) {
    Json::FastWriter writer;
    std::string str_value = writer.write(body);
    this->body = str_value;
}

int Response::gen_response(std::string &http_version, bool is_keepalive) {
	LOG_DEBUG("START gen_response code:%d, msg:%s", code_msg.status_code, code_msg.msg.c_str());
	res_bytes << http_version << " " << code_msg.status_code << " " << code_msg.msg << "\r\n";
	res_bytes << "Server: SimpleServer/0.1" << "\r\n";
	if(headers.find("Content-Type") == headers.end()) {
		res_bytes << "Content-Type: application/json; charset=UTF-8" << "\r\n";
	}
	res_bytes << "Content-Length: " << body.size() << "\r\n";

	std::string con_status = "Connection: close";
	if(is_keepalive) {
		con_status = "Connection: Keep-Alive";
	}
	res_bytes << con_status << "\r\n";

	for (std::map<std::string, std::string>::iterator it=headers.begin(); it!=headers.end(); ++it) {
		res_bytes << it->first << ": " << it->second << "\r\n";
	}
	// header end
	res_bytes << "\r\n";
	res_bytes << body;

	LOG_DEBUG("gen response context:%s", res_bytes.str().c_str());
	return 0;
}

int Response::readsome(char *buffer, int buffer_size, int &read_size) {
    res_bytes.read(buffer, buffer_size);
    read_size = res_bytes.gcount();

    if (!res_bytes.eof()) {
        return 1;
    }
    return 0;
}

int Response::rollback(int num) {
    if (res_bytes.eof()) {
        res_bytes.clear();
    }
    int rb_pos = (int) res_bytes.tellg() - num;
    res_bytes.seekg(rb_pos);
    return res_bytes.good() ? 0 : -1;
}

