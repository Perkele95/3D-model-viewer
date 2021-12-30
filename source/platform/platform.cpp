#include "platform.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

namespace Platform
{
    struct logical_device
    {
        HINSTANCE instance;
        HWND window;
        WINDOWPLACEMENT windowPlacement;
        int64_t perfCounter;
        int64_t perfCountFrequency;
        input_state input;
        uint32_t flags;
    };

    extern const size_t DeviceSize = sizeof(logical_device);

    inline void *Map(size_t size)
    {
        auto ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        return static_cast<void*>(ptr);
    }

    inline bool Unmap(void *mapped)
    {
        const bool result = VirtualFree(mapped, 0, MEM_RELEASE);
        return result;
    }

    static uint32_t *s_FlagsPtr = nullptr;

    LRESULT CALLBACK MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        LRESULT result = 0;
        switch(message){
            case WM_SIZE: *s_FlagsPtr |= FLAG_WINDOW_RESIZED; break;

            case WM_CLOSE:
            case WM_DESTROY: *s_FlagsPtr &= ~FLAG_RUNNING; break;
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

    bool SetupWindow(lDevice device, const char *appName)
    {
        WNDCLASSEXA windowClass{};
        windowClass.cbSize = sizeof(WNDCLASSEXA);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = MainWindowCallback;
        windowClass.hInstance = device->instance;
        windowClass.lpszClassName = "wnd_class";
        const ATOM result = RegisterClassExA(&windowClass);

        constexpr auto screenSize = vec2(1920i32, 1080i32);
        constexpr auto viewportSize = vec2(1280i32, 720i32);
        const auto viewport = (screenSize / 2) - (viewportSize / 2);

        if(result != 0){
            device->window = CreateWindowExA(0, windowClass.lpszClassName, appName,
                                              WS_VISIBLE,
                                              viewport.x, viewport.y,
                                              viewportSize.x, viewportSize.y,
                                              NULL, NULL, device->instance, NULL);
            SetDefaultWindowStyle(device->window);
        }
        return bool(result);
    }

    void ToggleFullscreen(lDevice device)
    {
        if(device->flags & FLAG_WINDOW_FULLSCREEN){
            SetDefaultWindowStyle(device->window);
            SetWindowPlacement(device->window, &device->windowPlacement);
            device->flags &= ~FLAG_WINDOW_FULLSCREEN;
        }
        else{
            auto monitor = MonitorFromWindow(device->window, MONITOR_DEFAULTTOPRIMARY);
            MONITORINFO mi = {sizeof(mi)};
            auto monitorInfo = GetMonitorInfoA(monitor, &mi);
            if(GetWindowPlacement(device->window, &device->windowPlacement) && monitorInfo){
                SetWindowLong(device->window, GWL_STYLE, WS_VISIBLE & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(device->window, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                             mi.rcMonitor.right - mi.rcMonitor.left,
                             mi.rcMonitor.bottom - mi.rcMonitor.top,
                             SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                device->flags |= FLAG_WINDOW_FULLSCREEN;
            }
        }
        device->flags |= FLAG_WINDOW_RESIZED;
    }

    void Terminate(lDevice device)
    {
        device->flags &= ~FLAG_RUNNING;
    }

    float GetTimestep(lDevice device)
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);

        float dt = float(counter.QuadPart - device->perfCounter);
        dt /= float(device->perfCountFrequency);
        device->perfCounter = counter.QuadPart;
        return dt;
    }

    void SetupVkSurface(lDevice device, VkInstance instance, VkSurfaceKHR *pSurface)
    {
        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hinstance = device->instance;
        createInfo.hwnd = device->window;
        vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, pSurface);
    }

    void ProcessKeyPress(input_state *input, uint32_t keyCode)
    {
        switch(keyCode){
            case 'W': input->keyPressEvents |= KEY_EVENT_W; break;
            case 'A': input->keyPressEvents |= KEY_EVENT_A; break;
            case 'S': input->keyPressEvents |= KEY_EVENT_S; break;
            case 'D': input->keyPressEvents |= KEY_EVENT_D; break;
            case 'Q': input->keyPressEvents |= KEY_EVENT_Q; break;
            case 'E': input->keyPressEvents |= KEY_EVENT_E; break;
            case 'F': input->keyPressEvents |= KEY_EVENT_F; break;
            case VK_UP: input->keyPressEvents |= KEY_EVENT_UP; break;
            case VK_DOWN: input->keyPressEvents |= KEY_EVENT_DOWN; break;
            case VK_LEFT: input->keyPressEvents |= KEY_EVENT_LEFT; break;
            case VK_RIGHT: input->keyPressEvents |= KEY_EVENT_RIGHT; break;
            case VK_SPACE: input->keyPressEvents |= KEY_EVENT_SPACE; break;
            case VK_ESCAPE: input->keyPressEvents |= KEY_EVENT_ESCAPE; break;
            case VK_CONTROL: input->keyPressEvents |= KEY_EVENT_CONTROL; break;
            case VK_SHIFT: input->keyPressEvents |= KEY_EVENT_SHIFT; break;
            case VK_F4: input->keyPressEvents |= KEY_EVENT_F4; break;
            default: break;
        }
    }

