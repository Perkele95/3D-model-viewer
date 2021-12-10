#include "../base.hpp"
#include "mv_tools_win32.hpp"
#include "../input.hpp"
#include "../mv_allocator.hpp"
#include "../model_viewer.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

static vec2<int32_t> *GlobalExtentPtr = nullptr;
static uint32_t *GlobalFlagsPtr = nullptr;
static HINSTANCE *GlobalInstancePtr = nullptr;
static HWND *GlobalWindowPtr = nullptr;

LRESULT CALLBACK MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch(message){
        case WM_SIZE:{
            GlobalExtentPtr->x = lParam & 0x0000FFFF;
            GlobalExtentPtr->y = (lParam >> 16) & 0x0000FFFF;
            *GlobalFlagsPtr |= CORE_FLAG_WINDOW_RESIZED;
        } break;

        case WM_APP:

        case WM_CLOSE:
        case WM_DESTROY: *GlobalFlagsPtr &= ~CORE_FLAG_RUNNING; break;
        default: result = DefWindowProc(window, message, wParam, lParam); break;
    }
    return result;
}

void SetSurface(VkInstance instance, VkSurfaceKHR *pSurface)
{
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = *GlobalWindowPtr;
    createInfo.hinstance = *GlobalInstancePtr;
    vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, pSurface);
}

void SetCursorVisibility(bool show)
{
    ShowCursor(BOOL(show));
}

