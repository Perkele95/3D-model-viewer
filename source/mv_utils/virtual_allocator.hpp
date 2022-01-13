#pragma once

#if defined(_WIN32)
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #undef WIN32_LEAN_AND_MEAN
#elif defined(__linux__)
    //
#endif

class virtual_allocator
{
public:
    virtual_allocator() = default;

    virtual_allocator(const virtual_allocator &src) = delete;
    virtual_allocator(const virtual_allocator &&src) = delete;
    virtual_allocator &operator=(const virtual_allocator &src) = delete;
    virtual_allocator &operator=(const virtual_allocator &&src) = delete;

protected:
    template<typename T>
    T *allocate(size_t count) const
    {
#if defined(_WIN32)
        auto memory = VirtualAlloc(nullptr, count * sizeof(T), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        return static_cast<T*>(memory);
#elif defined(__linux__)
        return nullptr;
#else
        return nullptr;
#endif
    }

    template<>
    void *allocate(size_t count) const
    {
#if defined(_WIN32)
        auto memory = VirtualAlloc(nullptr, count, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        return static_cast<void*>(memory);
#elif defined(__linux__)
        return nullptr;
#else
        return nullptr;
#endif
    }

    bool deallocate(void *address)
    {
#if defined(_WIN32)
        auto result = VirtualFree(address, 0, MEM_RELEASE);
        return static_cast<bool>(result);
#elif defined(__linux__)
        return false;
#else
        return false;
#endif
    }
};