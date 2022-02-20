#include "platform.hpp"

#include <vulkan/vulkan.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <vulkan/vulkan_win32.h>

namespace plt
{
    static uint32_t *s_FlagsPtr = nullptr;

    struct device_T
    {
        device_T(HINSTANCE hInstance)
            : keyEventsDown(0), mouseEventsDown(0),
            mouse(vec2(0i32)), mouseDelta(vec2(0i32)),
            mousePrev(vec2(0i32)), mouseWheel(0)
        {
            instance = hInstance;
            window = NULL;
            flags = uint32_t(plt::core_flag::running);
            windowPlacement = {sizeof(WINDOWPLACEMENT)};

            s_FlagsPtr = &flags;

            LARGE_INTEGER perfCountFrequencyResult{};
            QueryPerformanceFrequency(&perfCountFrequencyResult);
            perfCountFrequency = perfCountFrequencyResult.QuadPart;

            LARGE_INTEGER counter{};
            QueryPerformanceCounter(&counter);
            perfCounter = counter.QuadPart;
        }

        ~device_T()
        {
            //
        }

        HINSTANCE instance;
        HWND window;
        WINDOWPLACEMENT windowPlacement;
        int64_t perfCounter;
        int64_t perfCountFrequency;
        uint32_t flags;

        // Input

        event_type keyEventsDown;
        event_type mouseEventsDown;
        int32_t mouseWheel;
        vec2<int32_t> mouse, mousePrev, mouseDelta;
    };

    extern const size_t DEVICE_SIZE = sizeof(device_T);

    // Window

    LRESULT CALLBACK MainWindowCallback(HWND window, UINT message,
                                        WPARAM wParam, LPARAM lParam)
    {
        LRESULT result = 0;
        switch(message){
            case WM_SIZE: *s_FlagsPtr |= uint32_t(core_flag::window_resized); break;
            case WM_CLOSE:
            case WM_DESTROY: *s_FlagsPtr &= ~uint32_t(core_flag::running); break;
            default: result = DefWindowProc(window, message, wParam, lParam); break;
        }
        return result;
    }

