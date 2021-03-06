#pragma once

#include "../mv_utils/array_types.hpp"
#include "../platform/platform.hpp"

class linear_storage
{
public:
    linear_storage(size_t size)
    {
        m_begin = pltf::MapMemory(size);
        m_cursor = m_begin;
        m_end = static_cast<uint8_t*>(m_begin) + size;
    }

    ~linear_storage()
    {
        pltf::UnmapMemory(m_begin);
    }

    template<typename T>
    T *allocate(size_t count)
    {
        alignas(T) auto block = static_cast<T*>(m_cursor);
        m_cursor = static_cast<T*>(m_cursor) + count;
        return (m_cursor >= m_end) ? nullptr : block;
    }

    template<typename T>
    constexpr view<T> allocateView(size_t count)
    {
        return view<T>(allocate<T>(count), count);
    }

private:
    void *m_begin, *m_cursor, *m_end;
};