/*
 * http_parser.h
 *
 *  Created on: Oct 26, 2014
 *      Author: liao
 */

#ifndef SIM_PARSER_H_
#define SIM_PARSER_H_

#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <sys/time.h>

#include "json/json.h"
#include "simple_log.h"
#include "http_parser.h"

struct CodeMsg {
    int status_code;
    std::string msg;
};

const static CodeMsg STATUS_OK = {200, "OK"};
const static CodeMsg STATUS_NOT_FOUND = {404, "Not Found"};
const static CodeMsg STATUS_METHOD_NOT_ALLOWED = {405, "Method Not Allowed"};
const static CodeMsg STATUS_LENGTH_REQUIRED = {411, "Length Required"};

const static int PARSE_REQ_LINE = 0;
const static int PARSE_REQ_HEAD = 1;
const static int PARSE_REQ_BODY = 2;
const static int PARSE_REQ_OVER = 3;
const static int PARSE_REQ_HEAD_OVER = 4;

const static int NEED_MORE_STATUS = 1;
const static int PARSE_LEN_REQUIRED = 2;

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

        RequestParam &get_request_param() {
            return param;
        }

        std::string to_string() {
            std::string ret = "method:";
            ret += method;
            ret += ",";
            ret += "request_url:";
            ret += request_url;
            ret += ",";
            ret += "http_version:";
            ret += http_version;
            return ret;
        }
        /**
         * request_url : /sayhello?name=tom&age=3
         */
        int parse_request_url_params();
    private:
        RequestParam param;
};


class RequestBody {
    private:
        std::string _raw_string;
        RequestParam _req_params;
    public:
        std::string get_param(std::string name);

        void get_params(std::string &name, std::vector<std::string> &params);

        std::string *get_raw_string();

        RequestParam *get_req_params();
};

class Request {
    private:
        std::map<std::string, std::string> headers;
        int total_req_size;
        RequestBody _body;
        http_parser_settings _settings;
        http_parser _parser;
    public:
        Request();

        ~Request();

        std::string get_param(std::string name);

        std::string get_unescape_param(std::string name);

        void get_params(std::string &name, std::vector<std::string> &params);

        void add_header(std::string &name, std::string &value);

        std::string get_header(std::string name);

        std::string get_request_uri();

        int parse_request(const char *read_buffer, int read_size);

        int clear();

        RequestBody *get_body();

        std::string get_method();

        bool last_was_value;

        std::vector<std::string> header_fields;

        std::vector<std::string> header_values;

        int parse_part;

        int _parse_err;

        RequestLine line;

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
        Request *req;
    public:
        int fd;
        timeval start;

        HttpContext(int fd) {
            this->fd = fd;
            req = new Request();
            res = new Response();
        }

        ~HttpContext() {
            delete_req_res();
        }

        int record_start_time() {
            gettimeofday(&start, NULL);
            return 0;
        }

        int get_cost_time() {
            timeval end;
            gettimeofday(&end, NULL);
            int cost_time = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
            return cost_time;
        }

        void print_access_log(std::string &client_ip) {
            std::string http_method = this->req->line.method;
            std::string request_url = this->req->line.request_url;
            int cost_time = get_cost_time();
            LOG_INFO("access_log %s %s status_code:%d cost_time:%d us, body_size:%d, client_ip:%s",
                    http_method.c_str(), request_url.c_str(), res->code_msg.status_code,
                    cost_time, res->body.size(), client_ip.c_str());
        }

        inline void delete_req_res() {
            if (req != NULL) {
                delete req;
                req = NULL;
            }
            if (res != NULL) {
                delete res;
                res = NULL;
            }
        }

        void clear() {
            delete_req_res();
            req = new Request();
            res = new Response();
        }

        Response &get_res() {
            return *res;
        }

        Request &get_requset() {
            return *req;
        }

};

#endif /* SIM_PARSER_H_ */
