#pragma once

#include <stdint.h>

enum class result_state
{
    ok,
    error
};

template<typename T, typename E>
struct result
{
    result(const T &data)
    {
        m_state = result_state::ok;
        m_data.ok = data;
    }

    result(const E &error)
    {
        m_state = result_state::error;
        m_data.error = error;
    }

    result(const result<T, E> &src) = delete;
    result(const result<T, E> &&src) = delete;

    result<T, E> &operator=(const result<T, E> &src) = delete;
    result<T, E> &operator=(const result<T, E> &&src) = delete;

    operator result_state() const {return m_state;}

    explicit operator T() const {return m_data.ok;}
    explicit operator E() const {return m_data.error;}

private:
    union result_container
    {
        T ok;
        E error;
    } m_data;

    result_state m_state;
};