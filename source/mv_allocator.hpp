#pragma once

#include "base.hpp"

struct mv_allocator
{
    mv_allocator(void *buffer, size_t permanentCapacity, size_t transientCapacity)
    {
        this->permanentBegin = buffer;
        this->permanentCursor = this->permanentBegin;
        this->permanentEnd = static_cast<uint8_t*>(this->permanentBegin) + permanentCapacity;

        this->transientBegin = static_cast<uint8_t*>(this->permanentEnd) + 1;
        this->transientCursor = this->transientBegin;
        this->transientEnd = static_cast<uint8_t*>(this->transientBegin) + transientCapacity;
    }

    mv_allocator(const mv_allocator &src) = delete;
    mv_allocator(const mv_allocator &&src) = delete;
    mv_allocator &operator=(const mv_allocator &src) = delete;
    mv_allocator &operator=(const mv_allocator &&src) = delete;

    template<typename T>
    T *allocPermanent(size_t count)
    {
        auto alloc = static_cast<T*>(this->permanentCursor);
        this->permanentCursor = static_cast<T*>(this->permanentCursor) + count;

        if(this->permanentCursor > this->permanentEnd)
            alloc = nullptr;

        return alloc;
    }

    template<typename T>
    T *allocTransient(size_t count)
    {
        auto alloc = static_cast<T*>(this->transientCursor);
        this->transientCursor = static_cast<T*>(this->transientCursor) + count;

        if(this->transientCursor > this->transientEnd){
            this->transientCursor = this->transientBegin;
            alloc = static_cast<T*>(this->transientCursor);
        }

        return alloc;
    }

    template<typename T>
    view<T> allocViewPermanent(size_t count)
    {
        auto result = view(this->allocPermanent<T>(count), count);
        return result;
    }

    template<typename T>
    view<T> allocViewTransient(size_t count)
    {
        auto result = view(this->allocTransient<T>(count), count);
        return result;
    }
private:
    void *permanentBegin, *transientBegin;
    void *permanentEnd, *transientEnd;
    void *permanentCursor, *transientCursor;
};