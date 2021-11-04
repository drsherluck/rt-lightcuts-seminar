#include "file.h"
#include "log.h"

#include <stdio.h>

i32 read_file(const char* path, file_contents_t* out)
{
    if (!out) return 0;

    FILE* fp;
    fp = fopen(path, "r");
    if (!fp)
    {
        LOG_ERROR("Could not open file");
        return -1;
    }

    // get file size
    fseek(fp, 0, SEEK_END);
    out->size = ftell(fp);
    rewind(fp);

    out->contents = (u8*)malloc(sizeof(u8) * out->size);
    if (!out->contents) 
    {
        LOG_ERROR("Could not allocate file buffer");
        return -2;
    }

    i32 bytes = fread(out->contents, 1, out->size, fp);
    if (bytes != out->size)
    {
        LOG_ERROR("Could not read the file");
        return -3;
    }

    fclose(fp);
    return bytes;
}
