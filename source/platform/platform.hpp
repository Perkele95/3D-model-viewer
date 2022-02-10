#pragma once

#include <stdint.h>

#include "../mv_utils/vec.hpp"

// Vulkan specific forward declarations
// For surface creation

struct VkInstance_T;
struct VkSurfaceKHR_T;
using VkInstance = VkInstance_T*;
using VkSurfaceKHR = VkSurfaceKHR_T*;

namespace plt
{
    enum class core_flag
    {
        running = (1 << 0),
        window_resized = (1 << 1),
        window_fullscreen = (1 << 2),
    };

    struct device_T;
    using device = device_T*;

    extern const size_t DEVICE_SIZE;

    constexpr auto SCREEN_SIZE = vec2(1920i32, 1080i32);
    constexpr auto VIEWPORT_SIZE = vec2(1280i32, 720i32);

    // Window

    bool SpawnWindow(device d, const char *title);
    void ToggleFullScreen(device d);
    void Terminate(device d);

    // Timestep/clock

    float GetTimestep(device d);

    // Vulkan surface

    void CreateSurface(device d, VkInstance instance, VkSurfaceKHR *pSurface);

    // Files

    using file_path = const char*;
    using file_handle = void*;

    struct file
    {
        file_handle handle;
        size_t size;
    };

    namespace filesystem
    {
        file read(file_path path);
        bool write(file_path path, file &file);
        bool close(file &file);
        bool exists(file_path path);
    }

    // Input

    using event_type = uint32_t;

    void PollEvents(device d);
    bool GetFlag(device d, core_flag flag);
    void SetFlag(device d, core_flag flag);

    enum class key_code : uint8_t
    {
        lmb = 0x01,
        rmb = 0x02,
        shift = 0x10,
        ctrl = 0x11,
        alt = 0x12,
        num0 = 0x30,
        num1 = 0x31,
        num2 = 0x32,
        num3 = 0x33,
        num4 = 0x34,
        num5 = 0x35,
        num6 = 0x36,
        num7 = 0x37,
        num8 = 0x38,
        num9 = 0x39,
        a = 0x41,
        d = 0x44,
        s = 0x53,
        w = 0x57,
        left = 0x25,
        up = 0x26,
        right = 0x27,
        down = 0x28,
    };

    bool IsKeyDown(key_code key);

    enum class key_event
    {
        w = 1,
        a = 1 << 1,
        s = 1 << 2,
        d = 1 << 3,
        q = 1 << 4,
        e = 1 << 5,
        f = 1 << 6,
        up = 1 << 7,
        down = 1 << 8,
        left = 1 << 9,
        right = 1 << 10,
        space = 1 << 11,
        escape = 1 << 12,
        control = 1 << 13,
        shift = 1 << 14,
        f4 = 1 << 15,
    };

    bool KeyEvent(device d, key_event e);

    enum class mouse_event
    {
        move = 1,
        lmb = 1 << 1,
        rmb = 1 << 2,
        mmb = 1 << 3,
    };

    bool MouseEvent(device d, mouse_event e);
}

// Entrypoint with pre-initialised logical device
// Defined by target application
void EntryPoint(plt::device d);