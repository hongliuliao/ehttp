/*
 * http_parser_test.cpp
 *
 *  Created on: Oct 30, 2014
 *      Author: liao
 */
#include "sim_parser.h"
#include "http_parser.h"
#include "simple_log.h"

#include "gtest/gtest.h"

const std::string TEST_GET_REQ = "GET /hello?name=aaa&kw=%E4%B8%89%E6%98%8E&m-name=a1&m-name=a2 HTTP/1.1\r\n" \
                             "Host: api.yeelink.net\r\n" \
                             "U-ApiKey: 121234132432143\r\n" \
                             "\r\n";

const std::string TEST_POST_REQ = "POST /login?u=a HTTP/1.0\r\n" \
                                  "Content-Type: application/x-www-form-urlencoded\r\n" \
                                  "Content-Length: 14\r\n" \
                                  "\r\n" \
                                  "name=aa&pwd=xx";

const std::string TEST_POST_REQ_LOW_LEN = "POST /login?u=a HTTP/1.0\r\n" \
                                  "Content-Type: application/x-www-form-urlencoded\r\n" \
                                  "Content-length: 14\r\n" \
                                  "\r\n" \
                                  "name=aa&pwd=xx";

const std::string TEST_POST_REQ1 = "POST /lo";
const std::string TEST_POST_REQ2 = 
                                   "Hgin HTTP/1.0\r\nContent-Type: application/x-www-form-"
                                   "urlencoded\r\n"
                                   "Content-Length: 14\r\n"
                                   "\r\n"
                                   "name=aa&pwd=xx";

const std::string TEST_MULTIPART = "POST /login HTTP/1.0\r\n" \
                                  "Content-Type: multipart/form-data; boundary=----WebKitFormBoundaryrGKCBY7qhFd3TrwA\r\n" \
                                  "Content-length: 323\r\n" \
                                  "\r\n" \
                                  "------WebKitFormBoundaryrGKCBY7qhFd3TrwA\r\n" \
                                  "Content-Disposition: form-data; name=\"user_name\"\r\n"
                                  "\r\n" \
                                  "title\r\n" \
                                  "------WebKitFormBoundaryrGKCBY7qhFd3TrwA\r\n" \
                                  "Content-Disposition: form-data; name=\"user_photo\"; filename=\"chrome.png\"\r\n" \
                                  "Content-Type: image/png\r\n" \
                                  "\r\n" \
                                  "PNG ... content of chrome.png ...\r\n" \
                                  "\r\n" \
                                  "------WebKitFormBoundaryrGKCBY7qhFd3TrwA--";

int on_message_begin(http_parser *p) {
    LOG_INFO("MESSAGE START!");
    return 0;
}

int on_status(http_parser *p, const char *buf, size_t len) {
    std::string status;
    status.assign(buf, len);
    LOG_INFO("on status:%s!", status.c_str());
    return 0;
}

int request_url_cb (http_parser *p, const char *buf, size_t len) {
    std::string url; 
    url.assign(buf, len);
    LOG_INFO("GET URL:%s", url.c_str());
    return 0;
}

int header_field_cb (http_parser *p, const char *buf, size_t len) {
    LOG_INFO("method:%d,http_major:%d,http_minor:%d ", p->method, p->http_major, p->http_minor);
    std::string field;
    field.assign(buf, len);
    LOG_INFO("GET field:%s", field.c_str());
    return 0;
}

int header_value_cb (http_parser *p, const char *buf, size_t len) {
    std::string field;
    field.assign(buf, len);
    LOG_INFO("GET value:%s", field.c_str());
    return 0;
}

int on_headers_complete(http_parser *p) {
    LOG_INFO("HEADERS COMPLETE!");
    return 0;
}

int on_body(http_parser *p, const char *buf, size_t len) {
    std::string field;
    field.assign(buf, len);
    LOG_INFO("GET body:%s", field.c_str());
    return 0;
}

int on_message_complete(http_parser *p) {
    LOG_INFO("msg COMPLETE! content-len:%d", p->content_length);
    return 0;
}

int on_chunk_header(http_parser *p) {
    LOG_INFO("on chunk header");
    return 0;
}

