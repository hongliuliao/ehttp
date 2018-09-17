/*
 * http_server_test.cpp
 *
 *  Created on: Oct 26, 2014
 *      Author: liao
 */
#include <sstream>
#include <cstdlib>
#include <unistd.h>

#include "gtest/gtest.h"

#include "simple_log.h"
#include "http_server.h"
#include "threadpool.h"

pthread_key_t g_tp_key;

void test_start_fn() {
    pthread_t t = pthread_self();
    LOG_INFO("start thread data function , tid:%u", t);
    unsigned long *a = new unsigned long();
    *a = t;

    pthread_setspecific(g_tp_key, a);
}

void test_exit_fn() {
    pthread_t t = pthread_self();
    LOG_INFO("exit thread data function , tid:%u", t);

    void *v = pthread_getspecific(g_tp_key);
    ASSERT_TRUE(v != NULL);

    unsigned long *a = (unsigned long *)v;
    LOG_INFO("exit thread cb, tid:%u, data:%u", t, *a);
    ASSERT_EQ(t, *a);

    delete a; // free resources
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

int main(int argc, char **args) {
    int ret = log_init("./conf", "simple_log.conf");
    if (ret != 0) {
        printf("log init error!");
        return 0;
    }
    if (argc < 2) {
        LOG_ERROR("usage: ./http_multi_thread_demo [port]");
        return -1;
    }

    pthread_key_create(&g_tp_key,NULL);
    
    ThreadPool tp;
    tp.set_thread_start_cb(test_start_fn);
    tp.set_thread_exit_cb(test_exit_fn);
    tp.set_pool_size(4);

    HttpServer http_server;
    http_server.set_thread_pool(&tp);

    http_server.add_mapping("/hello", hello);

    http_server.add_bind_ip("127.0.0.1");
    http_server.set_port(atoi(args[1]));
    http_server.set_backlog(100000);
    http_server.set_max_events(100000);
    http_server.start_async();
    //sleep(1);
    http_server.join();
    return 0;
}
