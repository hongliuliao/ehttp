#include "gtest/gtest.h"

#include "simple_config.h"

TEST(SimpleConfigTest, test_get_config_map) {
    std::string conf = "./conf/simple_log.conf";
    std::map<std::string, std::string> configs;
    int ret = get_config_map(conf.c_str(), configs);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(3, (int)configs.size());
}
