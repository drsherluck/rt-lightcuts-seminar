#ifndef FILE_H
#define FILE_H

#include "common.h"

#include <stddef.h>

struct file_contents_t
{
    u8*    contents;
    size_t size;
};

i32 read_file(const char* path, file_contents_t* out);

#endif // FILE_H
