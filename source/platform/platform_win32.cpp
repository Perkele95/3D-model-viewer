#include "platform.hpp"

#include <vulkan/vulkan.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <vulkan/vulkan_win32.h>

namespace pltf
{
	struct logical_device_T
	{
		HINSTANCE instance;
		HWND window;
		WINDOWPLACEMENT windowPlacement;

		LARGE_INTEGER counter;
		LARGE_INTEGER counterFrequency;
        timestep_type dt;

        key_event_callback keyEventCallback;
        mouse_move_callback mouseMoveCallback;
        mouse_button_callback mouseButtonCallback;
		scroll_wheel_callback scrollWheelCallback;

		void *callbackHandle;
	};

	static bool s_Running = false;
	static bool s_WindowResized = false;
	static bool s_WindowFullscreen = false;

	LRESULT CALLBACK MainWindowCallback(HWND window, UINT message,
										WPARAM wParam, LPARAM lParam)
	{
		LRESULT result = 0;
		switch (message) {
			case WM_SIZE: s_WindowResized = true; break;
			case WM_CLOSE:
			case WM_DESTROY: s_Running = false; break;
			default: result = DefWindowProc(window, message, wParam, lParam); break;
		}
		return result;
	}

	inline void SetDefaultWindowStyle(HWND window)
	{
		SetWindowLong(window, GWL_STYLE, WS_VISIBLE & ~WS_OVERLAPPEDWINDOW);
		SetWindowPos(window, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
					 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}

	bool WindowCreate(logical_device device, const char* title)
	{
		WNDCLASSEXA windowClass{};
		windowClass.cbSize = sizeof(WNDCLASSEXA);
		windowClass.style = CS_HREDRAW | CS_VREDRAW;
		windowClass.lpfnWndProc = MainWindowCallback;
		windowClass.hInstance = device->instance;
		windowClass.lpszClassName = "wnd_class";
		const ATOM result = RegisterClassExA(&windowClass);

		const auto viewportX = SCREEN_SIZE_X / 2 - VIEWPORT_SIZE_X / 2;
		const auto viewportY = SCREEN_SIZE_Y / 2 - VIEWPORT_SIZE_Y / 2;

		if (result != 0) {
			device->window = CreateWindowExA(0, windowClass.lpszClassName, title,
											 WS_VISIBLE,
											 viewportX, viewportY,
											 VIEWPORT_SIZE_X, VIEWPORT_SIZE_Y,
											 NULL, NULL, device->instance, NULL);
			SetDefaultWindowStyle(device->window);
		}

		return false;
	}

	void WindowSetFullscreen(logical_device device)
	{
		auto monitor = MonitorFromWindow(device->window, MONITOR_DEFAULTTOPRIMARY);
		MONITORINFO mi = { sizeof(mi) };
		auto monitorInfo = GetMonitorInfoA(monitor, &mi);

		if (GetWindowPlacement(device->window, &device->windowPlacement) && monitorInfo) {
			SetWindowLong(device->window, GWL_STYLE, WS_VISIBLE & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(device->window, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
						 mi.rcMonitor.right - mi.rcMonitor.left,
						 mi.rcMonitor.bottom - mi.rcMonitor.top,
						 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
			s_WindowResized = s_WindowFullscreen == false;
			s_WindowFullscreen = true;
		}
	}

	void WindowSetMinimised(logical_device device)
	{
		SetDefaultWindowStyle(device->window);
		SetWindowPlacement(device->window, &device->windowPlacement);
		s_WindowResized = s_WindowFullscreen == false;
		s_WindowFullscreen = false;
	}

	void WindowClose()
	{
		s_Running = false;
	}

	void DeviceSetHandle(logical_device device, void *handle)
	{
		device->callbackHandle = handle;
	}

	void *DeviceGetHandle(logical_device device)
	{
		return device->callbackHandle;
	}

    bool IsRunning()
	{
		return s_Running;
	}

    bool IsFullscreen()
	{
		return s_WindowFullscreen;
	}

    bool HasResized()
	{
		return s_WindowResized;
	}

	timestep_type GetTimestep(logical_device device)
	{
        return device->dt;
	}

	void DebugBreak()
	{
		::DebugBreak();
	}

	void DebugString(const char* string)
	{
		OutputDebugStringA(string);
	}

	void SurfaceCreate(logical_device device, VkInstance instance, VkSurfaceKHR *pSurface)
	{
		VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hinstance = device->instance;
        createInfo.hwnd = device->window;
        vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, pSurface);
	}

	inline key_code GetKeyCode(WORD param)
	{
		switch (param) {
			case 'A': return key_code::A;
			case 'B': return key_code::B;
			case 'C': return key_code::C;
			case 'D': return key_code::D;
			case 'E': return key_code::E;
			case 'F': return key_code::F;
			case 'G': return key_code::G;
			case 'H': return key_code::H;
			case 'I': return key_code::I;
			case 'J': return key_code::J;
			case 'K': return key_code::K;
			case 'L': return key_code::L;
			case 'M': return key_code::M;
			case 'N': return key_code::N;
			case 'O': return key_code::O;
			case 'P': return key_code::P;
			case 'Q': return key_code::Q;
			case 'R': return key_code::R;
			case 'S': return key_code::S;
			case 'T': return key_code::T;
			case 'U': return key_code::U;
			case 'V': return key_code::V;
			case 'W': return key_code::W;
			case 'X': return key_code::X;
			case 'Y': return key_code::Y;
			case 'Z': return key_code::Z;
			case VK_UP: return key_code::Up;
			case VK_DOWN: return key_code::Down;
			case VK_LEFT: return key_code::Left;
			case VK_RIGHT: return key_code::Right;
			case VK_SPACE: return key_code::Space;
			case VK_ESCAPE: return key_code::Escape;
			case VK_F1: return key_code::F1;
			case VK_F2: return key_code::F2;
			case VK_F3: return key_code::F3;
			case VK_F4: return key_code::F4;
			case VK_F5: return key_code::F5;
			case VK_F6: return key_code::F6;
			case VK_F7: return key_code::F7;
			case VK_F8: return key_code::F8;
			case VK_F9: return key_code::F9;
			case VK_F10: return key_code::F10;
			case VK_F11: return key_code::F11;
			case VK_F12: return key_code::F12;
			case VK_F13: return key_code::F13;
			case VK_F14: return key_code::F14;
			default: break;
		}
		return key_code::Invalid;
	}

	inline modifier GetModifier(LPARAM param)
	{
		const auto word = HIWORD(param);
		const auto altFlag = (word & KF_ALTDOWN) >> 13;
		const auto repeatFlag = (word & KF_REPEAT) >> 13;
		const auto extFlag = (word & KF_EXTENDED) >> 6;
		modifier mod = altFlag | repeatFlag | extFlag;
		return mod;

		/* A simpler explanation of this procedure:

		if((HIWORD(param) & KF_ALTDOWN))
			mod |= MODIFIER_ALT;

		if((HIWORD(param) & KF_REPEAT))
			mod |= MODIFIER_REPEAT;

		if((HIWORD(param) & KF_EXTENDED))
			mod |= MODIFIER_EXTENTED;
		*/
	}

    void EventsPoll(logical_device device)
	{
		s_WindowResized = false;

		LARGE_INTEGER counter;
		QueryPerformanceCounter(&counter);

        device->dt = timestep_type(counter.QuadPart - device->counter.QuadPart);
        device->dt /= timestep_type(device->counterFrequency.QuadPart);
		device->counter = counter;

		MSG event;
		while (PeekMessageA(&event, device->window, 0, 0, PM_REMOVE | PM_QS_INPUT)) {
			switch (event.message) {
				case WM_KEYUP:
				case WM_SYSKEYUP: // call keyup proc
					break;
				case WM_KEYDOWN:
				case WM_SYSKEYDOWN:{
					auto mod = GetModifier(event.lParam);
					device->keyEventCallback(device, GetKeyCode(LOWORD(event.wParam)), mod);
				} break;

				case WM_MOUSEWHEEL:{
					auto wheelDelta = double(GET_WHEEL_DELTA_WPARAM(event.wParam) / WHEEL_DELTA);
					device->scrollWheelCallback(device, 0.0f, wheelDelta);
				} break;
				case WM_MOUSEMOVE: device->mouseMoveCallback(device);
					break;
				case WM_LBUTTONDOWN: device->mouseButtonCallback(device, mouse_button::lmb);
					break;
				case WM_RBUTTONDOWN: device->mouseButtonCallback(device, mouse_button::rmb);
					break;
				case WM_MBUTTONDOWN: device->mouseButtonCallback(device, mouse_button::mmb);
					break;

				default: {
					TranslateMessage(&event);
					DispatchMessage(&event);
				} break;
			}
		}
	}

	void EventsSetKeyDownProc(logical_device device, key_event_callback proc)
    {
		if(proc != nullptr)
        	device->keyEventCallback = proc;
    }

	void EventsSetMouseMoveProc(logical_device device, mouse_move_callback proc)
    {
		if(proc != nullptr)
        	device->mouseMoveCallback = proc;
    }

	void EventsSetMouseDownProc(logical_device device, mouse_button_callback proc)
    {
		if(proc != nullptr)
        	device->mouseButtonCallback = proc;
    }

	void EventsSetScrollWheelProc(logical_device device, scroll_wheel_callback proc)
	{
		if(proc != nullptr)
        	device->scrollWheelCallback = proc;
	}

	inline int GetVirtualKey(key_code key)
	{
		switch (key) {
			case key_code::A: return 'A';
			case key_code::B: return 'B';
			case key_code::C: return 'C';
			case key_code::D: return 'D';
			case key_code::E: return 'E';
			case key_code::F: return 'F';
			case key_code::G: return 'G';
			case key_code::H: return 'H';
			case key_code::I: return 'I';
			case key_code::J: return 'J';
			case key_code::K: return 'K';
			case key_code::L: return 'L';
			case key_code::M: return 'M';
			case key_code::N: return 'N';
			case key_code::O: return 'O';
			case key_code::P: return 'P';
			case key_code::Q: return 'Q';
			case key_code::R: return 'R';
			case key_code::S: return 'S';
			case key_code::T: return 'T';
			case key_code::U: return 'U';
			case key_code::V: return 'V';
			case key_code::W: return 'W';
			case key_code::X: return 'X';
			case key_code::Y: return 'Y';
			case key_code::Z: return 'Z';
			case key_code::Up: return VK_UP;
			case key_code::Down: return VK_DOWN;
			case key_code::Left: return VK_LEFT;
			case key_code::Right: return VK_RIGHT;
			case key_code::Space: return VK_SPACE;
			case key_code::Escape: return VK_ESCAPE;
			case key_code::F1: return VK_F1;
			case key_code::F2: return VK_F2;
			case key_code::F3: return VK_F3;
			case key_code::F4: return VK_F4;
			case key_code::F5: return VK_F5;
			case key_code::F6: return VK_F6;
			case key_code::F7: return VK_F7;
			case key_code::F8: return VK_F8;
			case key_code::F9: return VK_F9;
			case key_code::F10: return VK_F10;
			case key_code::F11: return VK_F11;
			case key_code::F12: return VK_F12;
			case key_code::F13: return VK_F13;
			case key_code::F14: return VK_F14;
			default: break;
		}
		return 0;
	}

	bool IsKeyDown(key_code key)
	{
        const auto vk = GetVirtualKey(key);
		const auto result = GetKeyState(vk) & 0xFF00;
		return bool(result);
	}

	void *MapMemory(size_t size)
	{
		return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	}

	bool UnmapMemory(void *mapped)
	{
		return bool(VirtualFree(mapped, 0, MEM_RELEASE));
	}
}

void KeyEventStub(pltf::logical_device, pltf::key_code, pltf::modifier){}
void MouseMoveStub(pltf::logical_device){}
void MouseButtonStub(pltf::logical_device, pltf::mouse_button){}
void ScrollWheelStub(pltf::logical_device, double x, double y){}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
                   _In_ PSTR lpCmdLine, _In_ INT nCmdShow)
{
	pltf::logical_device_T device;
	device.instance = hInstance;
	device.window = NULL;
	device.windowPlacement = {sizeof(WINDOWPLACEMENT)};

	QueryPerformanceFrequency(&device.counterFrequency);
	QueryPerformanceCounter(&device.counter);

	device.dt = 0.001f;
	device.keyEventCallback = KeyEventStub;
	device.mouseMoveCallback = MouseMoveStub;
	device.mouseButtonCallback = MouseButtonStub;
	device.scrollWheelCallback = ScrollWheelStub;
	device.callbackHandle = nullptr;

	pltf::s_Running = true;

    const auto mainResult = EntryPoint(&device);
    return mainResult;
}

namespace io
{
    struct file
	{
		HANDLE data;
	};