TEST(RequestTest, test_parse_get) {
    set_log_level("WARN");

    Request req;
    int ret = req.parse_request(TEST_GET_REQ.c_str(), TEST_GET_REQ.size());
    if (ret != 0) {
        LOG_ERROR("PARSE request error which ret:%d, req str:%s", ret, TEST_GET_REQ.c_str());
    }
    ASSERT_EQ(0, ret);

    std::string name = req.get_param("name");
    std::string kw = req.get_unescape_param("kw");
    LOG_INFO("Get 'name' param :%s", name.c_str());
    LOG_INFO("Get 'kw' param :%s", kw.c_str());
    ASSERT_STREQ("aaa", name.c_str());
    ASSERT_STREQ("三明", kw.c_str());
    std::vector<std::string> params;
    req.get_params("name", params);
    ASSERT_EQ(1, (int) params.size());
    std::string uri = req.get_request_uri();
    ASSERT_EQ("/hello", uri);
    ASSERT_EQ("", req.get_param("nofound"));
    std::vector<std::string> mnames;
    req.get_params("m-name", mnames);
    ASSERT_EQ(2, mnames.size());
    // client ip
    std::string client_ip = "127.0.0.1";
    req.set_client_ip(&client_ip);
    ASSERT_EQ(client_ip, *(req.get_client_ip()));
}

TEST(RequestTest, test_parse_post) {
    set_log_level("WARN");

    Request req;
    int ret = req.parse_request(TEST_POST_REQ.c_str(), TEST_POST_REQ.size());
    if (ret != 0) {
        LOG_ERROR("PARSE request error which ret:%d, req str:%s", ret, TEST_POST_REQ.c_str());
    }
    ASSERT_EQ(0, ret);
    std::string name = req.get_param("name");
    std::string pwd = req.get_param("pwd");
    LOG_INFO("Get 'name' param :%s", name.c_str());
    LOG_INFO("Get 'pwd' param :%s", pwd.c_str());
    ASSERT_STREQ("aa", name.c_str());
    ASSERT_STREQ("xx", pwd.c_str());
}

TEST(HttpParserTest, test_parse_request) {
    http_parser_settings settings;
    settings.on_message_begin = NULL;
    settings.on_url = request_url_cb;
    settings.on_status = on_status;
    settings.on_header_field = header_field_cb;
    settings.on_header_value = header_value_cb;
    settings.on_headers_complete = on_headers_complete;
    settings.on_body = on_body;
    settings.on_message_complete = on_message_complete;
    settings.on_chunk_header = NULL;
    settings.on_chunk_complete = NULL;

    http_parser parser;
    http_parser_init(&parser, HTTP_REQUEST);
    Request req;
    parser.data = &req;
    int nparsed = http_parser_execute(&parser, &settings, TEST_POST_REQ.c_str(), TEST_POST_REQ.size());
    LOG_INFO("parsed size:%d, parser->upgrade:%d,total_size:%u", nparsed, parser.upgrade, TEST_POST_REQ.size());
    if (parser.http_errno) {
        LOG_INFO("ERROR:%s", http_errno_description(HTTP_PARSER_ERRNO(&parser)));
    }
    ASSERT_EQ(TEST_POST_REQ.size(), nparsed);
    ASSERT_EQ(0, parser.http_errno);
}

