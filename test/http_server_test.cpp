/*
 * http_server_test.cpp
 *
 *  Created on: Oct 26, 2014
 *      Author: liao
 */
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include "simple_log.h"
#include "http_server.h"
#include "threadpool.h"

pthread_key_t g_tp_key;

void a_test_fn() {
    pthread_t t = pthread_self();
    LOG_INFO("start thread data function , tid:%u", t);
    unsigned long *a = new unsigned long();
    *a = t;

    pthread_setspecific(g_tp_key, a);
}

void hello(Request &request, Json::Value &root) {
	root["hello"] = "world";
    
    LOG_INFO("get client ip:%s", request.get_client_ip()->c_str());
    pthread_t t = pthread_self();
    int *tmp = (int*)pthread_getspecific(g_tp_key);
    if (tmp == NULL) {
        LOG_INFO("not thread data, tid:%u", t);
        return;
    }
    LOG_INFO("get thread data:%lu", *tmp);
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

    //pthread_key_create(&g_tp_key,NULL);
    //
    //ThreadPool tp;
    //tp.set_thread_start_cb(a_test_fn);
    //tp.set_pool_size(4);

    HttpServer http_server;
    //http_server.set_thread_pool(&tp);

    http_server.add_mapping("/hello", hello);
    http_server.add_mapping("/usleep", usleep);
    http_server.add_mapping("/sayhello", sayhello);
    http_server.add_mapping("/login", login, GET_METHOD | POST_METHOD);

    http_server.add_bind_ip("127.0.0.1");
    http_server.set_port(atoi(args[1]));
    http_server.set_backlog(100000);
    http_server.set_max_events(100000);
    //http_server.add_bind_ip("192.168.238.158");
    http_server.start_async();
    //sleep(1);
    //http_server.stop();
    http_server.join();
    return 0;
}
