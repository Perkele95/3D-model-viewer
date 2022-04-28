#pragma once

#include "array_types.hpp"
#include <cstring>

class StringbBuilder
{
public:
    static constexpr size_t DEFAULT_MAX = 256;

    static constexpr size_t Strlen(const char *cstring)
    {
        size_t count = 1;
        while (*cstring++)
            count++;
        return count;
    }

    static constexpr view<const char> MakeView(const char *cstring)
    {
        return view(cstring, Strlen(cstring));
    }

    static constexpr view<char> MakeView(char *cstring)
    {
        return view(cstring, Strlen(cstring));
    }

    StringbBuilder(size_t capacity = DEFAULT_MAX)
    {
        m_str = new char[capacity];
        memset(m_str, 0, capacity);
        m_capacity = capacity;
        m_length = 0;
    }

    void destroy()
    {
        delete m_str;
    }

    constexpr StringbBuilder &operator<<(view<const char> stringView)
    {
        const auto length = stringView.count - 1;
        const bool hasSpace = (m_length + length) < m_capacity;

        if(hasSpace)
        {
            const auto dst = m_str + m_length;
            memcpy(dst, stringView.data, length);
            m_length += length;
        }

        return *this;
    }

    constexpr StringbBuilder &operator<<(view<char> stringView)
    {
        const auto length = stringView.count - 1;
        const bool hasSpace = (m_length + length) < m_capacity;

        if(hasSpace)
        {
            const auto dst = m_str + m_length;
            memcpy(dst, stringView.data, length);
            m_length += length;
        }

        return *this;
    }

    constexpr StringbBuilder &operator<<(const StringbBuilder &ref)
    {
        return (*this << ref.getView());
    }

    constexpr StringbBuilder &operator<<(const char *cstring)
    {
        const auto length = Strlen(cstring);
        return (*this << view(cstring, length));
    }

    constexpr StringbBuilder &operator<<(char *cstring)
    {
        const auto length = Strlen(cstring);
        return (*this << view(cstring, length));
    }

    constexpr StringbBuilder &flush()
    {
        memset(m_str, 0, m_length);
        m_length = 0;
        return *this;
    }

    constexpr const char *c_str() const
    {
        return m_str;
    }

    constexpr view<const char> getView() const
    {
        return view<const char>(m_str, m_length);
    }

private:
    char*   m_str;
    size_t  m_capacity;
    size_t  m_length;
};