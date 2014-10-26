/*
 * http_server_test.cpp
 *
 *  Created on: Oct 26, 2014
 *      Author: liao
 */
#include <sstream>
#include "simple_log.h"
#include "http_server.h"

Response hello(Request &request) {
	return Response(200, "hello world! \n");
}

Response sayhello(Request &request) {
	LOG_DEBUG("start process request...");

	std::string name = request.get_param_by_name("name");
	std::string age = request.get_param_by_name("age");

	std::stringstream ss;
	ss << "hello " << name << ", age:" + age << "\n";

	return Response(200, ss.str());
}

int main() {
	HttpServer http_server;

	http_server.add_mapping("/hello", hello);
	http_server.add_mapping("/sayhello", sayhello);

	http_server.start(3490);
	return 0;
}
