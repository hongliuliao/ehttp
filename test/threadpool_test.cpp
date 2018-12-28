#include "simple_log.h"
#include "threadpool.h"
#include "gtest/gtest.h"

pthread_key_t g_tp_key;

void tp_start_cb() {
    pthread_t t = pthread_self();
    LOG_INFO("start thread data function , tid:%u", t);
    unsigned long *a = new unsigned long();
    *a = t;

    pthread_setspecific(g_tp_key, a);
}

void tp_exit_cb() {
    pthread_t t = pthread_self();
    LOG_INFO("exit thread cb, tid:%u", t);

    void *v = pthread_getspecific(g_tp_key);
    ASSERT_TRUE(v != NULL);

    unsigned long *a = (unsigned long *)v;
    LOG_INFO("exit thread cb, tid:%u, data:%u", t, *a);
    ASSERT_EQ(t, *a);

    delete a;
}

TEST(ThreadPoolTest, test_tp_cb) {
    pthread_key_create(&g_tp_key,NULL);
    set_log_level("WARN");

    ThreadPool tp;
    tp.set_thread_start_cb(tp_start_cb);
    tp.set_thread_exit_cb(tp_exit_cb);
    tp.set_pool_size(4);
    int ret = tp.start_threadpool();
    ASSERT_EQ(0, ret);

    sleep(1); // sleep a moment to make sure all threads start

    ret = tp.destroy_threadpool(); 
    ASSERT_EQ(0, ret);
}

TEST(ThreadPoolTest, test_tp_exit) {
    // add a thread pool not init
    ThreadPool tp2;
}

