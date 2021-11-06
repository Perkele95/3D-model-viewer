#pragma once

#include "base.hpp"
#include "mv_allocator.hpp"

struct model_viewer
{
    model_viewer(mv_allocator *allocator, vec2<int32_t> extent, uint32_t flags);
    ~model_viewer();

    model_viewer(const model_viewer &src) = delete;
    model_viewer(const model_viewer &&src) = delete;
    model_viewer &operator=(const model_viewer &src) = delete;
    model_viewer &operator=(const model_viewer &&src) = delete;

    void run(mv_allocator *allocator, vec2<int32_t> extent, float dt);
};