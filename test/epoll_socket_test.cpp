#include "gtest/gtest.h"

#include "epoll_socket.h"
#include "simple_log.h"

/*
TEST(EpollSocketTest, test_accept_socket) {
    set_log_level("WARN");
    EpollSocket es;
    int sockfd = 0;
    std::string client_ip = "127.0.0.1";
    int ret = es.accept_socket(sockfd, client_ip);
    ASSERT_EQ(0, ret);
}
*/

class MockEpollWatcher : public EpollSocketWatcher {
    public:
        virtual ~MockEpollWatcher() {}

        virtual int on_accept(EpollContext &epoll_context) {
            return 0;
        }

        virtual int on_readable(int &epollfd, epoll_event &event) {
            return 0;
        }

        virtual int on_writeable(EpollContext &epoll_context) {
            return WRITE_CONN_ALIVE;
        }

        virtual int on_close(EpollContext &epoll_context) {
            return 0;
        }
};

TEST(EpollSocketTest, test_handle_accept_event) {
    set_log_level("WARN");
    EpollSocket es;
    int epollfd = 0;
    epoll_event accept_event; 
    MockEpollWatcher watcher;
    int ret = es.handle_accept_event(epollfd, accept_event, watcher);
    ASSERT_EQ(0, ret);
}

TEST(EpollSocketTest, test_clear_idle_clients) {
    set_log_level("WARN");
    EpollSocket es;
    EpollContext *ctx = es.create_client(0, "127.0.0.1");
    std::string ctx_str = ctx->to_string();
    ASSERT_TRUE(!ctx_str.empty());

    es.add_client(ctx);
    ASSERT_EQ(1, (int)es.get_clients().size());

    std::stringstream ss;
    int ret = es.get_clients_info(ss);
    ASSERT_EQ(0, ret);
    ASSERT_TRUE(ss.str().size() > 0);

    es.set_client_max_idle_time(3);
    time_t now = time(NULL);
    es.update_interact_time(ctx, now - 5);
    std::stringstream client_info;
    /*
    es.get_clients_info(client_info);
    LOG_INFO("before clear info:%s, now:%jd", 
            client_info.str().c_str(), now);
    */
    int num = es.clear_idle_clients();
    ASSERT_EQ(1, num);
    ASSERT_EQ(0, (int)es.get_clients().size());

    EpollContext *ctx2 = es.create_client(0, "127.0.0.1");
    es.add_client(ctx2);
    ASSERT_EQ(1, (int)es.get_clients().size());
    es.remove_client(ctx2);
    ASSERT_EQ(0, (int)es.get_clients().size());
}

TEST(EpollSocketTest, test_handle_writeable_event) {
    EpollContext *ctx = new EpollContext();
    EpollSocket es;
    int epollfd = 0;
    epoll_event ee;
    ee.data.ptr = ctx;
    ee.events = 0;
    MockEpollWatcher w;
    int ret = es.handle_writeable_event(epollfd, ee, w);
    ASSERT_EQ(0, ret);
}

TEST(EpollSocketTest, test_start_epoll) {
    EpollSocket es;
    es.set_port(3456);
    es.set_backlog(1000);
    es.set_max_events(100);
    es.add_bind_ip("127.0.0.1");
    int ret = es.start_epoll();
    ASSERT_EQ(-1, ret); // mock epoll_wait
    ret = es.stop_epoll();
    ASSERT_EQ(0, ret); // mock epoll_wait
}

class TestWatcher : public EpollSocketWatcher {
    public:
        virtual int on_accept(EpollContext &epoll_context) {
            return 0;
        }

        virtual int on_readable(int &epollfd, epoll_event &event) {
            return 0;
        }

        /**
         * return :
         * if return value == 1, we will close the connection
         * if return value == 2, we will continue to write
         */
        virtual int on_writeable(EpollContext &epoll_context) {
            return 1;
        }

        virtual int on_close(EpollContext &epoll_context) {
            return 0;
        }
};

TEST(EpollSocketTest, test_handle_event) {
    EpollSocket es;
    int ret = es.init_tp();
    ASSERT_EQ(0, ret); 

    epoll_event ee;
    memset(&ee, 0, sizeof(ee));
    ret = es.handle_event(ee);
    ASSERT_EQ(-1, ret); 

    ee.events = EPOLLOUT;
    es.set_watcher(new TestWatcher());
    ret = es.handle_event(ee);
    ASSERT_EQ(-1, ret); // epoll ctx is null

    EpollContext *ctx = es.create_client(0, "127.0.0.1");
    ee.data.ptr = ctx;
    ret = es.handle_event(ee);
    ASSERT_EQ(0, ret); 

    ee.data.ptr = NULL;
    ret = es.handle_readable_event(ee);
    ASSERT_EQ(-1, ret); 
    
    ee.data.ptr = ctx;
    ret = es.handle_readable_event(ee);
    ASSERT_EQ(0, ret); 
}
