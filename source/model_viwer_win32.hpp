#pragma once

#include "base.hpp"
#include "input.hpp"

struct core_memory
{
    core_memory();
    ~core_memory();

    core_memory(const core_memory &src) = delete;
    core_memory(const core_memory &&src) = delete;
    core_memory &operator=(const core_memory &src) = delete;
    core_memory &operator=(const core_memory &&src) = delete;

    void *allocPermanent();
    void *allocTransient();

    template<typename T>
    view<T> allocViewPermanent();

    template<typename T>
    view<T> allocViewTransient();

    uint8_t *permenentStorage, transientStorage;
    size_t permanentCapacity, transientCapacity;
    size_t permanentSize, transientSize;
};