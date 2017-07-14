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

const static int PARSE_MULTI_DISPOSITION = 0;
const static int PARSE_MULTI_CONTENT_TYPE = 1;
//const static int PARSE_MULTI_DATA = 2;
const static int PARSE_MULTI_OVER = 3;

const static int NEED_MORE_STATUS = 1;
const static int PARSE_LEN_REQUIRED = 2;

class RequestParam {
    public:

        /**
         * get param by name
         * when not found , return empty string
         */
        std::string get_param(std::string &name);

        /**
         * get params by name
         * when params in url like age=1&age=2, it will return [1, 2]
         */
        void get_params(std::string &name, std::vector<std::string> &params);

        /**
         * query_url : name=tom&age=3
         */
        int parse_query_url(std::string &query_url);

        int add_param_pair(const std::string &key, const std::string &value);
    private:
        std::multimap<std::string, std::string> _params;
};

/**
 * parse for http protocol first line, like below:
 * GET /login?name=tom&age=1 HTTP/1.0
 */
class RequestLine {
    public:
        /**
         * return "GET" or "POST"
         */
        std::string get_method();
        /**
         * like /login
         */
        std::string get_request_uri();
        /**
         * like /login?name=tom&age=1
         */
        std::string get_request_url();
        /**
         * return "HTTP/1.0" or "HTTP/1.1"
         */
        std::string get_http_version();

        RequestParam &get_request_param();

        std::string to_string();
        /**
         * request_url : /sayhello?name=tom&age=3
         */
        int parse_request_url_params();
        
        void set_method(std::string m);
        
        void set_request_url(std::string url);
        
        void append_request_url(std::string p_url);
        
        void set_http_version(const std::string &v);
    private:
        RequestParam _param;
        std::string _method;       // like GET/POST
        std::string _request_url;  // like /hello?name=aaa
        std::string _http_version; // like HTTP/1.1
};

class FileItem {
    public:
        FileItem();
        /**
         * the item is file field or normal field 
         **/
        bool is_file();
        
        std::string *get_fieldname();
        // get upload file name
        std::string *get_filename();
        // get upload file content type
        std::string *get_content_type();
        // get file data or field value
        std::string *get_data();

        bool get_parse_state();
        void set_is_file();
        void set_name(const std::string &name);
        void set_filename(const std::string &filename);
        void append_data(const char *c, size_t len);
        void set_content_type(const char *c, int len);
        void set_parse_state(int state);
    private:
        bool _is_file;
        bool _parse_state;
        std::string _content_type;
        std::string _name;
        std::string _filename;
        std::string _data;
};

class RequestBody {
    public:

        /**
         * when Content-Type is "application/x-www-form-urlencoded"
         * we will parse the request body , it will excepted like below
         *
         *     "name=tom&age=1"
         *
         */
        std::string get_param(std::string name);

        void get_params(std::string &name, std::vector<std::string> &params);

        /**
         * get request body bytes
         */
        std::string *get_raw_string();

        RequestParam *get_req_params();

        int parse_multi_params();

        std::vector<FileItem> *get_file_items();

    private:
        std::string _raw_string;
        RequestParam _req_params;
        std::vector<FileItem> _file_items;
};

class Request {
    public:
        Request();

        ~Request();

        std::string get_param(std::string name);

        /**
         * Now, it's the same as get_param(std::string name);
         */
        std::string get_unescape_param(std::string name);

        void get_params(std::string &name, std::vector<std::string> &params);

        std::string get_header(std::string name);

        /**
         * return like /login
         */
        std::string get_request_uri();
        
        /**
         * return like /login?name=tom&age=1
         */
        std::string get_request_url();

        std::string *get_client_ip();

        void set_client_ip(std::string *ip);
        
        void add_header(std::string &name, std::string &value);

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
        std::string *_client_ip;
};

class Response {
    public:
        Response(CodeMsg status_code = STATUS_OK);
        Response(CodeMsg status_code, Json::Value &body);

        void set_head(const std::string &name, const std::string &value);

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

        Request &get_request();
        
        int _fd;
        timeval _start;
    private:
        Response *_res;
        Request *_req;
};

#endif /* SIM_PARSER_H_ */
