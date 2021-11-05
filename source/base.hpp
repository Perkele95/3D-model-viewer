#pragma once

#include "mv_utils/view.hpp"
#include "mv_utils/vec.hpp"

#if defined(DEBUG_BUILD)
    #if defined(_WIN32)
        #define WIN32_LEAN_AND_MEAN
        #define NOMINMAX
        #include <windows.h>
        #define vol_assert(expression) if(!(expression)){DebugBreak();}
    #elif defined(__linux__)
		#include <signal.h>
		#define vol_assert(expression) if(!(expression)){raise(SIGTRAP);}
    #else
        #define vol_assert(expression)
    #endif
#else
    #define vol_assert(expression)
#endif

struct file_t
{
    void *handle;
    size_t size;
};

namespace io
{
    bool close(file_t &file);
    file_t read(const char *filename);
    bool write(const char *filename, file_t &file);
    bool fileExists(const char *filename);
    file_t mapFile(size_t size);
}

constexpr size_t KiloBytes(const size_t amount) {return amount * 1024ULL;}
constexpr size_t MegaBytes(const size_t amount) {return amount * 1024ULL * 1024ULL;}
constexpr size_t GigaBytes(const size_t amount) {return amount * 1024ULL * 1024ULL * 1024ULL;}
constexpr uint32_t BIT(uint32_t value){return 1 << value;}

enum CORE_FLAG_BITS
{
    CORE_FLAG_RUNNING = BIT(0),
    CORE_FLAG_WINDOW_RESIZED = BIT(1),
    CORE_FLAG_WINDOW_FULLSCREEN = BIT(2),
};