#ifndef _WIN32

#include "updater_gui.h"
#include "updater_constants.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstring>
#include <cstdio>

namespace hb::updater
{
	struct updater_gui::impl
	{
		Display* display = nullptr;
		Window window = 0;
		GC gc = nullptr;
		Atom wm_delete = 0;
		bool cancelled = false;
		char status_text[256] = "Checking for updates...";
		int progress_permille = 0; // 0..1000

		static void draw(impl* p)
		{
			if (!p || !p->display || !p->window || !p->gc)
				return;

			Display* d = p->display;
			Window w = p->window;
			GC g = p->gc;

			// Clear background
			XSetForeground(d, g, WhitePixel(d, DefaultScreen(d)));
			XFillRectangle(d, w, g, 0, 0, window_width, window_height);

			// Draw status text
			XSetForeground(d, g, BlackPixel(d, DefaultScreen(d)));
			XDrawString(d, w, g, 12, 28, p->status_text,
				static_cast<int>(std::strlen(p->status_text)));

			// Draw progress bar background (gray)
			int bar_x = 12;
			int bar_y = 48;
			int bar_w = window_width - 24;
			int bar_h = 28;

			// Gray background
			XSetForeground(d, g, 0xCCCCCC);
			XFillRectangle(d, w, g, bar_x, bar_y, bar_w, bar_h);

			// Blue fill
			int fill_w = (bar_w * p->progress_permille) / 1000;
			if (fill_w > 0)
			{
				XSetForeground(d, g, 0x3366CC);
				XFillRectangle(d, w, g, bar_x, bar_y, fill_w, bar_h);
			}

			// Border
			XSetForeground(d, g, BlackPixel(d, DefaultScreen(d)));
			XDrawRectangle(d, w, g, bar_x, bar_y, bar_w, bar_h);

			XFlush(d);
		}
	};

	updater_gui::updater_gui() = default;

	updater_gui::~updater_gui()
	{
		destroy();
	}

	bool updater_gui::create()
	{
		m_impl = new impl();

		m_impl->display = XOpenDisplay(nullptr);
		if (!m_impl->display)
		{
			delete m_impl;
			m_impl = nullptr;
			return false;
		}

		int screen = DefaultScreen(m_impl->display);
		int screen_w = XDisplayWidth(m_impl->display, screen);
		int screen_h = XDisplayHeight(m_impl->display, screen);
		int x = (screen_w - window_width) / 2;
		int y = (screen_h - window_height) / 2;

		m_impl->window = XCreateSimpleWindow(
			m_impl->display, RootWindow(m_impl->display, screen),
			x, y, window_width, window_height, 1,
			BlackPixel(m_impl->display, screen),
			WhitePixel(m_impl->display, screen));

		XStoreName(m_impl->display, m_impl->window, window_title);

		// Handle window close
		m_impl->wm_delete = XInternAtom(m_impl->display, "WM_DELETE_WINDOW", False);
		XSetWMProtocols(m_impl->display, m_impl->window, &m_impl->wm_delete, 1);

		XSelectInput(m_impl->display, m_impl->window,
			ExposureMask | StructureNotifyMask);

		m_impl->gc = XCreateGC(m_impl->display, m_impl->window, 0, nullptr);

		XMapWindow(m_impl->display, m_impl->window);
		XFlush(m_impl->display);

		return true;
	}

	void updater_gui::destroy()
	{
		if (m_impl)
		{
			if (m_impl->display)
			{
				if (m_impl->gc)
					XFreeGC(m_impl->display, m_impl->gc);
				if (m_impl->window)
					XDestroyWindow(m_impl->display, m_impl->window);
				XCloseDisplay(m_impl->display);
			}
			delete m_impl;
			m_impl = nullptr;
		}
	}

	void updater_gui::set_status(const char* text)
	{
		if (!m_impl) return;
		std::snprintf(m_impl->status_text, sizeof(m_impl->status_text), "%s", text);
		impl::draw(m_impl);
	}

	void updater_gui::set_progress(float fraction)
	{
		if (!m_impl) return;
		int val = static_cast<int>(fraction * 1000.0f);
		if (val < 0) val = 0;
		if (val > 1000) val = 1000;
		m_impl->progress_permille = val;
		impl::draw(m_impl);
	}

	void updater_gui::pump_messages()
	{
		if (!m_impl || !m_impl->display) return;

		while (XPending(m_impl->display) > 0)
		{
			XEvent ev;
			XNextEvent(m_impl->display, &ev);

			if (ev.type == Expose)
			{
				impl::draw(m_impl);
			}
			else if (ev.type == ClientMessage)
			{
				if (static_cast<Atom>(ev.xclient.data.l[0]) == m_impl->wm_delete)
					m_impl->cancelled = true;
			}
		}
	}

	bool updater_gui::is_cancelled() const
	{
		return m_impl && m_impl->cancelled;
	}

	bool show_server_unreachable_dialog()
	{
		std::fprintf(stderr, "[AutoUpdater] Update server unreachable, continuing with current version.\n");
		return false;
	}
}

#endif // !_WIN32
