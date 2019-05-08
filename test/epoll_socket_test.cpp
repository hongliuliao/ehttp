#include "gtest/gtest.h"

#include "epoll_socket.h"
#include "simple_log.h"

TEST(EpollSocketTest, test_clear_idle_clients) {
    EpollContext *ctx = new EpollContext();
    EpollSocket es;
    es.add_client(ctx);
    ASSERT_EQ(1, (int)es.get_clients()->size());

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
    ASSERT_EQ(0, (int)es.get_clients()->size());

    es.add_client(ctx);
    ASSERT_EQ(1, (int)es.get_clients()->size());
    es.remove_client(ctx);
    ASSERT_EQ(0, (int)es.get_clients()->size());
}
