#pragma once

#include "mv_utils/utilities.hpp"
#include "mv_utils/array_types.hpp"
#include "mv_utils/mat4.hpp"
#include "mv_utils/stringbuilder.hpp"
#include "platform/platform.hpp"

#if defined(DEBUG)
    #define mv_dbg_assert(expression, errorString)\
    if(!(expression)){pltf::DebugString(errorString); pltf::DebugBreak();}
#else
    #define mv_dbg_assert(expression, errorString)
#endif

enum class log_level {info, warning, error, trace};
using log_message_callback = void(*)(log_level, const char*);

constexpr size_t KiloBytes(const size_t amount) {return amount * 1024ULL;}
constexpr size_t MegaBytes(const size_t amount) {return amount * 1024ULL * 1024ULL;}
constexpr size_t GigaBytes(const size_t amount) {return amount * 1024ULL * 1024ULL * 1024ULL;}
constexpr uint32_t BIT(uint32_t value){return 1 << value;}

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

enum class CoreResult
{
    Success,
    Format_Not_Supported,
    Source_Missing
};

constexpr auto SHADERS_PATH = view("../../shaders/");
constexpr auto ASSETS_PATH = view("../../assets/");