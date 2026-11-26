// display_settings.h: the engine-neutral display model (d-038)
//
// Part of the graphics protocol: the client describes the display it wants with display_settings and hands
// it to IWindow::apply_display_settings; each backend implements the spec its own way. The tables, labels
// and list builders here are the shared half of the spec so every backend offers the same lists.
// Header-only; shared code, so no logging.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace hb::shared::render {

// How the window is presented
namespace window_mode
{
	enum type : std::uint8_t
	{
		windowed,              // a window of the configured size, centred on its monitor
		borderless_fullscreen, // an undecorated window covering the monitor at the desktop's own mode
		exclusive_fullscreen,  // owns the monitor: switches it to the configured size and refresh rate
	};
}

// How the 4:3 logical view fits a non-4:3 output in the fullscreen modes (windowed always fills the window)
namespace aspect_mode
{
	enum type : std::uint8_t
	{
		letterbox, // keep 4:3, bars on the unused dimension
		stretch,   // the world fills the whole output
	};
}

// A monitor mode: size in pixels and refresh rate in Hz (0 when unknown)
struct display_mode
{
	int width = 0;
	int height = 0;
	int refresh_rate = 0;
};

// Everything one apply call sets
struct display_settings
{
	window_mode::type mode = window_mode::windowed;
	int width = 800;                   // windowed and exclusive size; borderless always uses the desktop
	int height = 600;
	int refresh_rate = 60;             // Hz, 0 = unlimited: the frame cap while vsync is off, and in
	                                   // exclusive fullscreen also the rate the monitor is switched to
	bool vsync = false;                // on: frames pace to the monitor's current rate
	aspect_mode::type aspect = aspect_mode::letterbox;
	bool windowed_borderless = false;  // windowed only: no title bar (the old "Window Style: Borderless")

	bool operator==(const display_settings&) const = default;
};

// A resolution the player can pick; label like "1920 x 1080 (16:9)", the desktop's own size marked
struct resolution_option
{
	int width = 0;
	int height = 0;
	bool desktop = false;
	std::string label;
};

// A refresh rate the player can pick; rate 0 is "Unlimited"
struct refresh_option
{
	int rate = 0;
	std::string label;
};

inline constexpr bool is_fullscreen(window_mode::type mode)
{
	return mode != window_mode::windowed;
}

// The letterbox layout applies only in the fullscreen modes; windowed always fills the window
inline constexpr bool uses_letterbox(const display_settings& settings)
{
	return is_fullscreen(settings.mode) && settings.aspect == aspect_mode::letterbox;
}

// ============== Persisted names (settings files) ==============

inline constexpr std::string_view window_mode_key(window_mode::type mode)
{
	switch (mode)
	{
	case window_mode::borderless_fullscreen: return "borderless";
	case window_mode::exclusive_fullscreen: return "exclusive";
	default: return "windowed";
	}
}

inline bool parse_window_mode(std::string_view key, window_mode::type& out)
{
	for (const window_mode::type mode : {window_mode::windowed, window_mode::borderless_fullscreen, window_mode::exclusive_fullscreen})
	{
		if (key == window_mode_key(mode))
		{
			out = mode;
			return true;
		}
	}
	return false;
}

inline constexpr std::string_view aspect_mode_key(aspect_mode::type aspect)
{
	return aspect == aspect_mode::stretch ? "stretch" : "letterbox";
}

inline bool parse_aspect_mode(std::string_view key, aspect_mode::type& out)
{
	if (key == "letterbox" || key == "stretch")
	{
		out = key == "stretch" ? aspect_mode::stretch : aspect_mode::letterbox;
		return true;
	}
	return false;
}

// ============== Curated lists (as Helbreath-SDL) ==============

struct display_size
{
	int width;
	int height;
};

// Offered when they fit between the logical size and the desktop
inline constexpr std::array<display_size, 25> curated_resolutions{{
	// 4:3 (1440x1080 beyond SDL's table: one of this client's earlier five sizes)
	{640, 480}, {800, 600}, {1024, 768}, {1152, 864}, {1280, 960}, {1400, 1050}, {1440, 1080}, {1600, 1200}, {1920, 1440},
	// 16:9
	{1280, 720}, {1366, 768}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3200, 1800}, {3840, 2160},
	// 21:9 and 32:9 ultrawide
	{2560, 1080}, {3440, 1440}, {3840, 1600}, {5120, 2160}, {3840, 1080}, {5120, 1440}, {7680, 2160},
	// 16:10
	{1280, 800}, {1920, 1200},
}};

