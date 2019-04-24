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

TEST(FileAppendeTest, test_shift_file_if_need) {
    FileAppender appender;
    int ret = appender.init("./log", "ehttp.log");
    ASSERT_EQ(0, ret);

    struct timeval now;
    struct timezone tz;
    gettimeofday(&now, &tz);
    ret = appender.shift_file_if_need(now, tz);
    ASSERT_EQ(0, ret);

    now.tv_sec = now.tv_sec + 3600 * 24 + 1; // tomorrow
    ret = appender.shift_file_if_need(now, tz);
    ASSERT_EQ(1, ret);
}

TEST(FileAppendeTest, test_delete_old_log) {
    FileAppender appender;
    int ret = appender.init("./log", "ehttp.log");
    ASSERT_EQ(0, ret);

    appender.set_retain_day(3);
    struct timeval now;
    struct timezone tz;
    gettimeofday(&now, &tz);
    ret = appender.delete_old_log(now);
    ASSERT_EQ(-1, ret);
}
