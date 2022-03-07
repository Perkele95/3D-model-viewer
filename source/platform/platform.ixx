module;

#include <vulkan/vulkan.h>

#if defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h>
	#include <vulkan/vulkan_win32.h>
#else
#endif

export module Platform;
// TODO(arle): make this automatic based on screen size
const auto SCREEN_SIZE_X = 1920;
const auto SCREEN_SIZE_Y = 1080;
const auto VIEWPORT_SIZE_X = 1280;
const auto VIEWPORT_SIZE_Y = 720;

namespace pltf
{
	struct logical_device_T;
	using logical_device = logical_device_T*;
	using timestep_type = float;
}

// User-defined entrypoint declaration
void PlatformEntryPoint(pltf::logical_device device);

namespace pltf
{
	enum class key_code : uint16_t
	{
		Invalid = 0,

		// From glfw3.h
		Space = 32,
		Apostrophe = 39, /* ' */
		Comma = 44, /* , */
		Minus = 45, /* - */
		Period = 46, /* . */
		Slash = 47, /* / */

		D0 = 48, /* 0 */
		D1 = 49, /* 1 */
		D2 = 50, /* 2 */
		D3 = 51, /* 3 */
		D4 = 52, /* 4 */
		D5 = 53, /* 5 */
		D6 = 54, /* 6 */
		D7 = 55, /* 7 */
		D8 = 56, /* 8 */
		D9 = 57, /* 9 */

		Semicolon = 59, /* ; */
		Equal = 61, /* = */

		A = 65,
		B = 66,
		C = 67,
		D = 68,
		E = 69,
		F = 70,
		G = 71,
		H = 72,
		I = 73,
		J = 74,
		K = 75,
		L = 76,
		M = 77,
		N = 78,
		O = 79,
		P = 80,
		Q = 81,
		R = 82,
		S = 83,
		T = 84,
		U = 85,
		V = 86,
		W = 87,
		X = 88,
		Y = 89,
		Z = 90,

		LeftBracket = 91,  /* [ */
		Backslash = 92,  /* \ */
		RightBracket = 93,  /* ] */
		GraveAccent = 96,  /* ` */

		World1 = 161, /* non-US #1 */
		World2 = 162, /* non-US #2 */

		/* Function keys */
		Escape = 256,
		Enter = 257,
		Tab = 258,
		Backspace = 259,
		Insert = 260,
		Delete = 261,
		Right = 262,
		Left = 263,
		Down = 264,
		Up = 265,
		PageUp = 266,
		PageDown = 267,
		Home = 268,
		End = 269,
		CapsLock = 280,
		ScrollLock = 281,
		NumLock = 282,
		PrintScreen = 283,
		Pause = 284,
		F1 = 290,
		F2 = 291,
		F3 = 292,
		F4 = 293,
		F5 = 294,
		F6 = 295,
		F7 = 296,
		F8 = 297,
		F9 = 298,
		F10 = 299,
		F11 = 300,
		F12 = 301,
		F13 = 302,
		F14 = 303,
		F15 = 304,
		F16 = 305,
		F17 = 306,
		F18 = 307,
		F19 = 308,
		F20 = 309,
		F21 = 310,
		F22 = 311,
		F23 = 312,
		F24 = 313,
		F25 = 314,

		/* Keypad */
		KP0 = 320,
		KP1 = 321,
		KP2 = 322,
		KP3 = 323,
		KP4 = 324,
		KP5 = 325,
		KP6 = 326,
		KP7 = 327,
		KP8 = 328,
		KP9 = 329,
		KPDecimal = 330,
		KPDivide = 331,
		KPMultiply = 332,
		KPSubtract = 333,
		KPAdd = 334,
		KPEnter = 335,
		KPEqual = 336,

		LeftShift = 340,
		LeftControl = 341,
		LeftAlt = 342,
		LeftSuper = 343,
		RightShift = 344,
		RightControl = 345,
		RightAlt = 346,
		RightSuper = 347,
		Menu = 348
	};

	enum class mouse_button
	{
		lmb,
		rmb,
		mmb
	};

	enum MODIFIER_BITS
	{
		MODIFIER_CTRL,
		MODIFIER_SHIFT,
		MODIFIER_ALT
	};

	using modifier = uint32_t;

	using key_event_callback = void(*)(logical_device, key_code, modifier);
	using mouse_move_callback = void(*)(logical_device);// TODO(arle): UNFINISHED
	using mouse_button_callback = void(*)(logical_device, mouse_button);

	static bool s_Running = false;
	static bool s_WindowResized = false;
	static bool s_WindowFullscreen = false;

	// Main

	export bool WindowCreate(logical_device device, const char *title);
	export void WindowSetFullscreen(logical_device device);
	export void WindowSetMinimised(logical_device device);
	export void WindowClose();

