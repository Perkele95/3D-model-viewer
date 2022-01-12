#pragma once

#include <stdint.h>

template<typename T>
struct dyn_array
{
    dyn_array(size_t count)
    : m_count(count)
    {
        m_data = new T[count];
    }

    ~dyn_array()
    {
        delete[] m_data;
    }

    dyn_array(const dyn_array<T> &src);
    dyn_array(const dyn_array<T> &&src);
    dyn_array<T> &operator=(const dyn_array<T> &src);
    dyn_array<T> &operator=(const dyn_array<T> &&src);

    constexpr dyn_array<T> &operator[](size_t index)
    {
        return m_data[index];
    }

    T *data() const
    {
        return m_data;
    }

    size_t count() const
    {
        return m_count;
    }

    void fill(const T &value)
    {
        for(size_t i = 0; i < m_count; i++)
            m_data[i] = value;
    }

private:
    T *m_data;
    size_t m_count;
};