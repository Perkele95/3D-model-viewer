#include "base.hpp"
#include "model_viewer.hpp"

void ApplicationEntryPoint(Platform::lDevice device)
{
    const auto permanentCapacity = MegaBytes(64);
    const auto transientCapacity = MegaBytes(512);

    Platform::SetupWindow(device, "3D model viewer");

    auto virtualBuffer = Platform::Map(permanentCapacity + transientCapacity);
    auto allocator = mv_allocator(virtualBuffer, permanentCapacity, transientCapacity);

    auto app = model_viewer(device, &allocator);

    float dt = 0.001f;
    auto flags = Platform::QueryFlags(device);
    while (flags & Platform::FLAG_RUNNING){
        auto input = Platform::PollEvents(device);

        app.run(&allocator, input, flags, dt);

        flags = Platform::QueryFlags(device);
        dt = Platform::GetTimestep(device);
    }
}