	// Timestep

	export timestep_type GetTimestep(logical_device device);

	// Surface

	export void MakeSurface(logical_device device, VkInstance instance, VkSurfaceKHR *pSurface);

	// Events

	void KeyEventDownStub(logical_device device, key_code key, modifier mod) {}
	void MouseEventMoveStub(logical_device device) {}
	void MouseEventDownStub(logical_device device, mouse_button btn) {}

	static key_event_callback s_KeyEventDown = KeyEventDownStub;
	static mouse_move_callback s_MouseEventMove = MouseEventMoveStub;
	static mouse_button_callback s_MouseEventDown = MouseEventDownStub;

	export void EventsPoll(logical_device device);
	export void EventsSetKeyDownProc(key_event_callback proc);
	export void EventsSetMouseMoveProc(mouse_move_callback proc);
	export void EventsSetMouseDownProc(mouse_button_callback proc);

	// Input state

	export bool InputIsKeyDown(key_code key);
	export void InputMousePosition(); //TODO(arle)

#if defined(_WIN32)

	struct logical_device_T
	{
		HINSTANCE instance;
		HWND window;
		WINDOWPLACEMENT windowPlacement;
		int64_t perfCounter;
		int64_t perfCountFrequency;
	};

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
		s_WindowFullscreen = true;
	}

	void WindowClose()
	{
		s_Running = false;
	}

	timestep_type GetTimestep(logical_device device)
	{
		LARGE_INTEGER counter;
		QueryPerformanceCounter(&counter);

		auto dt = timestep_type(counter.QuadPart - device->perfCounter);
		dt /= timestep_type(device->perfCountFrequency);
		device->perfCounter = counter.QuadPart;
		return dt;
	}

	void MakeSurface(logical_device device, VkInstance instance, VkSurfaceKHR* pSurface)
	{
		VkWin32SurfaceCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		createInfo.hinstance = device->instance;
		createInfo.hwnd = device->window;
		vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, pSurface);
	}

	inline key_code GetKeyCode(WPARAM param)
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

	void EventsPoll(logical_device device)
	{
		MSG event;
		while (PeekMessageA(&event, device->window, 0, 0, PM_REMOVE | PM_QS_INPUT)) {
			switch (event.message) {
				case WM_KEYUP:
				case WM_SYSKEYUP: // call keyup proc
					break;
				case WM_KEYDOWN:
				case WM_SYSKEYDOWN:
					//
					s_KeyEventDown(device, GetKeyCode(event.wParam), 0);
					break;
				case WM_MOUSEWHEEL: // call mousewheel proc with value
					break;
				case WM_MOUSEMOVE: s_MouseEventMove(device);
					break;
				case WM_LBUTTONDOWN: s_MouseEventDown(device, mouse_button::lmb);
					break;
				case WM_RBUTTONDOWN: s_MouseEventDown(device, mouse_button::rmb);
					break;
				case WM_MBUTTONDOWN: s_MouseEventDown(device, mouse_button::mmb);
					break;

				default: {
					TranslateMessage(&event);
					DispatchMessage(&event);
				} break;
			}
		}
	}

	void EventsSetKeyDownProc(key_event_callback proc)
	{
		s_KeyEventDown = proc;
	}

	void EventsSetMouseMoveProc(mouse_move_callback proc)
	{
		s_MouseEventMove = proc;
	}

	void EventsSetMouseDownProc(mouse_button_callback proc)
	{
		s_MouseEventDown = proc;
	}

	bool InputIsKeyDown(key_code key)
	{
		// TODO(arle): map from key_code -> win32 virtual key
		const auto result = GetKeyState(int(key)) & 0xFF00;
		return bool(result);
	}

#else

	static_assert("Target platform not supported!");

#endif
}

namespace io // NOTE(arle): consider moving into io.ixx
{
	struct file;

	struct file_view
	{
		const void *data;
		size_t size;
	};

	enum class cmd
	{
		read,
		write,
		rw_open,
		rw_create
	};

	export inline file *Open(const char *filename, cmd command);
	export inline bool Close(file *f);
	export inline bool Read(file *f, size_t size, void *buffer);
	export inline bool Write(file *f, size_t size, const void *source);
	export inline size_t GetSize(file *p);

	// TODO(arle): readentirefile
	// TODO(arle): writeentirefile
	// TODO(arle): closefileview

#if defined(_WIN32)

	struct file
	{
		HANDLE data;
	};

	file *Open(const char* filename, cmd command)
	{
		auto f = new file;
		switch (command)
		{
			case io::cmd::read:
				f->data = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
				break;
			case io::cmd::write:
				f->data = CreateFileA(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
				break;
			case io::cmd::rw_open:
				f->data = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
				break;
			case io::cmd::rw_create:
				f->data = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
				break;
		}
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

#else
#endif
}