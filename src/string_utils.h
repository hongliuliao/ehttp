#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <string>
#include <vector>

int ss_split_str(const std::string &input, const char s, 
        std::vector<std::string> &tokens);

int ss_str_tolower(std::string &input);

#endif
