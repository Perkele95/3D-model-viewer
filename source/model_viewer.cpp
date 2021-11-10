#include "model_viewer.hpp"

model_viewer::model_viewer(mv_allocator *allocator, vec2<int32_t> extent, uint32_t flags)
: context(allocator, flags & CORE_FLAG_ENABLE_VALIDATION, flags & CORE_FLAG_ENABLE_VSYNC)
{
    //auto teapot = io::read("assets/utah_teapot.obj");
}

model_viewer::~model_viewer()
{
    //
}

void model_viewer::run(mv_allocator *allocator, vec2<int32_t> extent, float dt)
{
    //
}