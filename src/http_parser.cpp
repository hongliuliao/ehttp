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
#include "http_parser.h"

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

int RequestLine::parse_request_line(const char *line, int size) {
    std::stringstream ss(std::string(line, size));

    std::getline(ss, method, ' ');
    if(!ss.good()) {
        return -1;
    }
    std::getline(ss, request_url, ' ');
    if(!ss.good()) {
        return -1;
    }
    int ret = parse_request_url_params();
    if (ret != 0) {
        LOG_WARN("parse_request_url_params fail which request_url:%s", request_url.c_str());
        return ret;
    }

    std::getline(ss, http_version, ' ');

    return 0;
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

std::string Request::get_param(std::string name) {
	if(line.method == "GET") {
		return line.get_request_param().get_param(name);
	}
	if(line.method == "POST") {
		return body.get_param(name);
	}
	return "";
}

void Request::get_params(std::string &name, std::vector<std::string> &params) {
    if(line.method == "GET") {
        line.get_request_param().get_params(name, params);
    }
    if(line.method == "POST") {
        body.get_params(name, params);
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


int Request::parse_request(const char *read_buffer, int buffer_size, int read_size, int &parse_part) {
    if(read_size == buffer_size) {
        LOG_WARN("NOT VALID DATA! single line max size is %d", buffer_size);
        return -1;
    }

    std::string req_str(read_buffer, read_size);
    LOG_DEBUG("read from client: size:%d, content:%s", read_size, req_str.c_str());

    std::stringstream ss(req_str);
    std::string line;
    int ret = 0;

    while(ss.good()) {
        std::getline(ss, line, '\n');
        if(line == "\r") {  /* the last line in head */
            parse_part = PARSE_REQ_OVER;

            if(this->line.method == "POST") { // post request need body
                parse_part = PARSE_REQ_BODY;
            }
            continue;
        }

        if(parse_part == PARSE_REQ_LINE) { // parse request line like  "GET /index.jsp HTTP/1.1"
            LOG_DEBUG("start parse req_line line:%s", line.c_str());
            ret = this->line.parse_request_line(line.c_str(), line.size() - 1);
            if(ret != 0) {
                LOG_INFO("parse request line error!");
                return -1;
            }
            parse_part = PARSE_REQ_HEAD;
            LOG_DEBUG("parse_request_line success which method:%s, url:%s, http_version:%s", this->line.method.c_str(), this->line.request_url.c_str(), this->line.http_version.c_str());

            // check method
            if(this->line.method != "GET" && this->line.method != "POST") {
                LOG_ERROR("un support method:%s", this->line.method.c_str());
                return -1;
            }
            continue;
        }

        if(parse_part == PARSE_REQ_HEAD && !line.empty()) { // read head
            LOG_DEBUG("start PARSE_REQ_HEAD line:%s", line.c_str());

            std::vector<std::string> parts;
            split_str(parts, line, ':'); // line like Cache-Control:max-age=0
            if(parts.size() < 2) {
                LOG_WARN("not valid head which line:%s", line.c_str());
                continue;
            }

            add_header(parts[0], parts[1]);
            continue;
        }

        if(parse_part == PARSE_REQ_BODY && !line.empty()) {
            LOG_DEBUG("start PARSE_REQ_BODY line:%s", line.c_str());

            this->body.parse_query_url(line);
            parse_part = PARSE_REQ_OVER;
            break;
        }
    }

    if(parse_part != PARSE_REQ_OVER) {
        LOG_DEBUG("parse request no over parse_part:%d", parse_part);
        return 1; // to be continue
    }
    return ret;
}

Response::Response() {
    this->code_msg = STATUS_OK;
}

Response::Response(CodeMsg status_code) {
	this->code_msg = status_code;
}

Response::Response(CodeMsg status_code, Json::Value &json_value) {
	Json::FastWriter writer;
	std::string str_value = writer.write(json_value);

	LOG_DEBUG("get json value in res : %s, code:%d, msg:%s", str_value.c_str(), status_code.status_code, status_code.msg.c_str());

	this->code_msg = status_code;
	this->body = str_value;
};

void Response::set_head(std::string name, std::string &value) {
	this->headers[name] = value;
}

std::string Response::gen_response(std::string &http_version, bool is_keepalive) {
	std::stringstream res;
	LOG_DEBUG("START gen_response code:%d, msg:%s", code_msg.status_code, code_msg.msg.c_str());
	res << http_version << " " << code_msg.status_code << " " << code_msg.msg << "\r\n";
	res << "Server: SimpleServer/0.1" << "\r\n";
	if(headers.find("Content-Type") == headers.end()) {
		res << "Content-Type: application/json; charset=UTF-8" << "\r\n";
	}
	res << "Content-Length: " << body.size() << "\r\n";

	std::string con_status = "Connection: close";
	if(is_keepalive) {
		con_status = "Connection: Keep-Alive";
	}
	res << con_status << "\r\n";

	for (std::map<std::string, std::string>::iterator it=headers.begin(); it!=headers.end(); ++it) {
		res << it->first << ": " << it->second << "\r\n";
	}
	// header end
	res << "\r\n";
	res << body;

	LOG_DEBUG("gen response context:%s", res.str().c_str());
	return res.str();
}



static inline std::string &ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
        return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
        return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
        return ltrim(rtrim(s));
}

void split_str(std::vector<std::string> &result, std::string &str, char split_char) {

	std::stringstream ss(str);
	while(ss.good()) {
		std::string temp;
		std::getline(ss, temp, split_char);

		result.push_back(trim(temp));
	}

}
