#pragma once

#include "mv_utils/array_types.hpp"
#include "mv_utils/mat4.hpp"
#include "platform/platform.hpp"

#if defined(DEBUG)
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

// NOTE(arle): temporary, to be replaced
#define INTERNAL_DIR "../../"
#define MATERIALS_DIR "assets/materials/"

constexpr size_t KiloBytes(const size_t amount) {return amount * 1024ULL;}
constexpr size_t MegaBytes(const size_t amount) {return amount * 1024ULL * 1024ULL;}
constexpr size_t GigaBytes(const size_t amount) {return amount * 1024ULL * 1024ULL * 1024ULL;}
constexpr uint32_t BIT(uint32_t value){return 1 << value;}

template<typename T>
inline constexpr T clamp(T value, T min, T max)
{
    const T result = value < min ? min : value;
    return result > max ? max : result;
}

constexpr float PI32 = 3.141592741f;
constexpr float GetRadians(float angle) {return angle / 180.0f * PI32;}

constexpr vec4<float> GetColour(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    return vec4(float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, float(a) / 255.0f);
}

constexpr vec4<float> GetColour(uint32_t hex)
{
    return vec4(float((hex & 0xFF000000) >> 24) / 255.0f,
                float((hex & 0x00FF0000) >> 16) / 255.0f,
                float((hex & 0x0000FF00) >> 8) / 255.0f,
                float((hex & 0x000000FF) >> 0) / 255.0f);
}