#pragma once

#include <stdint.h>

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

template<typename T, int N>
constexpr T accumulate(const T (&array)[N], T value = T(0))
{
    for (size_t i = 0; i < N; i++)
        value += array[i];

    return value;
}