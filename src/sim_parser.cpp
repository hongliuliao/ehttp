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
#include "multipart_parser.h"

#define MAX_REQ_SIZE 10485760
#define EHTTP_VERSION "1.0.3"

std::string RequestParam::get_param(std::string &name) {
    std::multimap<std::string, std::string>::iterator i = this->_params.find(name);
    if (i == _params.end()) {
        return std::string();
    }
    return i->second;
}

void RequestParam::get_params(std::string &name, std::vector<std::string> &params) {
    std::pair<std::multimap<std::string, std::string>::iterator, std::multimap<std::string, std::string>::iterator> ret = this->_params.equal_range(name);
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
            _params.insert(std::pair<std::string, std::string>(key, value));
        }
    }
    return 0;
}
    
int RequestParam::add_param_pair(const std::string &key, const std::string &value) {
    _params.insert(std::pair<std::string, std::string>(key, value));
    return 0;
}

std::string RequestLine::get_request_uri() {
    std::stringstream ss(this->get_request_url());
    std::string uri;
    std::getline(ss, uri, '?');
    return uri;
}

RequestParam &RequestLine::get_request_param() {
    return _param;
}

std::string RequestLine::to_string() {
    std::string ret = "method:";
    ret += _method;
    ret += ",";
    ret += "request_url:";
    ret += _request_url;
    ret += ",";
    ret += "http_version:";
    ret += _http_version;
    return ret;
}

int RequestLine::parse_request_url_params() {
    std::stringstream ss(_request_url);
    LOG_DEBUG("start parse params which request_url:%s", _request_url.c_str());

    std::string uri;
    std::getline(ss, uri, '?');
    if(ss.good()) {
        std::string query_url;
        std::getline(ss, query_url, '?');

        _param.parse_query_url(query_url);
    }
    return 0;
}

std::string RequestLine::get_method() {
    return _method;
}

void RequestLine::set_method(std::string m) {
    _method = m;
}

std::string RequestLine::get_request_url() {
    return _request_url;
}

void RequestLine::set_request_url(std::string url) {
    _request_url = url;
}

void RequestLine::append_request_url(std::string p_url) {
    _request_url += p_url;
}

std::string RequestLine::get_http_version() {
    return _http_version;
}

void RequestLine::set_http_version(const std::string &v) {
    _http_version = v;
}

FileItem::FileItem() {
    _is_file = false;
    _parse_state = PARSE_MULTI_DISPOSITION;
}

bool FileItem::is_file() {
    return _is_file;
}

std::string *FileItem::get_fieldname() {
    return &_name;
}

std::string *FileItem::get_filename() {
    return &_filename;
}

std::string *FileItem::get_content_type() {
    return &_content_type;
}

std::string *FileItem::get_data() {
    return &_data;
}

void FileItem::set_is_file() {
    _is_file = true;
}

void FileItem::set_name(const std::string &name) {
    _name = name;    
}

void FileItem::set_filename(const std::string &filename) {
    _filename = filename;    
}

void FileItem::append_data(const char *c, size_t len) {
    _data.append(c, len);    
}

void FileItem::set_content_type(const char *c, int len) {
    _content_type.assign(c, len);    
}

bool FileItem::get_parse_state() {
    return _parse_state;
}