struct win32_context
{
    win32_context(size_t virtualMemoryBufferSize)
    {
        this->perfCountFrequency = {};
        this->perfCounter = {};
        this->windowPlacement = {sizeof(WINDOWPLACEMENT)};
        this->instance = NULL;
        this->window = NULL;
        this->extent = vec2(0);
        this->flags = CORE_FLAG_RUNNING | CORE_FLAG_ENABLE_VALIDATION | CORE_FLAG_CONSTRAIN_MOUSE;
        this->dt = 0.0f;
        this->input.mouse = vec2(0i32);
        this->input.mousePrev = vec2(0i32);
        this->input.mouseDelta = vec2(0i32);

        QueryPerformanceFrequency(&this->perfCountFrequency);
        QueryPerformanceCounter(&this->perfCounter);

        GlobalExtentPtr = &this->extent;
        GlobalFlagsPtr = &this->flags;
        GlobalInstancePtr = &this->instance;
        GlobalWindowPtr = &this->window;

        this->virtualMemoryBuffer = VirtualAlloc(NULL, virtualMemoryBufferSize,
                                    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }

    ~win32_context()
    {
        VirtualFree(this->virtualMemoryBuffer, 0, MEM_RELEASE);
    }

    bool createWindow(LPCSTR lpWindowName)
    {
        WNDCLASSEXA windowClass{};
        windowClass.cbSize = sizeof(WNDCLASSEXA);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = MainWindowCallback;
        windowClass.hInstance = this->instance;
        windowClass.lpszClassName = "wnd_class";

        constexpr auto screenSize = vec2(1920i32, 1080i32);
        constexpr auto viewportSize = vec2(1280i32, 720i32);
        constexpr auto viewportOrigin = vec2(
            (screenSize.x / 2) - (viewportSize.x / 2),
            (screenSize.y / 2) - (viewportSize.y / 2)
        );

        const ATOM result = RegisterClassExA(&windowClass);
        if(result != 0){
            auto style = WS_VISIBLE | WS_SYSMENU;
            this->window = CreateWindowExA(0, windowClass.lpszClassName, lpWindowName,
                                           style,
                                           viewportOrigin.x, viewportOrigin.y,
                                           viewportSize.x, viewportSize.y,
                                           NULL, NULL, this->instance, NULL);

            SetWindowLong(this->window, GWL_STYLE, WS_VISIBLE & ~WS_OVERLAPPEDWINDOW);
            constexpr UINT setWindowPosFlags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                                               SWP_NOOWNERZORDER | SWP_FRAMECHANGED;
            SetWindowPos(this->window, NULL, 0,0,0,0, setWindowPosFlags);
        }

        return bool(result);
    }

    void update()
    {
        this->flags &= ~CORE_FLAG_WINDOW_RESIZED;

        LARGE_INTEGER counter{};
        QueryPerformanceCounter(&counter);
        this->dt = float(counter.QuadPart - this->perfCounter.QuadPart);
        this->dt /= float(this->perfCountFrequency.QuadPart);
        this->perfCounter = counter;
    }

    void processKeyPress(uint32_t keyCode)
    {
        switch(keyCode){
            case 'W': this->input.keyPressEvents |= KEY_EVENT_W; break;
            case 'A': this->input.keyPressEvents |= KEY_EVENT_A; break;
            case 'S': this->input.keyPressEvents |= KEY_EVENT_S; break;
            case 'D': this->input.keyPressEvents |= KEY_EVENT_D; break;
            case 'Q': this->input.keyPressEvents |= KEY_EVENT_Q; break;
            case 'E': this->input.keyPressEvents |= KEY_EVENT_E; break;
            case 'F': this->input.keyPressEvents |= KEY_EVENT_F; break;
            case VK_UP: this->input.keyPressEvents |= KEY_EVENT_UP; break;
            case VK_DOWN: this->input.keyPressEvents |= KEY_EVENT_DOWN; break;
            case VK_LEFT: this->input.keyPressEvents |= KEY_EVENT_LEFT; break;
            case VK_RIGHT: this->input.keyPressEvents |= KEY_EVENT_RIGHT; break;
            case VK_SPACE: this->input.keyPressEvents |= KEY_EVENT_SPACE; break;
            case VK_ESCAPE: this->input.keyPressEvents |= KEY_EVENT_ESCAPE; break;
            case VK_CONTROL: this->input.keyPressEvents |= KEY_EVENT_CONTROL; break;
            case VK_SHIFT: this->input.keyPressEvents |= KEY_EVENT_SHIFT; break;
            default: break;
        }
    }

    void pollEvents()
    {
        this->input.keyPressEvents = 0;
        this->input.mousePressEvents = 0;
        this->input.mouseWheel = 0;
        this->input.mousePrev = this->input.mouse;

        MSG message;
        while(PeekMessageA(&message, 0, 0, 0, PM_REMOVE | PM_QS_INPUT)){
            switch(message.message){
                case WM_MOUSEMOVE:{
                    const auto p = MAKEPOINTS(message.lParam);
                    this->input.mouse = vec2(int32_t(p.x), int32_t(p.y));
                    this->input.mousePressEvents |= MOUSE_EVENT_MOVE;
                } break;

                case WM_LBUTTONDOWN: this->input.mousePressEvents |= MOUSE_EVENT_LMB; break;
                case WM_RBUTTONDOWN: this->input.mousePressEvents |= MOUSE_EVENT_RMB; break;
                case WM_LBUTTONUP:
                case WM_RBUTTONUP:
                case WM_MBUTTONDOWN: this->input.mousePressEvents |= MOUSE_EVENT_MMB; break;
                case WM_MBUTTONUP:
                case WM_MOUSEWHEEL:{
                    auto wheelValue = GET_WHEEL_DELTA_WPARAM(message.wParam);
                    this->input.mouseWheel = -(wheelValue / 120);
                } break;
                case WM_SYSKEYUP:
                case WM_KEYUP: break;

                case WM_KEYDOWN:
                case WM_SYSKEYDOWN:{
                    uint32_t keyCode = static_cast<uint32_t>(message.wParam);
                    processKeyPress(keyCode);

                    const bool altKeyDown = (message.lParam & BIT(29));
                    if(altKeyDown && keyCode == VK_F4)
                        this->flags &= ~CORE_FLAG_RUNNING;
                } break;

                default:{
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                } break;
            }
        }

        GetWindowRect(this->window, &this->windowRect);
        const auto halfExtent = this->extent / 2;
        this->windowCentre = halfExtent + vec2(int32_t(this->windowRect.left), int32_t(this->windowRect.top));
        this->input.mouseDelta = this->input.mouse - this->input.mousePrev;

        if(this->flags & CORE_FLAG_CONSTRAIN_MOUSE){
            SetCursorPos(this->windowCentre.x, this->windowCentre.y);
            this->input.mouse = halfExtent;
        }

        GetKeyboardState(this->input.keyBoard);
    }

    // Thanks to Raymond Chan:
    // https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
    void toggleFullscreen()
    {
        const auto style = GetWindowLong(this->window, GWL_STYLE);
        if(style & WS_OVERLAPPEDWINDOW){
            auto monitor = MonitorFromWindow(this->window, MONITOR_DEFAULTTOPRIMARY);
            MONITORINFO mi = {sizeof(mi)};

            auto monitorInfo = GetMonitorInfoA(monitor, &mi);
            if(GetWindowPlacement(this->window, &this->windowPlacement) && monitorInfo){
                SetWindowLong(this->window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(this->window, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                             mi.rcMonitor.right - mi.rcMonitor.left,
                             mi.rcMonitor.bottom - mi.rcMonitor.top,
                             SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                this->flags |= CORE_FLAG_WINDOW_FULLSCREEN;
            }
        }
        else{
            SetWindowLong(this->window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
            SetWindowPlacement(this->window, &this->windowPlacement);
            constexpr UINT setWindowPosFlags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                                               SWP_NOOWNERZORDER | SWP_FRAMECHANGED;
            SetWindowPos(this->window, NULL, 0, 0, 0, 0, setWindowPosFlags);
            this->flags &= ~CORE_FLAG_WINDOW_FULLSCREEN;
        }
    }

    LARGE_INTEGER perfCountFrequency, perfCounter;
    WINDOWPLACEMENT windowPlacement;
    RECT windowRect;
    HINSTANCE instance;
    HWND window;
    vec2<int32_t> extent, windowCentre;
    input_state input;
    uint32_t flags;
    float dt;
    void *virtualMemoryBuffer;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    const auto permanentCapacity = MegaBytes(64);
    const auto transientCapacity = MegaBytes(512);
    win32_context context(permanentCapacity + transientCapacity);

    if(context.createWindow("3D model viewer") == false)
        return 0;

    context.flags &= ~CORE_FLAG_WINDOW_RESIZED;

    auto allocator = mv_allocator(context.virtualMemoryBuffer, permanentCapacity, transientCapacity);

    auto core = model_viewer(&allocator, context.extent, context.flags);

    while(context.flags & CORE_FLAG_RUNNING){
        context.pollEvents();
        core.run(&allocator, &context.input, &context.flags, context.dt);
        context.update();
    }

    return 0;
}