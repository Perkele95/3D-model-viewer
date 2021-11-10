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
    view(T *data, size_t count)
    {
        this->data = data;
        this->count = count;
    }

    template<int N>
    view(T (&array)[N])
    {
        this->data = array;
        this->count = N;
    }

    T &operator[](size_t i) const
    {
        return this->data[i];
    }

    void fill(const T &data)
    {
        for(size_t i = 0; i < this->count; i++)
            this->data[i] = data;
    }

    T *data;
    size_t count;
};