#pragma once

#include "mv_utils/view.hpp"
#include "mv_utils/vec.hpp"
#include "mv_tools.hpp"

#if defined(MV_DEBUG)
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

constexpr size_t KiloBytes(const size_t amount) {return amount * 1024ULL;}
constexpr size_t MegaBytes(const size_t amount) {return amount * 1024ULL * 1024ULL;}
constexpr size_t GigaBytes(const size_t amount) {return amount * 1024ULL * 1024ULL * 1024ULL;}
constexpr uint32_t BIT(uint32_t value){return 1 << value;}

template<typename T>
inline T clamp(T value, T min, T max)
{
    const T result = value < min ? min : value;
    return result > max ? max : result;
}

enum CORE_FLAG_BITS
{
    CORE_FLAG_RUNNING = BIT(0),
    CORE_FLAG_WINDOW_RESIZED = BIT(1),
    CORE_FLAG_WINDOW_FULLSCREEN = BIT(2),
    CORE_FLAG_ENABLE_VALIDATION = BIT(3),
    CORE_FLAG_ENABLE_VSYNC = BIT(4),
};

constexpr float PI32 = 3.141592741f;