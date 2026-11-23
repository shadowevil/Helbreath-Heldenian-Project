#ifdef _WIN32

#include "updater_gui.h"
#include "updater_constants.h"
#include "updater_platform.h"

namespace hb::updater
{
	static constexpr int id_status_label = 100;
	static constexpr int id_progress_bar = 101;

	struct updater_gui::impl
	{
		HWND hwnd = nullptr;
		HWND label = nullptr;
		HWND progress = nullptr;
		bool cancelled = false;

		static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
		{
			if (msg == WM_CREATE)
			{
				auto* cs = reinterpret_cast<CREATESTRUCTA*>(lp);
				SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
				return 0;
			}

			auto* self = reinterpret_cast<impl*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

			if (msg == WM_CLOSE && self)
			{
				self->cancelled = true;
				return 0;
			}

			if (msg == WM_DESTROY)
			{
				PostQuitMessage(0);
				return 0;
			}

			return DefWindowProcA(hwnd, msg, wp, lp);
		}
	};

	updater_gui::updater_gui() = default;

	updater_gui::~updater_gui()
	{
		destroy();
	}

	bool updater_gui::create()
	{
		INITCOMMONCONTROLSEX icc = {};
		icc.dwSize = sizeof(icc);
		icc.dwICC = ICC_PROGRESS_CLASS;
		InitCommonControlsEx(&icc);

		m_impl = new impl();

		WNDCLASSEXA wc = {};
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = impl::wnd_proc;
		wc.hInstance = GetModuleHandleA(nullptr);
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		wc.lpszClassName = "HBUpdaterWnd";
		RegisterClassExA(&wc);

		int screen_w = GetSystemMetrics(SM_CXSCREEN);
		int screen_h = GetSystemMetrics(SM_CYSCREEN);
		int x = (screen_w - window_width) / 2;
		int y = (screen_h - window_height) / 2;

		m_impl->hwnd = CreateWindowExA(
			0, "HBUpdaterWnd", window_title,
			WS_POPUP | WS_CAPTION | WS_SYSMENU,
			x, y, window_width, window_height,
			nullptr, nullptr, GetModuleHandleA(nullptr), m_impl);

		if (!m_impl->hwnd)
			return false;

		// Status label
		m_impl->label = CreateWindowExA(
			0, "STATIC", "Checking for updates...",
			WS_CHILD | WS_VISIBLE | SS_LEFT,
			12, 12, window_width - 24, 24,
			m_impl->hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id_status_label)),
			GetModuleHandleA(nullptr), nullptr);

		// Progress bar
		m_impl->progress = CreateWindowExA(
			0, PROGRESS_CLASSA, nullptr,
			WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
			12, 48, window_width - 24, 28,
			m_impl->hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id_progress_bar)),
			GetModuleHandleA(nullptr), nullptr);

		SendMessageA(m_impl->progress, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
		SendMessageA(m_impl->progress, PBM_SETPOS, 0, 0);

		ShowWindow(m_impl->hwnd, SW_SHOW);
		UpdateWindow(m_impl->hwnd);

		return true;
	}

	void updater_gui::destroy()
	{
		if (m_impl)
		{
			if (m_impl->hwnd)
			{
				DestroyWindow(m_impl->hwnd);
				m_impl->hwnd = nullptr;
			}
			delete m_impl;
			m_impl = nullptr;
		}
	}

	void updater_gui::set_status(const char* text)
	{
		if (m_impl && m_impl->label)
			SetWindowTextA(m_impl->label, text);
	}

	void updater_gui::set_progress(float fraction)
	{
		if (m_impl && m_impl->progress)
		{
			int pos = static_cast<int>(fraction * 1000.0f);
			if (pos < 0) pos = 0;
			if (pos > 1000) pos = 1000;
			SendMessageA(m_impl->progress, PBM_SETPOS, static_cast<WPARAM>(pos), 0);
		}
	}

	void updater_gui::pump_messages()
	{
		MSG msg;
		while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
	}

	bool updater_gui::is_cancelled() const
	{
		return m_impl && m_impl->cancelled;
	}

	bool show_server_unreachable_dialog()
	{
		int result = MessageBoxA(
			nullptr,
			"Could not reach the update server.\n\n"
			"You can retry or continue with the current version.",
			window_title,
			MB_RETRYCANCEL | MB_ICONWARNING | MB_SETFOREGROUND);
		return result == IDRETRY;
	}
}

#endif // _WIN32
