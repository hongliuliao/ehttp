/*
 * http_server_test.cpp
 *
 *  Created on: Oct 26, 2014
 *      Author: liao
 */
#include <sstream>
#include <cstdlib>
#include "simple_log.h"
#include "http_server.h"

void hello(Request &request, Json::Value &root) {
	root["hello"] = "world";
}

void sayhello(Request &request, Json::Value &root) {
	std::string name = request.get_param("name");
	std::string age = request.get_param("age");

	root["name"] = name;
	root["age"] = atoi(age.c_str());
}

void login(Request &request, Json::Value &root) {
	std::string name = request.get_param("name");
	std::string pwd = request.get_param("pwd");

	LOG_DEBUG("login user which name:%s, pwd:%s", name.c_str(), pwd.c_str());
	root["code"] = 0;
	root["msg"] = "login success!";
}

void usleep(Request &request, Response &response) {
    Json::Value root;
    std::string sleep_time = request.get_param("usleep");
    if (sleep_time.empty()) {
        root["msg"] = "usleep is empty!";
        response.set_body(root);
        return;
    }
    usleep(atoi(sleep_time.c_str()));
    root["code"] = 0;
    root["msg"] = "success!";
    response.set_body(root);
}

void test_schedule() {
    LOG_INFO("START schedule ....");
}

int main(int argc, char **args) {
    if (argc < 2) {
        LOG_ERROR("usage: ./http_server_test [port]");
        return -1;
    }
	HttpServer http_server;

	http_server.add_mapping("/hello", hello);
	http_server.add_mapping("/usleep", usleep);
	http_server.add_mapping("/sayhello", sayhello);
	http_server.add_mapping("/login", login, POST_METHOD);
	//http_server.set_schedule(test_schedule);

	int port = atoi(args[1]);
	int backlog = 100000;
	int max_events = 1000;

	http_server.start(port, backlog, max_events);
	return 0;
}
