#pragma once

#include <stdint.h>

template<typename T, int N>
constexpr size_t arraysize(T (&array)[N]) { return N; }

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

    T &operator[](size_t i) const { return this->data[i]; }

    size_t size() const { return sizeof(T) * count; }

    T *data;
    size_t count;
};

template<typename T>
class data_buffer
{
public:
    data_buffer(size_t capacity)
    {
        m_data = new T[capacity];
        m_capacity = capacity;
    }

    data_buffer(const data_buffer &ref) = delete;
    data_buffer(const data_buffer &&rval) = delete;

    ~data_buffer()
    {
        delete[] m_data;
        m_data = nullptr;
    }

    data_buffer &operator=(const data_buffer &ref) = delete;
    data_buffer &operator=(const data_buffer &&rval) = delete;

    constexpr T &operator[](size_t index) { return m_data[index]; }

    T *data() { return m_data; }
    size_t capacity() { return m_capacity; }

    void fill(const T &data)
    {
        for (size_t i = 0; i < m_capacity; i++)
            m_data[i] = data;
    }

    view<T> getView() { return view<T>(m_data, m_capacity); }

private:
    T*      m_data;
    size_t  m_capacity;
};

template<typename T>
class fixed_capacity_array
{
public:
    fixed_capacity_array(size_t capacity)
    {
        m_data = new T[capacity];
        m_capacity = capacity;
        m_count = 0;
    }

    fixed_capacity_array(const fixed_capacity_array &ref) = delete;
    fixed_capacity_array(const fixed_capacity_array &&rval) = delete;

    ~fixed_capacity_array()
    {
        delete[] m_data;
        m_data = nullptr;
    }

    fixed_capacity_array &operator=(const fixed_capacity_array &ref) = delete;
    fixed_capacity_array &operator=(const fixed_capacity_array &&rval) = delete;

    constexpr T &operator[](size_t index) { return m_data[index]; }

    T *data() { return m_data; }
    size_t count() { return m_count; }
    size_t capacity() { return m_capacity; }

    void add(const T &data)
    {
        m_data[m_count++] = data;
    }

    void add_safe(const T &data)
    {
        if(m_count < m_capacity)
            m_data[m_count++] = data;
    }

    void fill(const T &data)
    {
        for (size_t i = 0; i < m_capacity; i++)
            m_data[i] = data;
        m_count = m_capacity;
    }

    view<T> view() { return view<T>(m_data, m_count); }

private:
    T*      m_data;
    size_t  m_count;
    size_t  m_capacity;
};