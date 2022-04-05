#include "base.hpp"
#include "platform/platform.hpp"
#include "model_viewer.hpp"

template<typename T>
class EventDispatcher
{
public:
    EventDispatcher(pltf::logical_device device, T *handle)
    {
        pltf::DeviceSetHandle(device, handle);
    }

    static void WindowSize(pltf::logical_device device, int32_t width, int32_t height)
    {
        auto app = static_cast<T*>(pltf::DeviceGetHandle(device));
        app->onWindowSize(width, height);
    }

    static void KeyEvent(pltf::logical_device device, pltf::key_code key, pltf::modifier mod)
    {
        auto app = static_cast<T*>(pltf::DeviceGetHandle(device));
        app->onKeyEvent(key, mod);
    }

    static void MouseEvent(pltf::logical_device device, pltf::mouse_button button)
    {
        auto app = static_cast<T*>(pltf::DeviceGetHandle(device));
        app->onMouseButtonEvent(button);
    }

    static void ScrollWheel(pltf::logical_device device, double x, double y)
    {
        auto app = static_cast<T*>(pltf::DeviceGetHandle(device));
        app->onScrollWheelEvent(x, y);
    }
};

int EntryPoint(pltf::logical_device device)
{
    auto app = ModelViewer(device);
    auto dispatcher = EventDispatcher<ModelViewer>(device, &app);

    pltf::EventsSetWindowSizeProc(device, dispatcher.WindowSize);
    pltf::EventsSetKeyDownProc(device, dispatcher.KeyEvent);
    pltf::EventsSetMouseDownProc(device, dispatcher.MouseEvent);
    pltf::EventsSetScrollWheelProc(device, dispatcher.ScrollWheel);

    app.run();

    return 0;
}