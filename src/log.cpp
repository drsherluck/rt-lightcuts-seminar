#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

static void get_timestamp(char* str, size_t size)
{
	time_t secs;
	time(&secs);
	struct tm* curr = localtime(&secs);
	snprintf(str, 9, "%.2d:%.2d:%.2d", curr->tm_hour, curr->tm_min, curr->tm_sec);
}

void log(const char* file, int line, const char* type, const char* msg)
{
	char timestamp[9];
	get_timestamp(timestamp, 9);
	printf("%s %s (%s:%d) %s\n", timestamp, type, file, line, msg);
}

void log(const char* file, int line, const char* type, ...)
{
	char timestamp[9];
	get_timestamp(timestamp, 9);
	va_list args, c_args;
	va_start(args, type);
	const char* fmt = va_arg(args, char*);

	// get size
	va_copy(c_args, args);	
	size_t size = vsnprintf(nullptr, 0, fmt, c_args);
	va_end(c_args);

	char* buf = (char*)malloc(size + 1);
	vsnprintf(buf, size + 1, fmt, args);
	va_end(args);

	printf("%s %s (%s:%d) %s\n", timestamp, type, file, line, buf);
	free(buf);
}