    void ProcessEvents(lDevice device, MSG event)
    {
        switch(event.message){
            case WM_MOUSEMOVE:{
                const auto p = MAKEPOINTS(event.lParam);
                device->input.mouse = vec2(int32_t(p.x), int32_t(p.y));
                device->input.mousePressEvents |= MOUSE_EVENT_MOVE;
                break;
            }

            case WM_LBUTTONDOWN: device->input.mousePressEvents |= MOUSE_EVENT_LMB; break;
            case WM_RBUTTONDOWN: device->input.mousePressEvents |= MOUSE_EVENT_RMB; break;
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN: device->input.mousePressEvents |= MOUSE_EVENT_MMB; break;

            case WM_MOUSEWHEEL:{
                auto wheelValue = GET_WHEEL_DELTA_WPARAM(event.wParam);
                device->input.mouseWheel = -(wheelValue / 120);
                break;
            }

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:{
                const auto keyCode = static_cast<uint32_t>(event.wParam);
                ProcessKeyPress(&device->input, keyCode);

                const bool altKeyDown = HIWORD(event.lParam) & KF_ALTDOWN;
                if(altKeyDown && keyCode == VK_F4)
                    device->flags &= ~FLAG_RUNNING;
                break;
            }

            default:{
                TranslateMessage(&event);
                DispatchMessage(&event);
                break;
            }
        };
    }

    const input_state *PollEvents(lDevice device)
    {
        device->input.keyPressEvents = 0;
        device->input.mousePressEvents = 0;
        device->input.mouseWheel = 0;
        device->input.mousePressEvents = 0;
        device->flags &= ~FLAG_WINDOW_RESIZED;

        MSG event;
        while(PeekMessage(&event, device->window, 0, 0, PM_REMOVE | PM_QS_INPUT))
            ProcessEvents(device, event);

        return &device->input;
    }

    uint32_t QueryFlags(lDevice device)
    {
        return device->flags;
    }

    bool IsKeyDown(KeyCode key)
    {
        const auto keyCode = static_cast<int32_t>(key);
        const auto result = GetKeyState(keyCode) & 0xFF00;
        return bool(result);
    }

    namespace io
    {
        bool close(file_t *file)
        {
            const bool result = Unmap(file->handle);
            file->handle = nullptr;
            file->size = 0;
            return result;
        }

        file_t read(const char *filename)
        {
            HANDLE fileHandle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ,
                                            0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
            LARGE_INTEGER fileSize;
            file_t result{};

            if((fileHandle != INVALID_HANDLE_VALUE) && GetFileSizeEx(fileHandle, &fileSize)){
                uint32_t fileSize32 = static_cast<uint32_t>(fileSize.QuadPart);
                result.handle = Map(fileSize.QuadPart);
                if(result.handle){
                    DWORD bytesRead;
                    if(ReadFile(fileHandle, result.handle, fileSize32, &bytesRead, 0) && fileSize32 == bytesRead){
                        result.size = fileSize32;
                    }
                    else{
                        Unmap(result.handle);
                        result.handle = nullptr;
                    }
                }
                CloseHandle(fileHandle);
            }
            return result;
        }

        bool write(const char *filename, file_t *file)
        {
            bool result = false;
            HANDLE fileHandle = CreateFileA(filename, GENERIC_WRITE, 0,
                                0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
            if(fileHandle != INVALID_HANDLE_VALUE){
                DWORD bytesWritten;
                if(WriteFile(fileHandle, file->handle, static_cast<DWORD>(file->size), &bytesWritten, 0))
                    result = (file->size == bytesWritten);

                CloseHandle(fileHandle);
            }
            return result;
        }

        bool fileExists(const char *filename)
        {
            WIN32_FIND_DATA findData{};
            HANDLE hFindFile = FindFirstFileA(filename, &findData);
            bool result = false;
            if(hFindFile != INVALID_HANDLE_VALUE){
                result = true;
                FindClose(hFindFile);
            }
            return result;
        }
    }
}

void StartTimestepClock(Platform::lDevice device)
{
    LARGE_INTEGER perfCountFrequencyResult{};
    QueryPerformanceFrequency(&perfCountFrequencyResult);
    device->perfCountFrequency = perfCountFrequencyResult.QuadPart;

    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    device->perfCounter = counter.QuadPart;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nShowCmd)
{
    Platform::logical_device device;
    device.instance = hInstance;
    device.window = NULL;
    device.flags = Platform::FLAG_RUNNING;
    device.windowPlacement = {sizeof(WINDOWPLACEMENT)};

    Platform::s_FlagsPtr = &device.flags;
    StartTimestepClock(&device);

    ApplicationEntryPoint(&device);
}