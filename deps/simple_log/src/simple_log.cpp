#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "stdarg.h"
#include <signal.h> 

#include "simple_config.h"
#include "simple_log.h"

// log context
const char log_config_file[100] = "conf/simple_log.conf";
const int max_single_log_size = 2048;
char single_log[max_single_log_size];

int log_level = DEBUG_LEVEL;
std::string log_file;

void _check_config_file() {
	std::map<std::string, std::string> configs;
	get_config_map(log_config_file, configs);
	if (!configs.empty()) {
		// read log level
		std::string log_level_str = configs["log_level"];
		set_log_level(log_level_str.c_str());

		// read log file
		log_file = configs["log_file"];
	}
}

void sigreload(int sig) {
    //printf("receive sig:%d \n", sig);
    _check_config_file();
}

std::string _get_show_time() {
	char show_time[40];
	memset(show_time, 0, 40);

	struct timeval tv;
	gettimeofday(&tv, NULL);

	struct tm *tm;
	tm = localtime(&tv.tv_sec);

	sprintf(show_time, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
			tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec, (int)(tv.tv_usec/1000));
	return std::string(show_time);
}

int _get_log_level(const char *level_str) {
	if(strcasecmp(level_str, "ERROR") == 0) {
		return ERROR_LEVEL;
	}
	if(strcasecmp(level_str, "WARN") == 0) {
		return WARN_LEVEL;
	}
	if(strcasecmp(level_str, "INFO") == 0) {
		return INFO_LEVEL;
	}
	if(strcasecmp(level_str, "DEBUG") == 0) {
		return DEBUG_LEVEL;
	}
	return DEBUG_LEVEL;
}

void set_log_level(const char *level) {
    log_level = _get_log_level(level);
}

void _log(const char *format, va_list ap) {
	if(log_file.empty()) { // if no config, send log to stdout
		vprintf(format, ap);
		printf("\n");
		return;
	}

	std::fstream fs(log_file.c_str(), std::fstream::out | std::fstream::app);
	if(fs.is_open()) {
		vsprintf(single_log, format, ap);
		fs << single_log << "\n";
		fs.close();
	}
}

int log_init() {
    signal(SIGUSR1, sigreload);
    _check_config_file();
    return 0;
}

void log_error(const char *format, ...) {
	if (log_level < ERROR_LEVEL) {
		return;
	}

	va_list ap;
	va_start(ap, format);

	_log(format, ap);

	va_end(ap);
}

void log_warn(const char *format, ...) {
	if (log_level < WARN_LEVEL) {
		return;
	}

	va_list ap;
	va_start(ap, format);

	_log(format, ap);

	va_end(ap);
}

void log_info(const char *format, ...) {
	if (log_level < INFO_LEVEL) {
		return;
	}

	va_list ap;
	va_start(ap, format);

	_log(format, ap);

	va_end(ap);
}

void log_debug(const char *format, ...) {
	if (log_level < DEBUG_LEVEL) {
		return;
	}

	va_list ap;
	va_start(ap, format);

	_log(format, ap);

	va_end(ap);
}
