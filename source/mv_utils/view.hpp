#pragma once

#include <stdint.h>

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
    : data(src), count(srcCount) {}

    template<int N>
    constexpr view(T (&array)[N])
    : data(array), count(N) {}

    T &operator[](size_t i) const
    {
        return this->data[i];
    }

    void fill(const T &value)
    {
        for(size_t i = 0; i < this->count; i++)
            this->data[i] = value;
    }

    T *data;
    size_t count;
};