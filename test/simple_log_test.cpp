#include "gtest/gtest.h"
#include "simple_log.h"

TEST(SimpleLogTest, test_log) {
    int ret = log_init("./conf", "simple_log.conf");
    ASSERT_EQ(0, ret);
    set_log_level("DEBUG");
    LOG_DEBUG("this is a debug log");
    LOG_INFO("this is a info log");
    LOG_WARN("this is a warn log");
    LOG_ERROR("this is a error log");
}