	template<>
	file *Open<cmd::read>(const char *filename)
	{
		auto f = new file;
		f->data = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		return f;
	}

	template<>
	file *Open<cmd::write>(const char *filename)
	{
		auto f = new file;
		f->data = CreateFileA(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		return f;
	}

	template<>
	file *Open<cmd::rw_open>(const char *filename)
	{
		auto f = new file;
		f->data = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		return f;
	}

	template<>
	file *Open<cmd::rw_create>(const char *filename)
	{
		auto f = new file;
		f->data = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		return f;
	}

	bool Close(file *f)
	{
		const bool result = CloseHandle(f->data);
		delete f;
		return result;
	}

	bool Read(file *f, size_t size, void *buffer)
	{
		DWORD bytesRead = 0;
		return ReadFile(f->data, buffer, DWORD(size), &bytesRead, nullptr);
	}

	bool Write(file *f, size_t size, const void *source)
	{
		DWORD bytesWritten = 0;
		return WriteFile(f->data, source, DWORD(size), &bytesWritten, nullptr);
	}

	size_t GetSize(file *f)
	{
		LARGE_INTEGER fileSize;
		GetFileSizeEx(f->data, &fileSize);
		return fileSize.QuadPart;
	}

	bool IsValid(file *f)
	{
		return (f->data != INVALID_HANDLE_VALUE);
	}

	file_map Map(file *f)
	{
		file_map map = {};
		map.size = GetSize(f);
		map.data = VirtualAlloc(nullptr, map.size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

		if(map.data != nullptr)
			Read(f, map.size, map.data);
		else
			map.size = 0;

		return map;
	}

	bool Unmap(file_map &map)
	{
		const auto result = VirtualFree(map.data, 0, MEM_RELEASE);
		map.data = nullptr;
		map.size = 0;
		return result;
	}
}