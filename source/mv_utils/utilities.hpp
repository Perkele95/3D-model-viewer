#pragma once

#include <stdint.h>
#include "array_types.hpp"

constexpr float PI32 = 3.141592741f;
constexpr double PI64 = 3.141592653589793;

constexpr float GetRadians(float angle)
{
    constexpr float factor = PI32 / 180.0f;
    return angle * factor;
}

constexpr double GetRadians(double angle)
{
    constexpr double factor = PI64 / 180.0;
    return angle * factor;
}

constexpr float GetAngle(float rad)
{
    constexpr float factor = 180.0f / PI32;
    return rad * factor;
}

constexpr double GetAngle(double rad)
{
    constexpr double factor = 180.0 / PI64;
    return rad * factor;
}

template<typename T>
constexpr T clamp(T value, T min, T max)
{
    const T result = value < min ? min : value;
    return result > max ? max : result;
}

template<typename T>
constexpr T min(T a, T b)
{
    return a < b ? a : b;
}

template<typename T>
constexpr T max(T a, T b)
{
    return a > b ? a : b;
}

template<typename T>
constexpr T accumulate(view<T> source, T initalValue = T(0))
{
    for (size_t i = 0; i < source.count; i++)
        initalValue += source[i];

    return initalValue;
}