void FileItem::set_parse_state(int state) {
    _parse_state = state;
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

int RequestBody::parse_multi_params() {
    for (size_t i = 0; i < _file_items.size(); i++) {
        FileItem &item = _file_items[i];
        std::string *field_name = item.get_fieldname();
        std::string *field_value = item.get_data();
        _req_params.add_param_pair(*field_name, *field_value);
        LOG_DEBUG("request body add param name:%s, data:%s", 
                field_name->c_str(), field_value->c_str());
    }
    return 0;
}

std::vector<FileItem> *RequestBody::get_file_items() {
    return &_file_items;
}

std::string Request::get_param(std::string name) {
    if (_line.get_method() == "GET") {
        return _line.get_request_param().get_param(name);
    }
    if (_line.get_method() == "POST") {
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
            unescape_param.append(1, (unsigned char) strtol(temp.c_str(), &ptr, 16));
            i += 2;
        } else if (param[i] == '+') {
            unescape_param.append(" ");
        } else {
            unescape_param.append(1, param[i]);
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
    if (_line.get_method() == "GET") {
        _line.get_request_param().get_params(name, params);
    }
    if (_line.get_method() == "POST") {
        _body.get_params(name, params);
    }
}

void Request::add_header(std::string &name, std::string &value) {
    this->_headers[name] = value;
}

std::string Request::get_header(std::string name) {
    return this->_headers[name];
}

std::string Request::get_request_uri() {
    return _line.get_request_uri();
}

std::string Request::get_request_url() {
    return _line.get_request_url();
}

int ss_on_url(http_parser *p, const char *buf, size_t len) {
    Request *req = (Request *) p->data;
    std::string url;
    url.assign(buf, len);
    req->_line.append_request_url(url);
    return 0;
}

int ss_on_header_field(http_parser *p, const char *buf, size_t len) {
    Request *req = (Request *) p->data;
    if (req->_parse_part == PARSE_REQ_LINE) {
        if (p->method == 1) {
            req->_line.set_method("GET");
        }
        if (p->method == 3) {
            req->_line.set_method("POST");
        }
        int ret = req->_line.parse_request_url_params();
        if (ret != 0) {
            return ret; 
        }
        if (p->http_major == 1 && p->http_minor == 0) {
            req->_line.set_http_version("HTTP/1.0");
        }
        if (p->http_major == 1 && p->http_minor == 1) {
            req->_line.set_http_version("HTTP/1.1");
        }
        req->_parse_part = PARSE_REQ_HEAD;
    }

    std::string field;
    field.assign(buf, len);
    if (req->_last_was_value) {
        req->_header_fields.push_back(field);
        req->_last_was_value = false;
    } else {
        req->_header_fields[req->_header_fields.size() - 1] += field;
    }
    LOG_DEBUG("GET field:%s", field.c_str());
    return 0;
}

int ss_on_header_value(http_parser *p, const char *buf, size_t len) {
    Request *req = (Request *) p->data;

    std::string value;
    value.assign(buf, len);
    if (!req->_last_was_value) {
        req->_header_values.push_back(value); 
    } else {
        req->_header_values[req->_header_values.size() - 1] += value; 
    } 
    LOG_DEBUG("GET value:%s", value.c_str());
    req->_last_was_value = true;
    return 0;
}

int ss_on_headers_complete(http_parser *p) {
    Request *req = (Request *) p->data;
    if (req->_header_fields.size() != req->_header_values.size()) {
        LOG_ERROR("header field size:%u != value size:%u",
                req->_header_fields.size(), req->_header_values.size());
        return -1;
    }
    for (size_t i = 0; i < req->_header_fields.size(); i++) {
        req->add_header(req->_header_fields[i], req->_header_values[i]);    
    }
    req->_parse_part = PARSE_REQ_HEAD_OVER;
    LOG_DEBUG("HEADERS COMPLETE! which field size:%u, value size:%u",
            req->_header_fields.size(), req->_header_values.size());
    if (req->get_method() == "POST" && req->get_header("Content-Length").empty()) {
        req->_parse_err = PARSE_LEN_REQUIRED;
        return -1;
    }
    return 0;
}

int ss_on_body(http_parser *p, const char *buf, size_t len) {
    Request *req = (Request *) p->data;
    req->get_body()->get_raw_string()->append(buf, len);
    req->_parse_part = PARSE_REQ_BODY;
    LOG_DEBUG("GET body len:%d", len);
    return 0;
}

int ss_split_str(const std::string &input, const char s, std::vector<std::string> &tokens) {
    if (input.empty()) {
        return 0;
    }
    std::stringstream ss;
    ss << input;
    std::string token;
    while (std::getline(ss, token, s)) {
        tokens.push_back(token);
    }
    return 0;
}

int ss_on_multipart_name(multipart_parser* p, const char *at, size_t length) {
    std::string s;
    s.assign(at, length);
    Request *req = (Request *)multipart_parser_get_data(p);
    std::vector<FileItem> *items = req->get_body()->get_file_items();
    if (s == "Content-Disposition") {
        FileItem item;
        item.set_parse_state(PARSE_MULTI_DISPOSITION);
        items->push_back(item);
    }
    if (s == "Content-Type") {
        CHECK_ERR(items->empty(), "items is empty! len:%lu", length); 
        FileItem &item = (*items)[items->size() - 1];
        item.set_parse_state(PARSE_MULTI_CONTENT_TYPE);
    }
    LOG_DEBUG("get multipart_name:%s", s.c_str());
    return 0;
}

int ss_parse_disposition_value(const std::string &input, std::string &output) {
    std::vector<std::string> name_tokens;
    ss_split_str(input, '=', name_tokens);
    if (name_tokens.size() != 2) {
        LOG_ERROR("ss_parse_multi_disposition_name(in name) err, input:%s", input.c_str());
        return -1;
    } 
    output = name_tokens[1];
    if (output == "\"\"") {
        LOG_DEBUG("filename is empty, input:%s", input.c_str());
        output = "";
        return 0;
    }
    output = output.substr(1, output.size() - 2); // remove ""
    if (output.empty()) {
        LOG_ERROR("ss_parse_multi_disposition_name(in name) err, name is empty");
        return -1;
    }
    return 0;
}

// parse data like "form-data; name="files"; filename="test_thread_cancel.cc""
int ss_parse_multi_disposition(const std::string &input, FileItem &item) {
    std::vector<std::string> pos_tokens;
    ss_split_str(input, ';', pos_tokens);
    if (pos_tokens.size() < 2) {
        LOG_ERROR("ss_parse_multi_disposition_name err, input:%s", input.c_str());
        return -1;
    }
    std::string fieldname;
    int ret = ss_parse_disposition_value(pos_tokens[1], fieldname); 
    CHECK_RET(ret, "parse disposition fieldname error!, input:%s", input.c_str());
    item.set_name(fieldname);
    
    if (pos_tokens.size() >=3) {
        item.set_is_file();
        std::string filename;
        ret = ss_parse_disposition_value(pos_tokens[2], filename); 
        CHECK_RET(ret, "parse disposition filename error!, input:%s", input.c_str());
        item.set_filename(filename);
    }
    return 0;
}

int ss_on_multipart_value(multipart_parser* p, const char *at, size_t length) {
    std::string s;
    s.assign(at, length);
    Request *req = (Request *)multipart_parser_get_data(p);
    std::vector<FileItem> *items = req->get_body()->get_file_items();
    if (items->empty()) {
        LOG_ERROR("items is empty in parse multi value!");
        return -1;
    }
    FileItem &item = (*items)[items->size() - 1];
    if (item.get_parse_state() == PARSE_MULTI_DISPOSITION) {
        int ret = ss_parse_multi_disposition(s, item);
        CHECK_RET(ret, "parse multi dispostion err:%d, input:%s", ret, s.c_str());
    }
    if (item.get_parse_state() == PARSE_MULTI_CONTENT_TYPE) {
        item.set_content_type(at, length);
    }
    LOG_DEBUG("get multipart_value:%s", s.c_str());
    return 0;
}

int ss_on_multipart_data(multipart_parser* p, const char *at, size_t length) {
    if (length == 0) {
        LOG_DEBUG("multipart data is empty, len:%lu", length);
        return 0;
    }
    
    Request *req = (Request *)multipart_parser_get_data(p);
    std::vector<FileItem> *items = req->get_body()->get_file_items();
    CHECK_ERR(items->empty(), "items is empty!, length:%lu", length); 

    FileItem &item = (*items)[items->size() - 1];
    item.append_data(at, length);

    LOG_DEBUG("on multipart data for name:%s, len:%lu", 
            item.get_fieldname()->c_str(), length);
    return 0;
}

int ss_on_multipart_data_over(multipart_parser* p) {
    Request *req = (Request *)multipart_parser_get_data(p);
    std::vector<FileItem> *items = req->get_body()->get_file_items();
    CHECK_ERR(items->empty(), "items is empty!, items size:%lu", items->size()); 
    
    FileItem &item = (*items)[items->size() - 1];
    item.set_parse_state(PARSE_MULTI_OVER); 
    return 0;
}

int ss_on_multipart_body_end(multipart_parser* p) {
    Request *req = (Request *)multipart_parser_get_data(p);
    LOG_DEBUG("get multipart_body end, params size:%lu", req->get_body()->get_file_items()->size());
    return req->get_body()->parse_multi_params();
}

// parse multipart data like "Content-Type:multipart/form-data; boundary=----GKCBY7qhFd3TrwA"
int ss_parse_multipart_data(Request *req) {
    std::string *body = req->get_body()->get_raw_string();
    if (body->empty()) {
        LOG_DEBUG("multipart data is empty, url:%s", req->get_request_url().c_str());
        return 0;
    }
    // parse boundary
    std::string ct = req->get_header("Content-Type");
    std::vector<std::string> ct_tokens;
    int ret = ss_split_str(ct, ';', ct_tokens);
    if (ret != 0 || ct_tokens.size() != 2) {
        LOG_ERROR("parse multipart data content type err:%d, ct:%s", ret, ct.c_str());
        return ret;
    }
    std::vector<std::string> boundary_tokens;
    ret = ss_split_str(ct_tokens[1], '=', boundary_tokens);
    if (ret != 0 || boundary_tokens.size() != 2) {
        LOG_ERROR("parse multipart data(boundary) content type err:%d, ct:%s", ret, ct.c_str());
        return ret;
    }
    std::string boundary = "--" + boundary_tokens[1];
    LOG_DEBUG("get url:%s, bounday:%s", req->get_request_url().c_str(), boundary.c_str());

    multipart_parser_settings mp_settings;
    memset(&mp_settings, 0, sizeof(multipart_parser_settings));
    mp_settings.on_header_field = ss_on_multipart_name;
    mp_settings.on_header_value = ss_on_multipart_value;
    mp_settings.on_part_data = ss_on_multipart_data;
    mp_settings.on_part_data_end = ss_on_multipart_data_over;
    mp_settings.on_body_end = ss_on_multipart_body_end;

    multipart_parser* parser = multipart_parser_init(boundary.c_str(), &mp_settings);
    multipart_parser_set_data(parser, req);
    size_t parsed = multipart_parser_execute(parser, body->c_str(), body->size());
    if (parsed != body->size()) {
        LOG_WARN("parse multipart data err, parsed:%lu, total:%lu, url:%s", 
                parsed, body->size(), req->get_request_url().c_str());
    }
    LOG_DEBUG("multipart_parser_execute, parsed:%lu, total:%lu", 
                parsed, body->size());
    multipart_parser_free(parser);

    return 0;
}

int ss_on_message_complete(http_parser *p) {
    Request *req = (Request *) p->data;
    std::string ct_header = req->get_header("Content-Type");
    // parse body params
    if (ct_header == "application/x-www-form-urlencoded") {
        std::string *raw_str = req->get_body()->get_raw_string();
        if (raw_str->size()) {
            req->get_body()->get_req_params()->parse_query_url(*raw_str);
        }
    }
    if (ct_header.find("multipart/form-data") != std::string::npos) {
        LOG_DEBUG("start parse multipart data! content type:%s", ct_header.c_str());
        ss_parse_multipart_data(req);
    }
    req->_parse_part = PARSE_REQ_OVER;
    LOG_DEBUG("msg COMPLETE!");
    return 0;
}

Request::Request() {
    _parse_part = PARSE_REQ_LINE;
    _total_req_size = 0;
    _last_was_value = true; // add new field for first
    _parse_err = 0;
    _client_ip = NULL;

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
    _total_req_size += read_size;
    if (_total_req_size > MAX_REQ_SIZE) {
        LOG_INFO("TOO BIG REQUEST WE WILL REFUSE IT!");
        return -1;
    }
    LOG_DEBUG("read from client: size:%d, content:%s", read_size, read_buffer);
    ssize_t nparsed = http_parser_execute(&_parser, &_settings, read_buffer, read_size);
    if (nparsed != read_size) {
        std::string err_msg = "unkonw";
        if (_parser.http_errno) {
            err_msg = http_errno_description(HTTP_PARSER_ERRNO(&_parser));
        }
        LOG_ERROR("parse request error! msg:%s", err_msg.c_str());
        return -1;
    }

    if (_parse_err) {
        return _parse_err;
    }
    if (_parse_part != PARSE_REQ_OVER) {
        return NEED_MORE_STATUS;
    }
    return 0;
}

RequestBody *Request::get_body() {
    return &_body;
}

std::string Request::get_method() {
    return _line.get_method();
}

std::string *Request::get_client_ip() {
    return _client_ip;
}

void Request::set_client_ip(std::string *ci) {
    this->_client_ip = ci;
}

Response::Response(CodeMsg status_code) {
    this->_code_msg = status_code;
    this->_is_writed = 0;
}

void Response::set_head(const std::string &name, const std::string &value) {
    this->_headers[name] = value;
}

void Response::set_body(Json::Value &body) {
    Json::FastWriter writer;
    std::string str_value = writer.write(body);
    this->_body = str_value;
}

int Response::gen_response(std::string &http_version, bool is_keepalive) {
    LOG_DEBUG("START gen_response code:%d, msg:%s", _code_msg.status_code, _code_msg.msg.c_str());
    _res_bytes << http_version << " " << _code_msg.status_code << " " << _code_msg.msg << "\r\n";
    _res_bytes << "Server: ehttp/" << EHTTP_VERSION << "\r\n";
    if (_headers.find("Content-Type") == _headers.end()) {
        _res_bytes << "Content-Type: application/json; charset=UTF-8" << "\r\n";
    }
    if (_headers.find("Content-Length") == _headers.end()) {
        _res_bytes << "Content-Length: " << _body.size() << "\r\n";
    }

    std::string con_status = "Connection: close";
    if(is_keepalive) {
        con_status = "Connection: Keep-Alive";
    }
    _res_bytes << con_status << "\r\n";

    for (std::map<std::string, std::string>::iterator it = _headers.begin(); it != _headers.end(); ++it) {
        _res_bytes << it->first << ": " << it->second << "\r\n";
    }
    // header end
    _res_bytes << "\r\n";
    _res_bytes << _body;

    LOG_DEBUG("gen response context:%s", _res_bytes.str().c_str());
    return 0;
}

int Response::readsome(char *buffer, int buffer_size, int &read_size) {
    _res_bytes.read(buffer, buffer_size);
    read_size = _res_bytes.gcount();

    if (!_res_bytes.eof()) {
        return 1;
    }
    return 0;
}

int Response::rollback(int num) {
    if (_res_bytes.eof()) {
        _res_bytes.clear();
    }
    int rb_pos = (int) _res_bytes.tellg() - num;
    _res_bytes.seekg(rb_pos);
    return _res_bytes.good() ? 0 : -1;
}

HttpContext::HttpContext(int fd) {
    this->_fd = fd;
    _req = new Request();
    _res = new Response();
}

HttpContext::~HttpContext() {
    delete_req_res();
}

int HttpContext::record_start_time() {
    gettimeofday(&_start, NULL);
    return 0;
}

int HttpContext::get_cost_time() {
    timeval end;
    gettimeofday(&end, NULL);
    int cost_time = (end.tv_sec - _start.tv_sec) * 1000000 + (end.tv_usec - _start.tv_usec);
    return cost_time;
}

void HttpContext::print_access_log(std::string &client_ip) {
    std::string http_method = this->_req->_line.get_method();
    std::string request_url = this->_req->_line.get_request_url();
    int cost_time = get_cost_time();
    LOG_INFO("access_log %s %s status_code:%d cost_time:%d us, body_size:%d, client_ip:%s",
            http_method.c_str(), request_url.c_str(), _res->_code_msg.status_code,
            cost_time, _res->_body.size(), client_ip.c_str());
}

inline void HttpContext::delete_req_res() {
    if (_req != NULL) {
        delete _req;
        _req = NULL;
    }
    if (_res != NULL) {
        delete _res;
        _res = NULL;
    }
}

void HttpContext::clear() {
    delete_req_res();
    _req = new Request();
    _res = new Response();
}

Response &HttpContext::get_res() {
    return *_res;
}

Request &HttpContext::get_request() {
    return *_req;
}