    inline void SetDefaultWindowStyle(HWND window)
    {
        SetWindowLong(window, GWL_STYLE, WS_VISIBLE & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(window, NULL, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                                                  SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }

    bool SpawnWindow(device d, const char *title)
    {
        WNDCLASSEXA windowClass{};
        windowClass.cbSize = sizeof(WNDCLASSEXA);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = MainWindowCallback;
        windowClass.hInstance = d->instance;
        windowClass.lpszClassName = "wnd_class";
        const ATOM result = RegisterClassExA(&windowClass);

        const auto viewport = (SCREEN_SIZE / 2) - (VIEWPORT_SIZE / 2);

        if(result != 0){
            d->window = CreateWindowExA(0, windowClass.lpszClassName, title,
                                              WS_VISIBLE,
                                              viewport.x, viewport.y,
                                              VIEWPORT_SIZE.x, VIEWPORT_SIZE.y,
                                              NULL, NULL, d->instance, NULL);
            SetDefaultWindowStyle(d->window);
        }
        return bool(result);
    }

    void ToggleFullScreen(device d)
    {
        if(d->flags & uint32_t(core_flag::window_fullscreen)){
            SetDefaultWindowStyle(d->window);
            SetWindowPlacement(d->window, &d->windowPlacement);
            d->flags &= ~uint32_t(core_flag::window_fullscreen);
        }
        else{
            auto monitor = MonitorFromWindow(d->window, MONITOR_DEFAULTTOPRIMARY);
            MONITORINFO mi = {sizeof(mi)};
            auto monitorInfo = GetMonitorInfoA(monitor, &mi);
            if(GetWindowPlacement(d->window, &d->windowPlacement) && monitorInfo){
                SetWindowLong(d->window, GWL_STYLE, WS_VISIBLE & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(d->window, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                             mi.rcMonitor.right - mi.rcMonitor.left,
                             mi.rcMonitor.bottom - mi.rcMonitor.top,
                             SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                d->flags |= uint32_t(core_flag::window_fullscreen);
            }
        }
        d->flags |= uint32_t(core_flag::window_resized);
    }

    void Terminate(device d)
    {
        d->flags &= ~uint32_t(core_flag::running);
    }

    // Timestep/clock

    float GetTimestep(device d)
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);

        float dt = float(counter.QuadPart - d->perfCounter);
        dt /= float(d->perfCountFrequency);
        d->perfCounter = counter.QuadPart;
        return dt;
    }

    // Vulkan surface

    void CreateSurface(device d, VkInstance instance, VkSurfaceKHR *pSurface)
    {
        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hinstance = d->instance;
        createInfo.hwnd = d->window;
        vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, pSurface);
    }

    // Files

    namespace filesystem
    {
        file read(path filePath)
        {
            HANDLE fileHandle = CreateFileA(filePath, GENERIC_READ, FILE_SHARE_READ,
                                            0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

            file f = {};
            LARGE_INTEGER fileSize;
            if((fileHandle != INVALID_HANDLE_VALUE) && GetFileSizeEx(fileHandle, &fileSize)){
                const auto size = uint32_t(fileSize.QuadPart);
                f.handle = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

                if(f.handle != nullptr){
                    DWORD bytesRead;
                    if(ReadFile(fileHandle, f.handle, size, &bytesRead, 0) && size == bytesRead){
                        f.size = size;
                    }
                    else{
                        close(f);
                    }
                }
                CloseHandle(fileHandle);
            }
            return f;
        }

        bool write(path filePath, file &file)
        {
            HANDLE fileHandle = CreateFileA(filePath, GENERIC_WRITE, 0,
                                0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
            bool result = false;
            if(fileHandle != INVALID_HANDLE_VALUE){
                DWORD bytesWritten;
                auto r = WriteFile(fileHandle, file.handle, DWORD(file.size), &bytesWritten, 0);
                result = r && (bytesWritten == file.size);
                CloseHandle(fileHandle);
            }

            return result;
        }

        bool close(file &file)
        {
            const bool result = VirtualFree(file.handle, 0, MEM_RELEASE);
            file.handle = nullptr;
            file.size = 0;
            return result;
        }

        bool exists(path filePath)
        {
            WIN32_FIND_DATAA findData{};
            HANDLE hFindFile = FindFirstFileA(filePath, &findData);

            bool result = false;
            if(hFindFile != INVALID_HANDLE_VALUE){
                FindClose(hFindFile);
                result = true;
            }
            return result;
        }
    }

    inline void ProcessKeyPress(device d, WPARAM key)
    {
        switch(key){
            case 'W': d->keyEventsDown |= event_type(key_event::w); break;
            case 'A': d->keyEventsDown |= event_type(key_event::a); break;
            case 'S': d->keyEventsDown |= event_type(key_event::s); break;
            case 'D': d->keyEventsDown |= event_type(key_event::d); break;
            case 'Q': d->keyEventsDown |= event_type(key_event::q); break;
            case 'E': d->keyEventsDown |= event_type(key_event::e); break;
            case 'F': d->keyEventsDown |= event_type(key_event::f); break;
            case VK_UP: d->keyEventsDown |= event_type(key_event::up); break;
            case VK_DOWN: d->keyEventsDown |= event_type(key_event::down); break;
            case VK_LEFT: d->keyEventsDown |= event_type(key_event::left); break;
            case VK_RIGHT: d->keyEventsDown |= event_type(key_event::right); break;
            case VK_SPACE: d->keyEventsDown |= event_type(key_event::space); break;
            case VK_ESCAPE: d->keyEventsDown |= event_type(key_event::escape); break;
            case VK_CONTROL: d->keyEventsDown |= event_type(key_event::control); break;
            case VK_SHIFT: d->keyEventsDown |= event_type(key_event::shift); break;
            case VK_F4: d->keyEventsDown |= event_type(key_event::f4); break;
            default: break;
        }
    }

    void PollEvents(device d)
    {
        d->keyEventsDown = event_type(0);
        d->mouseEventsDown = event_type(0);
        d->mouseWheel = 0;
        d->flags &= ~uint32_t(core_flag::window_resized);

        MSG event;
        while(PeekMessage(&event, d->window, 0, 0, PM_REMOVE | PM_QS_INPUT)){
            switch (event.message){
                case WM_MOUSEMOVE:{
                    const auto p = MAKEPOINTS(event.lParam);
                    d->mouse = vec2(int32_t(p.x), int32_t(p.y));
                    d->mouseEventsDown |= event_type(mouse_event::move);
                } break;

                case WM_LBUTTONDOWN: d->mouseEventsDown |= event_type(mouse_event::lmb); break;
                case WM_RBUTTONDOWN: d->mouseEventsDown |= event_type(mouse_event::rmb); break;
                case WM_MBUTTONDOWN: d->mouseEventsDown |= event_type(mouse_event::mmb); break;

                case WM_MOUSEWHEEL:{
                    auto wheelValue = GET_WHEEL_DELTA_WPARAM(event.wParam);
                    d->mouseWheel = -(wheelValue / 120);
                } break;

                case WM_KEYDOWN:
                case WM_SYSKEYDOWN: ProcessKeyPress(d, event.wParam); break;

                default:{
                    TranslateMessage(&event);
                    DispatchMessage(&event);
                } break;
            }
        }
    }

    bool GetFlag(device d, core_flag flag)
    {
        return bool(d->flags & uint32_t(flag));
    }

    void SetFlag(device d, core_flag flag)
    {
        d->flags |= uint32_t(flag);
    }

    bool IsKeyDown(key_code key)
    {
        const auto result = GetKeyState(int(key)) & 0xFF00;
        return bool(result);
    }

    bool KeyEvent(device d, key_event e)
    {
        return bool(d->keyEventsDown & event_type(e));
    }

    bool MouseEvent(device d, mouse_event e)
    {
        return bool(d->mouseEventsDown & event_type(e));
    }
}

INT WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
                   _In_ PSTR lpCmdLine, _In_ INT nCmdShow)
{
    auto device = plt::device_T(hInstance);
    EntryPoint(&device);
    return 0;
}