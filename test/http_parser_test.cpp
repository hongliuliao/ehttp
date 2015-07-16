/*
 * http_parser_test.cpp
 *
 *  Created on: Oct 30, 2014
 *      Author: liao
 */
#include "http_parser.h"
#include "simple_log.h"

int test_split() {
    std::string temp = "  aaa:bbb\r\n";
    std::vector<std::string> parts;
    split_str(parts, temp, ':');
    LOG_DEBUG("parts[0]:%s", parts[0].c_str())
    LOG_DEBUG("parts[1]:%s", parts[1].c_str())
}

const std::string test_req = "GET /hello?name=aaa&kw=%E4%B8%89%E6%98%8E HTTP/1.1\r\n" \
                             "Host: api.yeelink.net\r\n" \
                             "U-ApiKey: 121234132432143\r\n" \
                             "\r\n";

int test_get_unescape() {
    Request req;
    int ret = req.parse_request(test_req.c_str(), test_req.size());
    if (ret != 0) {
        LOG_ERROR("PARSE request error which ret:%d, req str:%s", ret, test_req.c_str());
        return ret;
    }
    std::string value = req.get_param("name");
    LOG_INFO("Get 'name' param :%s", value.c_str());
    LOG_INFO("Get 'kw' param :%s", req.get_unescape_param("kw").c_str());
    return 0;
}

int main () {
    test_split();
    test_get_unescape();
	return 0;
}
