#pragma once

#include <stdint.h>
#include "dyn_array.hpp"

template<typename T, int N>
constexpr size_t arraysize(T (&array)[N])
{
    return N;
}

template<typename T>
struct view
{
    view() = default;
    constexpr view(T *src, size_t srcCount)
    : data(src), count(srcCount)
    {
    }

    template<int N>
    constexpr view(T (&array)[N])
    : data(array), count(N)
    {
    }

    view(dyn_array<T> &array)
    : data(array.data()), count(array.count())
    {
    }

    T &operator[](size_t i) const
    {
        return this->data[i];
    }

    size_t size() const
    {
        return sizeof(T) * count;
    }

    T *data;
    size_t count;
};