#include "gtest/gtest.h"

#include "epoll_socket.h"
#include "simple_log.h"

TEST(EpollSocketTest, test_clear_idle_clients) {
    EpollSocket es;
    EpollContext *ctx = es.create_client(0, "127.0.0.1");
    es.add_client(ctx);
    ASSERT_EQ(1, (int)es.get_clients().size());

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
    int ret = es.start_epoll();
    ASSERT_EQ(-1, ret); // mock epoll_wait
}
