#pragma once

#include "base.hpp"

struct mv_allocator : static_allocator
{
    mv_allocator(size_t permanentCapacity, size_t transientCapacity)
    {
        m_permanentBegin = allocate<void>(permanentCapacity + transientCapacity);
        m_permanentCursor = m_permanentBegin;
        m_permanentEnd = static_cast<uint8_t*>(m_permanentBegin) + permanentCapacity;

        m_transientBegin = static_cast<uint8_t*>(m_permanentEnd) + 1;
        m_transientCursor = m_transientBegin;
        m_transientEnd = static_cast<uint8_t*>(m_transientBegin) + transientCapacity;
    }

    ~mv_allocator()
    {
        deallocate(m_permanentBegin);
    }

    mv_allocator(const mv_allocator &src) = delete;
    mv_allocator(const mv_allocator &&src) = delete;
    mv_allocator &operator=(const mv_allocator &src) = delete;
    mv_allocator &operator=(const mv_allocator &&src) = delete;

    template<typename T>
    T *allocPermanent(size_t count)
    {
        alignas(T) auto alloc = static_cast<T*>(m_permanentCursor);
        m_permanentCursor = static_cast<T*>(m_permanentCursor) + count;

        if(m_permanentCursor > m_permanentEnd)
            alloc = nullptr;

        return alloc;
    }

    template<typename T>
    T *allocTransient(size_t count)
    {
        alignas(T) auto alloc = static_cast<T*>(m_transientCursor);
        m_transientCursor = static_cast<T*>(m_transientCursor) + count;

        if(m_transientCursor > m_transientEnd){
            m_transientCursor = m_transientBegin;
            alloc = static_cast<T*>(m_transientCursor);
        }

        return alloc;
    }

    template<typename T>
    view<T> allocViewPermanent(size_t count)
    {
        auto result = view(allocPermanent<T>(count), count);
        return result;
    }

    template<typename T>
    view<T> allocViewTransient(size_t count)
    {
        auto result = view(allocTransient<T>(count), count);
        return result;
    }

    void flushTransientBuffer()
    {
        m_transientCursor = m_transientBegin;
    }

private:
    void *m_permanentBegin, *m_transientBegin;
    void *m_permanentEnd, *m_transientEnd;
    void *m_permanentCursor, *m_transientCursor;
};