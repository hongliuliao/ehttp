#include "gtest/gtest.h"

#include "string_utils.h"

TEST(StringUtilsTest, test_ss_split_str) {
    std::string input = "aa,bb";
    std::vector<std::string> tokens;
    int ret = ss_split_str(input, ',', tokens);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(2, (int)tokens.size());
    ASSERT_EQ("aa", tokens[0]);
    ASSERT_EQ("bb", tokens[1]);
}