TEST(RequestTest, test_http_parser_stream) {
    set_log_level("WARN");

    http_parser_settings settings;
    settings.on_message_begin = on_message_begin;
    settings.on_url = request_url_cb;
    settings.on_status = on_status;
    settings.on_header_field = header_field_cb;
    settings.on_header_value = header_value_cb;
    settings.on_headers_complete = on_headers_complete;
    settings.on_body = on_body;
    settings.on_message_complete = on_message_complete;
    settings.on_chunk_header = NULL;
    settings.on_chunk_complete = NULL;

    http_parser parser;
    http_parser_init(&parser, HTTP_REQUEST);
    Request req;
    parser.data = &req;

    size_t nparsed = http_parser_execute(&parser, &settings, TEST_POST_REQ1.c_str(), TEST_POST_REQ1.size());
    LOG_INFO("parsed size:%d, parser->upgrade:%d,total_size:%u", nparsed, parser.upgrade, TEST_POST_REQ1.size());
    ASSERT_EQ(TEST_POST_REQ1.size(), nparsed);
    if (parser.http_errno) {
        LOG_INFO("ERROR:%s", http_errno_description(HTTP_PARSER_ERRNO(&parser)));
    }
    ASSERT_EQ((unsigned int)0, parser.http_errno);

    nparsed = http_parser_execute(&parser, &settings, TEST_POST_REQ2.c_str(), TEST_POST_REQ2.size());
    LOG_INFO("parsed size:%d, parser->upgrade:%d,total_size:%u", nparsed, parser.upgrade, TEST_POST_REQ2.size());
    ASSERT_EQ(TEST_POST_REQ2.size(), nparsed);
    if (parser.http_errno) {
        LOG_INFO("ERROR:%s", http_errno_description(HTTP_PARSER_ERRNO(&parser)));
    }
    ASSERT_EQ((unsigned int)0, parser.http_errno);
}

TEST(RequestTest, test_http_parser_post) {
    http_parser_settings settings;
    settings.on_message_begin = NULL;
    settings.on_url = request_url_cb;
    settings.on_status = on_status;
    settings.on_header_field = ss_on_header_field;
    settings.on_header_value = ss_on_header_value;
    settings.on_headers_complete = ss_on_headers_complete;
    settings.on_body = ss_on_body;
    settings.on_message_complete = on_message_complete;
    settings.on_chunk_header = NULL;
    settings.on_chunk_complete = NULL;

    http_parser parser;
    http_parser_init(&parser, HTTP_REQUEST);
    Request req;
    parser.data = &req;
    int nparsed = http_parser_execute(&parser, &settings, 
            TEST_POST_REQ_LOW_LEN.c_str(), TEST_POST_REQ_LOW_LEN.size());
    LOG_INFO("parsed size:%d, parser->upgrade:%d,total_size:%u", 
            nparsed, parser.upgrade, TEST_POST_REQ_LOW_LEN.size());
    if (parser.http_errno) {
        LOG_INFO("ERROR:%s", http_errno_description(HTTP_PARSER_ERRNO(&parser)));
    }
    ASSERT_EQ(0, (int)parser.http_errno);
}

TEST(RequestTest, test_parse_multipart_req) {
    //set_log_level("DEBUG");
    Request req;
    int ret = req.parse_request(TEST_MULTIPART.c_str(), (int)TEST_MULTIPART.size());
    ASSERT_EQ(0, ret);
    std::vector<FileItem> *items = req.get_body()->get_file_items();
    ASSERT_EQ(2, (int)items->size());
}

TEST(ResponseTest, test_gen_response) {
    CodeMsg cm = {0, "OK"};
    Response rsp(cm);
    rsp.set_head("Allow", "GET");
    Json::Value root;
    root["code"] = 0;
    rsp.set_body(root);
    int ret = rsp.gen_response("HTTP/1.0", false);
    ASSERT_EQ(0, ret);
    char buf[1024];
    int read_size;
    ret = rsp.readsome(buf, sizeof(buf), read_size);
    ASSERT_EQ(0, ret); // read eof
    ret = rsp.rollback(10); // rollback a little
    ASSERT_EQ(0, ret);
    int except_size = 5;
    ret = rsp.readsome(buf, except_size, read_size); // read a little which not reach eof
    ASSERT_EQ(NEED_MORE_STATUS, ret);
    ASSERT_EQ(except_size, read_size);
}

TEST(HttpContextTest, test_print_access_log) {
    //set_log_level("INFO");
    HttpContext hc(0); // use stdin fd
    hc.record_start_time();
    int ret = hc.get_request().parse_request(TEST_GET_REQ.c_str(), TEST_GET_REQ.size());
    ASSERT_EQ(0, ret);
    ret = hc.get_res().gen_response("HTTP/1.0", false);
    ASSERT_EQ(0, ret);
    ret = hc.print_access_log("127.0.0.1");
    ASSERT_EQ(0, ret);
}

