#include "base.hpp"
#include "platform/platform.hpp"
#include "model_viewer.hpp"

void KeyEventDispatch(pltf::logical_device device, pltf::key_code key, pltf::modifier mod)
{
    auto handle = pltf::DeviceGetHandle(device);
    auto object = reinterpret_cast<model_viewer*>(handle);
    object->onKeyEvent(device, key, mod);
}

void MouseEventDispatch(pltf::logical_device device, pltf::mouse_button button)
{
    auto handle = pltf::DeviceGetHandle(device);
    auto object = reinterpret_cast<model_viewer*>(handle);
    object->onMouseButtonEvent(device, button);
}

void ScrollWheelDispatch(pltf::logical_device device, double x, double y)
{
    auto handle = pltf::DeviceGetHandle(device);
    auto object = reinterpret_cast<model_viewer*>(handle);
    object->onScrollWheelEvent(device, x, y);
}

int EntryPoint(pltf::logical_device device)
{
    pltf::WindowCreate(device, "3D model viewer");

    auto app = model_viewer(device);

    pltf::DeviceSetHandle(device, &app);
    pltf::EventsSetKeyDownProc(device, KeyEventDispatch);
    pltf::EventsSetMouseDownProc(device, MouseEventDispatch);
    pltf::EventsSetScrollWheelProc(device, ScrollWheelDispatch);

    while(pltf::IsRunning()){
        pltf::EventsPoll(device);
        app.swapBuffers(device);
    }

    return 0;
}