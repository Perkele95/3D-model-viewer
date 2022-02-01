#pragma once

#include <stdint.h>

template<typename T, typename E>
struct result
{
    result(const T &data) : m_state(true)
    {
        m_data.ok = data;
    }

    result(const E &error) : m_state(false)
    {
        m_data.error = error;
    }

    static result<T, E> Ok(const T &data)
    {
        return data;
    }

    static result<T, E> Error(const E &error)
    {
        return error;
    }

    result(const result<T, E> &src)
    : m_state(src.m_state)
    {
        if(src.m_state)
            m_data.ok = src.m_data.ok;
        else
            m_data.error = src.m_data.error;
    }

    result(const result<T, E> &&src)
    : m_state(src.m_state)
    {
        if(src.m_state)
            m_data.ok = src.m_data.ok;
        else
            m_data.error = src.m_data.error;
    }

    result<T, E> &operator=(const result<T, E> &src) = delete;
    result<T, E> &operator=(const result<T, E> &&src) = delete;

    operator bool() const {return m_state;}

    T &as_ok() const {return m_data.ok;}
    E &as_err() const {return m_data.error;}

private:
    union result_container
    {
        T ok;
        E error;
    } m_data;

    bool m_state;
};