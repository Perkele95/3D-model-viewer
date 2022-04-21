#include "base.hpp"
#include "platform/platform.hpp"
#include "model_viewer.hpp"

int EntryPoint(pltf::logical_device device)
{
    auto app = ModelViewer(device);
    app.run();
    return 0;
}