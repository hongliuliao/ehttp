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
#include <errno.h>
#include <unistd.h>

#include "simple_config.h"
#include "simple_log.h"

// log context
const int MAX_SINGLE_LOG_SIZE = 2048;
const int ONE_DAY_SECONDS = 86400;

int log_level = DEBUG_LEVEL;
std::string g_dir;
std::string g_config_file;
bool use_file_appender = false;
FileAppender g_file_appender;

FileAppender::FileAppender() {
    _is_inited = false;
    _retain_day = -1;
}

FileAppender::~FileAppender() {
    if (_fs.is_open()) {
        _fs.close();
    }
}

int FileAppender::init(std::string dir, std::string log_file) {
    if (!dir.empty()) {
        int ret = mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (ret != 0 && errno != EEXIST) {
            printf("mkdir error which dir:%s err:%s\n", dir.c_str(), strerror(errno));
            _is_inited = true;
            return -1;
        }
    } else {
        dir = "."; // current dir
    }
    _log_dir = dir;
    _log_file = log_file;
    _log_file_path = dir + "/" + log_file;
    _fs.open(_log_file_path.c_str(), std::fstream::out | std::fstream::app);
    _is_inited = true;
    pthread_mutex_init(&writelock, NULL);
    return 0;
}

int FileAppender::write_log(char *log, const char *format, va_list ap) {
    pthread_mutex_lock(&writelock);
    if (_fs.is_open()) {
        vsnprintf(log, MAX_SINGLE_LOG_SIZE - 1, format, ap);
        _fs << log << "\n";
        _fs.flush();
    }
    pthread_mutex_unlock(&writelock);
    return 0;
}

int FileAppender::shift_file_if_need(struct timeval tv, struct timezone tz) {
    if (_last_sec == 0) {
        _last_sec = tv.tv_sec;
        return 0;
    }
    long fix_now_sec = tv.tv_sec - tz.tz_minuteswest * 60;
    long fix_last_sec = _last_sec - tz.tz_minuteswest * 60;
    if (fix_now_sec / ONE_DAY_SECONDS - fix_last_sec / ONE_DAY_SECONDS) {
        pthread_mutex_lock(&writelock);

        struct tm *tm;
        time_t y_sec = tv.tv_sec - ONE_DAY_SECONDS;
        tm = localtime(&y_sec); //yesterday
        char new_file[100];
        bzero(new_file, 100);
        sprintf(new_file, "%s.%04d-%02d-%02d",
                _log_file.c_str(),
                tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
        std::string new_file_path = _log_dir + "/" + new_file;
        if (access(new_file_path.c_str(), F_OK) != 0) {
            rename(_log_file_path.c_str(), new_file_path.c_str());
            // reopen new log file
            _fs.close();
            _fs.open(_log_file_path.c_str(), std::fstream::out | std::fstream::app);
        }

        pthread_mutex_unlock(&writelock);

        delete_old_log(tv);
    }

    _last_sec = tv.tv_sec;
    return 0;
}

bool FileAppender::is_inited() {
    return _is_inited;
}

void FileAppender::set_retain_day(int rd) {
    _retain_day = rd;
}

int FileAppender::delete_old_log(timeval tv) {
    if (_retain_day <= 0) {
        return 0;
    }
    struct timeval old_tv;
    old_tv.tv_sec = tv.tv_sec - _retain_day * 3600 * 24;
    old_tv.tv_usec = tv.tv_usec;
    char old_file[100];
    memset(old_file, 0, 100);
    struct tm *tm;
    tm = localtime(&old_tv.tv_sec);
    sprintf(old_file, "%s.%04d-%02d-%02d",
            _log_file.c_str(), tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday - 1);
    std::string old_file_path = _log_dir + "/" + old_file;
    return remove(old_file_path.c_str());
}

int _check_config_file() {
	std::map<std::string, std::string> configs;
    std::string log_config_file = g_dir + "/" + g_config_file;
	get_config_map(log_config_file.c_str(), configs);
	if (configs.empty()) {
        return 0;
    }
    // read log level
    std::string log_level_str = configs["log_level"];
    set_log_level(log_level_str.c_str());

    std::string rd = configs["retain_day"];
    if (!rd.empty()) {
        g_file_appender.set_retain_day(atoi(rd.c_str()));
    }
    // read log file
    std::string dir = configs["log_dir"];
    std::string log_file = configs["log_file"];
    int ret = 0;
    if (!log_file.empty()) {
        use_file_appender = true;
        if (!g_file_appender.is_inited()) {
            ret = g_file_appender.init(dir, log_file);
        }
    }
    return ret;
}

void sigreload(int sig) {
    //printf("receive sig:%d \n", sig);
    _check_config_file();
}

std::string _get_show_time(timeval tv) {
	char show_time[40];
	memset(show_time, 0, 40);

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
	if (!use_file_appender) { // if no config, send log to stdout
		vprintf(format, ap);
		printf("\n");
		return;
	}
    struct timeval now;
    struct timezone tz;
    gettimeofday(&now, &tz);
    std::string fin_format = _get_show_time(now) + " " + format;

    g_file_appender.shift_file_if_need(now, tz);
    char single_log[MAX_SINGLE_LOG_SIZE];
    bzero(single_log, MAX_SINGLE_LOG_SIZE);
    g_file_appender.write_log(single_log, fin_format.c_str(), ap);
}

int log_init(std::string dir, std::string file) {
    g_dir = dir;
    g_config_file = file;
    signal(SIGUSR1, sigreload);
    return _check_config_file();
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
