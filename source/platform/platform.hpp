#pragma once

#include <stdint.h>

#include "../mv_utils/vec.hpp"

struct file_t
{
    void *handle;
    size_t size;
};

namespace Platform
{
    enum FLAG_BITS
    {
        FLAG_RUNNING = (1 << 0),
        FLAG_WINDOW_RESIZED = (1 << 1),
        FLAG_WINDOW_FULLSCREEN = (1 << 2),
    };

    // Logical device for platform
    // Defined in platform compilation unit
    struct logical_device;
    using lDevice = logical_device*;

    // Sizeof(device_T), use to allocate device buffer
    extern const size_t DeviceSize;

    void *Map(size_t size);
    bool Unmap(void *mapped);

    namespace io
    {
        bool close(file_t *file);
        file_t read(const char *filename);
        bool write(const char *filename, file_t *file);
        bool fileExists(const char *filename);
    }

    bool SetupWindow(lDevice device, const char *appName);
    void ToggleFullscreen(lDevice device);
    void Terminate(lDevice device);
    float GetTimestep(lDevice device);
}

// Vulkan specific surface creation

struct VkInstance_T;
struct VkSurfaceKHR_T;
using VkInstance = VkInstance_T*;
using VkSurfaceKHR = VkSurfaceKHR_T*;

namespace Platform
{
    void SetupVkSurface(lDevice device, VkInstance instance, VkSurfaceKHR *pSurface);
}

using key_t = uint8_t;
using event_flags = uint32_t;

struct input_state
{
    event_flags keyPressEvents;
    event_flags mousePressEvents;
    key_t keyBoard[UINT8_MAX];
    int32_t mouseWheel;
    vec2<int32_t> mouse, mousePrev, mouseDelta;
};

enum class KeyCode : key_t
{
    LMBUTTON = 0x01,
    RMBUTTON = 0x02,
    SHIFT = 0x10,
    CTRL = 0x11,
    ALT = 0x12,
    NUM0 = 0x30,
    NUM1 = 0x31,
    NUM2 = 0x32,
    NUM3 = 0x33,
    NUM4 = 0x34,
    NUM5 = 0x35,
    NUM6 = 0x36,
    NUM7 = 0x37,
    NUM8 = 0x38,
    NUM9 = 0x39,
    A = 0x41,
    D = 0x44,
    S = 0x53,
    W = 0x57,
    LEFT = 0x25,
    UP = 0x26,
    RIGHT = 0x27,
    DOWN = 0x28,
};

enum KEY_EVENT_BITS
{
    KEY_EVENT_W = (1 << 0),
    KEY_EVENT_S = (1 << 1),
    KEY_EVENT_A = (1 << 2),
    KEY_EVENT_D = (1 << 3),
    KEY_EVENT_Q = (1 << 4),
    KEY_EVENT_E = (1 << 5),
    KEY_EVENT_UP = (1 << 6),
    KEY_EVENT_DOWN = (1 << 7),
    KEY_EVENT_LEFT = (1 << 8),
    KEY_EVENT_RIGHT = (1 << 9),
    KEY_EVENT_SPACE = (1 << 10),
    KEY_EVENT_ESCAPE = (1 << 11),
    KEY_EVENT_CONTROL = (1 << 12),
    KEY_EVENT_SHIFT = (1 << 13),
    KEY_EVENT_F = (1 << 14),
    KEY_EVENT_F4 = (1 << 15),
};

enum MOUSE_EVENT_BITS
{
    MOUSE_EVENT_MOVE = (1 << 0),
    MOUSE_EVENT_LMB = (1 << 1),
    MOUSE_EVENT_RMB = (1 << 2),
    MOUSE_EVENT_MMB = (1 << 3),
};

namespace Platform
{
    const input_state *PollEvents(lDevice device);
    uint32_t QueryFlags(lDevice device);
    bool IsKeyDown(KeyCode key);
}

void ApplicationEntryPoint(Platform::lDevice device);