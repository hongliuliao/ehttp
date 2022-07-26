/***************************************************************************
 * 
 * Copyright (c) 2020 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file test/http_server_test.cpp
 * @author liaohongliu(bdrp@baidu.com)
 * @date 2020/08/20 14:04:36
 * @brief 
 *  
 **/
#include "gtest/gtest.h"
#include "http_server.h"

TEST(HttpServerTest, test_add_buildin_mappings) {
    HttpServer server;
    int ret = server.add_buildin_mappings();
    ASSERT_EQ(0, ret);
}

void method_handler_test(Request& /*request*/, Response& /*response*/) {
}

void json_handler_test(Request& /*request*/, Json::Value& /*response*/) {
}

class JsonTestHandler : public HttpJsonHandler {
    public:
        int action(Request &/*req*/, Json::Value &/*root*/) {
            return 0;
        }
};

TEST(HttpServerTest, test_add_mappings) {
    HttpServer server;
    std::string method_path = "/test1";
    int ret = server.add_mapping(method_path, method_handler_test);
    ASSERT_EQ(0, ret);
    ret = server.add_mapping(method_path, method_handler_test);
    ASSERT_EQ(-1, ret);
    
    ret = server.add_mapping(method_path, json_handler_test);
    ASSERT_EQ(-1, ret);
    
    JsonTestHandler *handler = new JsonTestHandler();
    ret = server.add_mapping(method_path, handler);
    ASSERT_EQ(-1, ret);
}

TEST(HttpServerTest, test_start) {
    set_log_level("WARN");
    HttpServer server;
    int ret = server.add_mapping("/test2", json_handler_test, GET_METHOD | POST_METHOD);
    ASSERT_EQ(0, ret);

    server.add_bind_ip("127.0.0.1");
    server.set_backlog(100);
    server.set_max_events(1000);
    server.set_port(1122);
    server.set_client_max_idle_time(10);
    ret = server.start_async();
    if (!server.is_running()) {
        usleep(100 * 1000); // 100ms
    }
    server.stop();
    server.join();
    ASSERT_EQ(0, ret);
}

TEST(HttpEpollWatcherTest, test_handle_request) {
    std::map<std::string, Resource> resource_map;
    HttpEpollWatcher watcher(&resource_map);
    Request req;
    Response rsp;
    int ret = watcher.handle_request(req, rsp);
    ASSERT_EQ(0, ret);
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
