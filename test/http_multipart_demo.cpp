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
    // get file items
    std::vector<FileItem> *items = request.get_body()->get_file_items();
    for (size_t i = 0; i < items->size(); i++) {
        FileItem &item = (*items)[i];
        if (item.is_file()) {
            LOG_INFO("item name:%s, filename:%s, content-type:%s, data:%d", 
                    item.get_fieldname()->c_str(), item.get_filename()->c_str(),
                    item.get_content_type()->c_str(), item.get_data()->size());
            std::ofstream os(item.get_filename()->c_str());
            if (os.is_open()) {
                os << *(item.get_data());
                os.close();
            }
        } else {
            LOG_INFO("item name:%s, data:%s", 
                    item.get_fieldname()->c_str(), item.get_data()->c_str());
        }
    }
    root["code"] = 0;
	root["msg"] = "upload success!";
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

void download_file(Request &request, Response &response) {
    Json::Value root;
    std::string file_name = request.get_param("file");
    std::ifstream is(file_name.c_str());
    if (!is.good()) {
        root["code"] = -1;
        root["msg"] = "file not found:" + file_name;
        response.set_body(root);
        return;
    }
    std::stringstream ss;
    ss << is.rdbuf();//read the file
    std::string file_content = ss.str();
    response._body = file_content;
    if (file_name.find(".log") != std::string::npos) {
        response.set_head("Content-Type", "text/plain"); 
    } else {
        response.set_head("Content-Type", "application/octet-stream"); 
    }
    response.set_head("content-disposition", "attachment;filename=" + file_name);
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
    // curl localhost:3456/download?file=1.txt
    http_server.add_mapping("/download", download_file); 
    http_server.add_mapping("/file_action", file_action, POST_METHOD);

    http_server.set_port(atoi(args[1]));
    http_server.set_backlog(100000);
    http_server.set_max_events(100000);
    http_server.start_sync();
    return 0;
}
