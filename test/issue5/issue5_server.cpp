/*
 * http_server_test.cpp
 *
 *  Created on: Oct 26, 2014
 *      Author: liao
 */
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include "simple_log.h"
#include "http_server.h"
#include "threadpool.h"

void file_action(Request &request, Json::Value &root) {
	/*
    std::string fileId = request.get_param("fileId");
	std::string files = request.get_param("files");
	std::string files2 = request.get_param("files2");

	LOG_INFO("get fileId:%s", fileId.c_str());
	LOG_INFO("get files:%s", files.c_str());
	LOG_INFO("get files2:%s", files2.c_str());
    */
    // get file items
    std::vector<FileItem> *items = request.get_body()->get_file_items();
    for (size_t i = 0; i < items->size(); i++) {
        FileItem &item = (*items)[i];
        if (item.is_file()) {
            LOG_INFO("item name:%s, filename:%s, content-type:%s, data:%d", 
                    item.get_fieldname()->c_str(), item.get_filename()->c_str(),
                    item.get_content_type()->c_str(), item.get_data()->size());
        } else {
            LOG_INFO("item name:%s, data:%s", 
                    item.get_fieldname()->c_str(), item.get_data()->c_str());
        }
    }
	root["code"] = 0;
	root["msg"] = "login success!";
}

void response_file(Request &request, Response &response) {
    Json::Value root;
    std::string file_path = "." + request.get_request_uri();
    std::ifstream is(file_path.c_str());
    if (!is.good()) {
        root["code"] = -1;
        root["msg"] = "file not found:" + file_path;
        response.set_body(root);
        return;
    }
    std::stringstream ss;
    ss << is.rdbuf();//read the file
    std::string file_content = ss.str();
    response._body = file_content;
    response.set_head("Content-Type", "text/html");
}

int main(int argc, char **args) {
    int ret = log_init("./conf", "simple_log.conf");
    if (ret != 0) {
        printf("log init error!");
        return 0;
    }
    if (argc < 2) {
        LOG_ERROR("usage: ./http_server_test [port]");
        return -1;
    }

    HttpServer http_server;

    http_server.add_mapping("/test/issue5/issue5.html", response_file);
    http_server.add_mapping("/file_action", file_action, POST_METHOD);

    http_server.set_port(atoi(args[1]));
    http_server.set_backlog(100000);
    http_server.set_max_events(100000);
    http_server.start_sync();
    return 0;
}
