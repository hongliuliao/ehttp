#include <algorithm>
#include <sstream>
#include "string_utils.h"

int ss_split_str(const std::string &input, const char s, std::vector<std::string> &tokens) {
    if (input.empty()) {
        return 0;
    }
    std::stringstream ss;
    ss << input;
    std::string token;
    while (std::getline(ss, token, s)) {
        tokens.push_back(token);
    }
    return 0;
}

int ss_str_tolower(std::string &input) {
    std::transform(input.begin(), input.end(), 
            input.begin(), ::tolower);
    return 0;
}
