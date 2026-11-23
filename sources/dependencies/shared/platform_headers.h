// platform_headers.h: Centralized OS-specific header management
//
// Include this instead of <windows.h> directly. This header:
// 1. Configures Windows headers to minimize macro pollution
// 2. Includes the necessary OS headers
// 3. Undefines all Win32 macros that collide with game code
// 4. Defines platform-neutral native type aliases (NativeWindowHandle, NativeInstance)
// 5. Provides cross-platform wrappers in hb::platform namespace
//
// On non-Windows platforms, provides stub/fallback implementations.
//////////////////////////////////////////////////////////////////////

#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

// Include winsock2 before windows.h to prevent winsock1 inclusion.
// ASIO requires winsock2 and will error if winsock1 was loaded first.
#include <winsock2.h>
#include <windows.h>
#include <mmsystem.h>

// Undefine Win32 macros that collide with game code identifiers.
// windows.h rewrites these to ...A/...W suffixed versions, breaking
// any class methods or functions with the same name.
#undef GetMessage
#undef SendMessage
#undef PostMessage
#undef DrawText
#undef CreateFont
#undef PlaySound
#undef CreateFile
#undef DeleteFile
#undef GetObject
#undef LoadImage
#undef GetClassName
#undef CreateWindow
#undef GetCommandLine
#undef draw_text

#endif // _WIN32

#include <cstdio>

//////////////////////////////////////////////////////////////////////
// Platform-neutral native type aliases.
// On Windows these resolve to the real Win32 types (zero casts needed).
//////////////////////////////////////////////////////////////////////

namespace hb::shared::types {

#ifdef _WIN32
using NativeWindowHandle = HWND;
using NativeInstance = HINSTANCE;
#elif defined(__linux__)
using NativeWindowHandle = unsigned long;  // X11 Window
using NativeInstance = void*;
#elif defined(__APPLE__)
using NativeWindowHandle = void*;  // NSWindow*
using NativeInstance = void*;
#else
using NativeWindowHandle = void*;
using NativeInstance = void*;
#endif

} // namespace hb::shared::types

//////////////////////////////////////////////////////////////////////
// hb::platform — Cross-platform wrappers for OS-level operations.
//
// Engine implementations (SFMLWindow, etc.) call these instead of
// Win32 APIs directly. On Linux, stubs return safe defaults.
//////////////////////////////////////////////////////////////////////

namespace hb::platform
{

using handle = hb::shared::types::NativeWindowHandle;

// --- Screen queries ---

inline int get_screen_width()
{
#ifdef _WIN32
	return GetSystemMetrics(SM_CXSCREEN);
#else
	return 0;
#endif
}

inline int get_screen_height()
{
#ifdef _WIN32
	return GetSystemMetrics(SM_CYSCREEN);
#else
	return 0;
#endif
}

struct monitor_rect
{
	int x, y, width, height;
};

inline monitor_rect get_current_monitor_work_area(handle hwnd)
{
#ifdef _WIN32
	HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO info = {};
	info.cbSize = sizeof(info);
	if (GetMonitorInfo(monitor, &info))
	{
		return {
			info.rcWork.left,
			info.rcWork.top,
			info.rcWork.right - info.rcWork.left,
			info.rcWork.bottom - info.rcWork.top
		};
	}
#else
	(void)hwnd;
#endif
	return { 0, 0, 0, 0 };
}

inline monitor_rect get_current_monitor_size(handle hwnd)
{
#ifdef _WIN32
	HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO info = {};
	info.cbSize = sizeof(info);
	if (GetMonitorInfo(monitor, &info))
	{
		return {
			info.rcMonitor.left,
			info.rcMonitor.top,
			info.rcMonitor.right - info.rcMonitor.left,
			info.rcMonitor.bottom - info.rcMonitor.top
		};
	}
#else
	(void)hwnd;
#endif
	return { 0, 0, 0, 0 };
}

inline int get_monitor_refresh_rate()
{
#ifdef _WIN32
	DEVMODE dev_mode = {};
	dev_mode.dmSize = sizeof(dev_mode);
	if (EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &dev_mode))
	{
		if (dev_mode.dmDisplayFrequency > 0)
			return static_cast<int>(dev_mode.dmDisplayFrequency);
	}
#endif
	return 60;
}

// --- Timer resolution ---

inline void set_timer_resolution(unsigned int ms)
{
#ifdef _WIN32
	timeBeginPeriod(ms);
#else
	(void)ms;
#endif
}

inline void restore_timer_resolution(unsigned int ms)
{
#ifdef _WIN32
	timeEndPeriod(ms);
#else
	(void)ms;
#endif
}

// --- Window management ---

inline void add_minimize_button(handle hwnd)
{
#ifdef _WIN32
	LONG style = GetWindowLong(hwnd, GWL_STYLE);
	style |= WS_MINIMIZEBOX;
	SetWindowLong(hwnd, GWL_STYLE, style);
	SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
#else
	(void)hwnd;
#endif
}

inline void position_window(handle hwnd, int x, int y, int client_w, int client_h)
{
#ifdef _WIN32
	RECT rect = { 0, 0, client_w, client_h };
	DWORD style = static_cast<DWORD>(GetWindowLong(hwnd, GWL_STYLE));
	DWORD ex_style = static_cast<DWORD>(GetWindowLong(hwnd, GWL_EXSTYLE));
	AdjustWindowRectEx(&rect, style, FALSE, ex_style);
	SetWindowPos(hwnd, HWND_TOP, x, y,
		rect.right - rect.left, rect.bottom - rect.top, SWP_SHOWWINDOW);
#else
	(void)hwnd; (void)x; (void)y; (void)client_w; (void)client_h;
#endif
}

inline void resize_window(handle hwnd, int client_w, int client_h)
{
#ifdef _WIN32
	RECT rect = { 0, 0, client_w, client_h };
	DWORD style = static_cast<DWORD>(GetWindowLong(hwnd, GWL_STYLE));
	DWORD ex_style = static_cast<DWORD>(GetWindowLong(hwnd, GWL_EXSTYLE));
	AdjustWindowRectEx(&rect, style, FALSE, ex_style);
	SetWindowPos(hwnd, HWND_TOP, 0, 0,
		rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_SHOWWINDOW);
#else
	(void)hwnd; (void)client_w; (void)client_h;
#endif
}

inline void focus_window(handle hwnd)
{
#ifdef _WIN32
	SetForegroundWindow(hwnd);
	SetFocus(hwnd);
#else
	(void)hwnd;
#endif
}

inline bool get_client_size(handle hwnd, int& width, int& height)
{
#ifdef _WIN32
	RECT client_rect;
	if (GetClientRect(hwnd, &client_rect))
	{
		width = client_rect.right - client_rect.left;
		height = client_rect.bottom - client_rect.top;
		return true;
	}
#else
	(void)hwnd;
#endif
	width = 0;
	height = 0;
	return false;
}

inline void show_error_dialog(handle hwnd, const char* title, const char* message)
{
#ifdef _WIN32
	MessageBoxA(hwnd, message, title, MB_ICONEXCLAMATION | MB_OK);
#else
	(void)hwnd;
	std::fprintf(stderr, "%s: %s\n", title, message);
#endif
}

} // namespace hb::platform
