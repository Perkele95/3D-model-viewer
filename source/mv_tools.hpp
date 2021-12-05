#pragma once

#include <stdint.h>

namespace mv_tools
{
    using mv_handle = void*;

    mv_handle map(size_t size);
    bool unMap(mv_handle mapped);
}

struct file_t
{
    mv_tools::mv_handle handle;
    size_t size;
};

namespace io
{
    bool close(file_t *file);
    file_t read(const char *filename);
    bool write(const char *filename, file_t *file);
    bool fileExists(const char *filename);
}