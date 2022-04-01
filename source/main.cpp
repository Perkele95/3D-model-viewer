#include "base.hpp"
#include "platform/platform.hpp"
#include "model_viewer.hpp"

void WindowSizeDispatch(pltf::logical_device device, int32_t width, int32_t height)
{
    auto app = static_cast<ModelViewer*>(pltf::DeviceGetHandle(device));
    app->onWindowSize(width, height);
}

void KeyEventDispatch(pltf::logical_device device, pltf::key_code key, pltf::modifier mod)
{
    auto app = static_cast<ModelViewer*>(pltf::DeviceGetHandle(device));
    app->onKeyEvent(key, mod);
}

void MouseEventDispatch(pltf::logical_device device, pltf::mouse_button button)
{
    auto app = static_cast<ModelViewer*>(pltf::DeviceGetHandle(device));
    app->onMouseButtonEvent(button);
}

void ScrollWheelDispatch(pltf::logical_device device, double x, double y)
{
    auto app = static_cast<ModelViewer*>(pltf::DeviceGetHandle(device));
    app->onScrollWheelEvent(x, y);
}

int EntryPoint(pltf::logical_device device)
{
    auto app = ModelViewer(device);

    pltf::DeviceSetHandle(device, &app);
    pltf::EventsSetWindowSizeProc(device, WindowSizeDispatch);
    pltf::EventsSetKeyDownProc(device, KeyEventDispatch);
    pltf::EventsSetMouseDownProc(device, MouseEventDispatch);
    pltf::EventsSetScrollWheelProc(device, ScrollWheelDispatch);

    app.run();

    return 0;
}