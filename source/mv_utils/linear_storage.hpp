#pragma once

#include "virtual_allocator.hpp"
#include "view.hpp"

struct linear_storage : virtual_allocator
{
    linear_storage(size_t capacity)
    {
        m_begin = allocate<void>(capacity);
        m_cursor = m_begin;
        m_end = static_cast<uint8_t*>(m_begin) + capacity;
    }

    ~linear_storage()
    {
        deallocate(m_begin);
    }

    linear_storage(const linear_storage &src) = delete;
    linear_storage(const linear_storage &&src) = delete;
    linear_storage &operator=(const linear_storage &src) = delete;
    linear_storage &operator=(const linear_storage &&src) = delete;

    template<typename T>
    T *push(size_t count)
    {
        alignas(T) auto block = static_cast<T*>(m_cursor);
        m_cursor = static_cast<T*>(m_cursor) + count;

        if(m_cursor >= m_end)
            block = nullptr;

        return block;
    }

    template<typename T>
    view<T> pushView(size_t count)
    {
        auto ptr = push<T>(count);
        return {ptr, count};
    }

    void flush()
    {
        m_cursor = m_begin;
    }

private:
    void *m_begin, *m_cursor, *m_end;
};