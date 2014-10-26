#ifndef SIMPLE_LOG_H
#define SIMPLE_LOG_H

#include <cstring>
#include <string>

const int ERROR_LEVEL = 1;
const int WARN_LEVEL = 2;
const int INFO_LEVEL = 3;
const int DEBUG_LEVEL = 4;

#define LOG_ERROR(format, args...) \
		log_error("%s %s %s(%d): " format, _get_show_time().c_str(), "ERROR", __FILE__, __LINE__, ##args);

#define LOG_WARN(format, args...) \
		log_warn("%s %s %s(%d): " format, _get_show_time().c_str(), "WARN", __FILE__, __LINE__, ##args);

#define LOG_INFO(format, args...) \
		log_info("%s %s %s(%d): " format, _get_show_time().c_str(), "INFO", __FILE__, __LINE__, ##args);

#define LOG_DEBUG(format, args...) \
		log_debug("%s %s %s(%d): " format, _get_show_time().c_str(), "DEBUG", __FILE__, __LINE__, ##args);


std::string _get_show_time();

void init_log_config(char *config_file);
void log_error(const char *format, ...);
void log_warn(const char *format, ...);
void log_info(const char *format, ...);
void log_debug(const char *format, ...);

#endif
