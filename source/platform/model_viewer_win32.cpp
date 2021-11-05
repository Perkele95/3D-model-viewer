#include "../base.hpp"
#include "../input.hpp"
#include "../mv_allocator.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
// TODO(arle): consider moving io to separate header file
namespace io
{
    bool close(file_t &file)
    {
        const bool result = VirtualFree(file.handle, 0, MEM_RELEASE);
        file.handle = nullptr;
        file.size = 0;
        return result;
    }

    file_t read(const char *filename)
    {
        HANDLE fileHandle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        LARGE_INTEGER fileSize;
        file_t result{};

        if((fileHandle != INVALID_HANDLE_VALUE) && GetFileSizeEx(fileHandle, &fileSize)){
            uint32_t fileSize32 = static_cast<uint32_t>(fileSize.QuadPart);
            result.handle = VirtualAlloc(NULL, fileSize.QuadPart, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if(result.handle){
                DWORD bytesRead;
                if(ReadFile(fileHandle, result.handle, fileSize32, &bytesRead, 0) && fileSize32 == bytesRead){
                    result.size = fileSize32;
                }
                else{
                    VirtualFree(result.handle, 0, MEM_RELEASE);
                    result.handle = nullptr;
                }
            }
            CloseHandle(fileHandle);
        }
        return result;
    }

    bool write(const char *filename, file_t &file)
    {
        bool result = false;
        HANDLE fileHandle = CreateFileA(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        if(fileHandle != INVALID_HANDLE_VALUE){
            DWORD bytesWritten;
            if(WriteFile(fileHandle, file.handle, static_cast<DWORD>(file.size), &bytesWritten, 0))
                result = (file.size == bytesWritten);

            CloseHandle(fileHandle);
        }
        return result;
    }

    bool fileExists(const char *filename)
    {
        WIN32_FIND_DATA findData;
        HANDLE hFindFile = FindFirstFileA(filename, &findData);
        bool result = false;
        if(hFindFile != INVALID_HANDLE_VALUE){
            result = true;
            FindClose(hFindFile);
        }
        return result;
    }

    file_t mapFile(size_t size)
    {
        file_t file{};
        file.handle = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        file.size = size;
        return file;
    }
}

static vec2<int32_t> *GlobalExtentPtr = nullptr;
static uint32_t *GlobalFlagsPtr = nullptr;

LRESULT CALLBACK MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch(message){
        case WM_SIZE:
            GlobalExtentPtr->x = lParam & 0x0000FFFF;
            GlobalExtentPtr->y = (lParam >> 16) & 0x0000FFFF;
            *GlobalFlagsPtr |= CORE_FLAG_WINDOW_RESIZED;
            break;

        case WM_CLOSE:
        case WM_DESTROY: *GlobalFlagsPtr &= ~CORE_FLAG_RUNNING; break;
        default: result = DefWindowProc(window, message, wParam, lParam); break;
    }
    return result;
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
        this->flags = CORE_FLAG_RUNNING;
        this->dt = 0.0f;

        QueryPerformanceFrequency(&this->perfCountFrequency);
        QueryPerformanceCounter(&this->perfCounter);

        GlobalExtentPtr = &this->extent;
        GlobalFlagsPtr = &this->flags;

        this->virtualMemoryBuffer = VirtualAlloc(NULL, virtualMemoryBufferSize,
                                    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }

    ~win32_context()
    {
        VirtualFree(this->virtualMemoryBuffer, 0, MEM_RELEASE);
    }

    bool createWindow(LPCSTR lpWindowName)
    {
        WNDCLASSA windowClass{};
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = MainWindowCallback;
        windowClass.hInstance = this->instance;
        windowClass.hIcon = NULL;
        windowClass.lpszClassName = "wnd_class";

        const ATOM result = RegisterClassA(&windowClass);
        if(result != 0){
            this->window = CreateWindowExA(0, windowClass.lpszClassName, lpWindowName,
                                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                                CW_USEDEFAULT, CW_USEDEFAULT,
                                                CW_USEDEFAULT, CW_USEDEFAULT,
                                                NULL, NULL, this->instance, NULL);
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

        MSG message;
        while(PeekMessageA(&message, 0, 0, 0, PM_REMOVE | PM_QS_INPUT)){
            switch(message.message){
                case WM_MOUSEMOVE:{
                    this->input.mouse.x = message.lParam & 0x0000FFFF;
                    this->input.mouse.y = (message.lParam >> 16) & 0x0000FFFF;
                    this->input.mousePressEvents |= MOUSE_EVENT_MOVE;
                }break;

                case WM_LBUTTONDOWN: this->input.mousePressEvents |= MOUSE_EVENT_LMB; break;
                case WM_RBUTTONDOWN: this->input.mousePressEvents |= MOUSE_EVENT_RMB; break;
                case WM_LBUTTONUP:
                case WM_RBUTTONUP:
                case WM_MBUTTONDOWN: this->input.mousePressEvents |= MOUSE_EVENT_MMB; break;
                case WM_MBUTTONUP:
                case WM_MOUSEWHEEL:{
                    auto wheelValue = GET_WHEEL_DELTA_WPARAM(message.wParam);
                    this->input.mouseWheel = -(wheelValue / 120);
                }break;
                case WM_SYSKEYUP:
                case WM_KEYUP: break;

                case WM_KEYDOWN:
                case WM_SYSKEYDOWN:{
                    uint32_t keyCode = static_cast<uint32_t>(message.wParam);
                    processKeyPress(keyCode);
                    bool altKeyDown = (message.lParam & BIT(29));
                    if(message.lParam & BIT(29))
                        this->flags &= ~CORE_FLAG_RUNNING;
                }break;

                default:{
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }break;
            }
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
    HINSTANCE instance;
    HWND window;
    vec2<int32_t> extent;
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

    auto allocator = mv_allocator(context.virtualMemoryBuffer, permanentCapacity, transientCapacity);

    // Core->construct()
    while(context.flags & CORE_FLAG_RUNNING){
        context.pollEvents();

        // Core->run()

        context.update();
    }
    // Core->destruct()

    return 0;
}