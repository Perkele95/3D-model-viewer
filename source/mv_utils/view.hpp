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
    // TODO(arle): remove this method and use dyn_array::fill instead
    void fill(const T &value)
    {
        for(size_t i = 0; i < this->count; i++)
            this->data[i] = value;
    }

    T *data;
    size_t count;
};