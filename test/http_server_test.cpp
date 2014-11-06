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

Response hello(Request &request) {
	Json::Value root;
	root["hello"] = "world";
	return Response(STATUS_OK, root);
}

Response sayhello(Request &request) {
	std::string name = request.get_param("name");
	std::string age = request.get_param("age");

	Json::Value root;
	root["name"] = name;
	root["age"] = atoi(age.c_str());
	return Response(STATUS_OK, root);
}

Response login(Request &request) {
	std::string name = request.get_param("name");
	std::string pwd = request.get_param("pwd");

	LOG_DEBUG("login user which name:%s, pwd:%s", name.c_str(), pwd.c_str());
	Json::Value root;
	root["code"] = 0;
	root["msg"] = "login success!";
	return Response(STATUS_OK, root);
}

int main() {
	HttpServer http_server;

	http_server.add_mapping("/hello", hello);
	http_server.add_mapping("/sayhello", sayhello);
	http_server.add_mapping("/login", login, POST_METHOD);

	http_server.start(3490);
	return 0;
}
