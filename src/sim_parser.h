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
    public:

        std::string get_param(std::string &name);

        void get_params(std::string &name, std::vector<std::string> &params);

        /**
         * query_url : name=tom&age=3
         */
        int parse_query_url(std::string &query_url);
    private:
        std::multimap<std::string, std::string> _params;
};

class RequestLine {
    public:
        std::string get_request_uri();

        RequestParam &get_request_param();

        std::string to_string();
        /**
         * request_url : /sayhello?name=tom&age=3
         */
        int parse_request_url_params();
        
        std::string get_method();
        
        void set_method(std::string m);
        
        std::string get_request_url();
        
        void set_request_url(std::string url);
        
        void append_request_url(std::string p_url);
        
        std::string get_http_version();
        
        void set_http_version(std::string v);
    private:
        RequestParam _param;
        std::string _method;       // like GET/POST
        std::string _request_url;  // like /hello?name=aaa
        std::string _http_version; // like HTTP/1.1
};


class RequestBody {
    public:
        std::string get_param(std::string name);

        void get_params(std::string &name, std::vector<std::string> &params);

        std::string *get_raw_string();

        RequestParam *get_req_params();
    private:
        std::string _raw_string;
        RequestParam _req_params;
};

class Request {
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

        bool _last_was_value;
        std::vector<std::string> _header_fields;
        std::vector<std::string> _header_values;
        int _parse_part;
        int _parse_err;
        RequestLine _line;
    private:
        std::map<std::string, std::string> _headers;
        int _total_req_size;
        RequestBody _body;
        http_parser_settings _settings;
        http_parser _parser;

};

class Response {
    public:
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
        
        bool _is_writed;
        CodeMsg _code_msg;
        std::string _body;
    private:
        std::map<std::string, std::string> _headers;
        std::stringstream _res_bytes;
};

class HttpContext {
    public:
        HttpContext(int fd);

        ~HttpContext();

        int record_start_time();

        int get_cost_time();

        void print_access_log(std::string &client_ip);

        inline void delete_req_res();

        void clear();

        Response &get_res();

        Request &get_requset();
        
        int _fd;
        timeval _start;
    private:
        Response *_res;
        Request *_req;
};

#endif /* SIM_PARSER_H_ */
