#ifndef LOG_H
#define LOG_H

#include "common.h"
#include <stdlib.h>

#define __FILENAME__ (__FILE__ + SOURCE_PATH_SIZE)
#define LOG_INFO(...)  log_info(__FILENAME__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_error(__FILENAME__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) log_fatal(__FILENAME__, __LINE__, __VA_ARGS__)

#define TERM_RED   "\x1b[31m"
#define TERM_RESET "\x1b[0m"

void log(const char* file, i32 line, const char* type, ...);
void log(const char* file, i32 line, const char* type, const char* msg);

template<typename... Args>
void log_info(const char* file, i32 line, const Args& ...args)
{
	static const char* type = "[Info]";
	log(file, line, type, args...);
}

template<typename... Args>
void log_error(const char* file, i32 line, const Args& ...args)
{
	static const char* type = "\x1b[31;1m[Error]\x1b[0m";
	log(file, line, type, args...);
}

template<typename... Args>
void log_fatal(const char* file, i32 line, const Args& ...args)
{
    static const char* type = "\x1b[31;1m[Fatal]\x1b[0m";
    log(file, line, type, args...);
    abort();
}

#endif  // LOG_H