// The curated render rates; a backend unions in the rates its driver reports
inline constexpr std::array<int, 19> curated_refresh_rates{
	{24, 30, 48, 50, 60, 70, 75, 80, 85, 90, 100, 120, 144, 165, 180, 200, 240, 300, 360}};

// "1920 x 1080 (16:9)": the nearest named ratio within 3%, else the reduced w:h
inline std::string resolution_label(int width, int height, bool desktop = false)
{
	static constexpr std::array<display_size, 6> named{{{4, 3}, {5, 4}, {3, 2}, {16, 10}, {16, 9}, {21, 9}}};
	constexpr double tolerance = 0.03;

	const int divisor = (std::max)(1, std::gcd(width, height));
	std::string tag = std::format("{}:{}", width / divisor, height / divisor);
	if (height > 0)
	{
		const double aspect = static_cast<double>(width) / static_cast<double>(height);
		double best_error = tolerance;
		for (const display_size& ratio : named)
		{
			const double ratio_aspect = static_cast<double>(ratio.width) / static_cast<double>(ratio.height);
			const double error = std::abs(aspect - ratio_aspect) / ratio_aspect;
			if (error <= best_error)
			{
				best_error = error;
				tag = std::format("{}:{}", ratio.width, ratio.height);
			}
		}
	}
	return std::format("{} x {} ({}){}", width, height, tag, desktop ? " - Desktop" : "");
}

inline std::string refresh_label(int rate)
{
	return rate > 0 ? std::format("{} Hz", rate) : std::string("Unlimited");
}

// Curated sizes between the logical size and the desktop, plus the desktop's own size, ascending
inline std::vector<resolution_option> build_resolution_options(const display_mode& desktop, int logical_width, int logical_height)
{
	const int max_w = desktop.width > 0 ? desktop.width : logical_width;
	const int max_h = desktop.height > 0 ? desktop.height : logical_height;

	std::vector<display_size> sizes;
	auto offer = [&sizes](int w, int h) {
		if (std::none_of(sizes.begin(), sizes.end(), [&](const display_size& s) { return s.width == w && s.height == h; }))
			sizes.push_back({w, h});
	};
	for (const display_size& size : curated_resolutions)
	{
		if (size.width >= logical_width && size.height >= logical_height && size.width <= max_w && size.height <= max_h)
			offer(size.width, size.height);
	}
	if (max_w >= logical_width && max_h >= logical_height)
		offer(max_w, max_h);
	else
		offer(logical_width, logical_height);

	std::sort(sizes.begin(), sizes.end(), [](const display_size& a, const display_size& b) {
		return a.width != b.width ? a.width < b.width : a.height < b.height;
	});

	std::vector<resolution_option> out;
	out.reserve(sizes.size());
	for (const display_size& size : sizes)
	{
		const bool is_desktop = size.width == max_w && size.height == max_h;
		out.push_back({size.width, size.height, is_desktop, resolution_label(size.width, size.height, is_desktop)});
	}
	return out;
}

// 0 (Unlimited) first, then the curated rates and the driver's rates (rounded, deduplicated), ascending
inline std::vector<refresh_option> build_refresh_options(const std::vector<double>& driver_rates)
{
	std::vector<int> rates(curated_refresh_rates.begin(), curated_refresh_rates.end());
	for (const double raw : driver_rates)
	{
		// drivers report fractional rates (59.94 for a nominal 60); players pick whole numbers
		const int rate = static_cast<int>(std::lround(raw));
		if (rate > 0 && std::find(rates.begin(), rates.end(), rate) == rates.end())
			rates.push_back(rate);
	}
	std::sort(rates.begin(), rates.end());

	std::vector<refresh_option> out;
	out.reserve(rates.size() + 1);
	out.push_back({0, refresh_label(0)});
	for (const int rate : rates)
		out.push_back({rate, refresh_label(rate)});
	return out;
}

// The option nearest a size (exact first, then the smallest width + height difference)
inline const resolution_option* nearest_resolution(const std::vector<resolution_option>& options, int width, int height)
{
	const resolution_option* best = nullptr;
	int best_diff = 0;
	for (const resolution_option& option : options)
	{
		const int diff = std::abs(option.width - width) + std::abs(option.height - height);
		if (!best || diff < best_diff)
		{
			best = &option;
			best_diff = diff;
		}
	}
	return best;
}

} // namespace hb::shared::render
