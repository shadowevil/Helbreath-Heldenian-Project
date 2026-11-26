#include "config_manager.h"
#include "global_def.h"
#include "json.hpp"
#include "minimap.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <cstring>
#include <cstdlib>
#include <string>

using json = nlohmann::json;

static constexpr int minimap_zoom_max = hb::client::minimap::tier_count - 1; // tiers: 0 whole map, 1 mid

// Sanity bounds for a stored size; start-up snaps it to the window's resolution list (capped at the desktop)
static constexpr int max_window_width = 16384;
static constexpr int max_window_height = 16384;

namespace render = hb::shared::render;

config_manager& config_manager::get()
{
	static config_manager instance;
	return instance;
}

void config_manager::initialize()
{
	// Only initialize once - don't reset if already initialized
	if (m_initialized)
		return;

	set_defaults();
	m_initialized = true;
	m_dirty = false;
}

void config_manager::shutdown()
{
	// Auto-save if dirty
	if (m_dirty)
	{
		save();
	}
	m_initialized = false;
}

void config_manager::set_defaults()
{
	// Shortcut defaults (none assigned)
	m_magic_shortcut = -1;
	m_recent_shortcut = -1;
	for (int i = 0; i < MAX_SHORTCUTS; i++)
	{
		m_shortcuts[i] = -1;
	}

	// Audio defaults
	m_master_volume = 100;
	m_sound_volume = 100;
	m_music_volume = 100;
	m_ambient_volume = 100;
	m_ui_volume = 100;
	m_master_enabled = true;
	m_sound_enabled = true;
	m_music_enabled = true;
	m_ambient_enabled = true;
	m_ui_enabled = true;

	// Display model defaults
	m_display = {};
	m_display.width = 800;
	m_display.height = 600;
#ifdef DEF_WINDOWED_MODE
	m_display.mode = render::window_mode::windowed;
#else
	m_display.mode = render::window_mode::borderless_fullscreen;
#endif
	m_display.windowed_borderless = true;
	m_display.vsync = false;
	m_display.refresh_rate = 60;
	m_display.aspect = render::aspect_mode::letterbox;
	m_last_fullscreen_mode = render::window_mode::borderless_fullscreen;

	// Display/Detail defaults
	m_show_fps = false;
	m_show_latency = false;
	m_detail_level = 2;
	m_dialog_trans = false;
	m_running_mode = false;
	m_capture_mouse = true;
	m_background_fps_throttle = true;
	m_upscale_smoothing = true;
	m_font_smoothing = false;
	m_tile_grid = false;     // Simple tile grid off by default
	m_patching_grid = false; // Patching debug grid off by default
	m_reduced_motion = false;
	m_toggle_to_chat = true; // ON = current behavior (Enter required to chat)
	m_minimap_zoom_level = 1; // Mid tier by default

	m_dirty = false;
}

int config_manager::clamp(int value, int min, int max)
{
	if (value < min) return min;
	if (value > max) return max;
	return value;
}

bool config_manager::load(const char* filename)
{
	std::ifstream file(filename);
	if (!file.is_open())
	{
		// No config file - save defaults and return success
		save(filename);
		return true;
	}

	try
	{
		json j = json::parse(file);

		// Shortcuts
		if (j.contains("shortcuts"))
		{
			auto& shortcuts = j["shortcuts"];
			if (shortcuts.contains("magic"))
			{
				int slot = shortcuts["magic"].get<int>();
				m_magic_shortcut = (slot >= 0 && slot < MAX_MAGIC_SLOT) ? static_cast<short>(slot) : -1;
			}
			if (shortcuts.contains("slots") && shortcuts["slots"].is_array())
			{
				auto& slots = shortcuts["slots"];
				for (size_t i = 0; i < slots.size() && i < MAX_SHORTCUTS; i++)
				{
					int slot = slots[i].get<int>();
					m_shortcuts[i] = (slot >= 0 && slot < MAX_SHORTCUT_SLOT) ? static_cast<short>(slot) : -1;
				}
			}
		}

		// Audio settings
		if (j.contains("audio"))
		{
			auto& audio = j["audio"];
			if (audio.contains("masterVolume"))
			{
				m_master_volume = clamp(audio["masterVolume"].get<int>(), 0, 100);
			}
			if (audio.contains("soundVolume"))
			{
				m_sound_volume = clamp(audio["soundVolume"].get<int>(), 0, 100);
			}
			if (audio.contains("musicVolume"))
			{
				m_music_volume = clamp(audio["musicVolume"].get<int>(), 0, 100);
			}
			if (audio.contains("ambientVolume"))
			{
				m_ambient_volume = clamp(audio["ambientVolume"].get<int>(), 0, 100);
			}
			if (audio.contains("uiVolume"))
			{
				m_ui_volume = clamp(audio["uiVolume"].get<int>(), 0, 100);
			}
			if (audio.contains("masterEnabled"))
			{
				m_master_enabled = audio["masterEnabled"].get<bool>();
			}
			if (audio.contains("soundEnabled"))
			{
				m_sound_enabled = audio["soundEnabled"].get<bool>();
			}
			if (audio.contains("musicEnabled"))
			{
				m_music_enabled = audio["musicEnabled"].get<bool>();
			}
			if (audio.contains("ambientEnabled"))
			{
				m_ambient_enabled = audio["ambientEnabled"].get<bool>();
			}
			if (audio.contains("uiEnabled"))
			{
				m_ui_enabled = audio["uiEnabled"].get<bool>();
			}
		}

		// hb::shared::render::Window settings
		if (j.contains("window"))
		{
			auto& window = j["window"];
			if (window.contains("width"))
			{
				m_display.width = clamp(window["width"].get<int>(), 1, max_window_width);
			}
			if (window.contains("height"))
			{
				m_display.height = clamp(window["height"].get<int>(), 1, max_window_height);
			}
		}

		// Display/Detail settings
		if (j.contains("display"))
		{
			auto& display = j["display"];
			if (display.contains("showFps"))
			{
				m_show_fps = display["showFps"].get<bool>();
			}
			if (display.contains("showLatency"))
			{
				m_show_latency = display["showLatency"].get<bool>();
			}
			if (display.contains("detailLevel"))
			{
				m_detail_level = clamp(display["detailLevel"].get<int>(), 0, 2);
			}
			if (display.contains("dialogTransparency"))
			{
				m_dialog_trans = display["dialogTransparency"].get<bool>();
			}
			if (display.contains("runningMode"))
			{
				m_running_mode = display["runningMode"].get<bool>();
			}
			// Window mode: one string; older files had two booleans. "fullscreen" showed the desktop size,
			// so it becomes borderless fullscreen (no monitor mode switch, the windowed size is kept);
			// "borderless" was the windowed decoration and becomes windowedBorderless
			if (display.contains("windowMode") && display["windowMode"].is_string())
			{
				render::parse_window_mode(display["windowMode"].get<std::string>(), m_display.mode);
			}
			else if (display.contains("fullscreen"))
			{
				m_display.mode = display["fullscreen"].get<bool>() ? render::window_mode::borderless_fullscreen
					: render::window_mode::windowed;
			}
			// The mode Alt+Enter returns to; a fullscreen window mode in the file counts as the last one used
			if (render::is_fullscreen(m_display.mode))
			{
				m_last_fullscreen_mode = m_display.mode;
			}
			if (display.contains("lastFullscreenMode") && display["lastFullscreenMode"].is_string())
			{
				render::window_mode::type last = m_last_fullscreen_mode;
				if (render::parse_window_mode(display["lastFullscreenMode"].get<std::string>(), last) && render::is_fullscreen(last))
					m_last_fullscreen_mode = last;
			}
			if (display.contains("windowedBorderless"))
			{
				m_display.windowed_borderless = display["windowedBorderless"].get<bool>();
			}
			else if (display.contains("borderless"))
			{
				m_display.windowed_borderless = display["borderless"].get<bool>();
			}
			if (display.contains("captureMouse"))
			{
				m_capture_mouse = display["captureMouse"].get<bool>();
			}
			if (display.contains("tileGrid"))
			{
				m_tile_grid = display["tileGrid"].get<bool>();
			}
			if (display.contains("patchingGrid"))
			{
				m_patching_grid = display["patchingGrid"].get<bool>();
			}
			if (display.contains("vsync"))
			{
				m_display.vsync = display["vsync"].get<bool>();
			}
			// Refresh rate (0 = unlimited); older files had the frame cap as fpsLimit
			if (display.contains("refreshRate"))
			{
				m_display.refresh_rate = (std::max)(display["refreshRate"].get<int>(), 0);
			}
			else if (display.contains("fpsLimit"))
			{
				m_display.refresh_rate = (std::max)(display["fpsLimit"].get<int>(), 0);
			}
			if (display.contains("backgroundFpsThrottle"))
			{
				m_background_fps_throttle = display["backgroundFpsThrottle"].get<bool>();
			}
			// Aspect: "letterbox" / "stretch"; older files had fullscreenStretch
			if (display.contains("aspect") && display["aspect"].is_string())
			{
				render::parse_aspect_mode(display["aspect"].get<std::string>(), m_display.aspect);
			}
			else if (display.contains("fullscreenStretch"))
			{
				m_display.aspect = display["fullscreenStretch"].get<bool>() ? render::aspect_mode::stretch
					: render::aspect_mode::letterbox;
			}
			if (display.contains("upscaleSmoothing"))
			{
				m_upscale_smoothing = display["upscaleSmoothing"].get<bool>();
			}
			if (display.contains("fontSmoothing"))
			{
				m_font_smoothing = display["fontSmoothing"].get<bool>();
			}
			if (display.contains("reducedMotion"))
			{
				m_reduced_motion = display["reducedMotion"].get<bool>();
			}
			if (display.contains("toggleToChat"))
			{
				m_toggle_to_chat = display["toggleToChat"].get<bool>();
			}
			if (display.contains("minimapZoomLevel"))
			{
				m_minimap_zoom_level = clamp(display["minimapZoomLevel"].get<int>(), 0, minimap_zoom_max);
			}
		}
	}
	catch (const json::exception&)
	{
		// Parse error - keep defaults (values read so far stay); nothing is saved now, and the first
		// later save keeps a copy of the file the player had
		m_load_failed = true;
		m_backup_pending = true;
		return false;
	}

	// save immediately to persist any defaults for new keys or corrected values
	m_dirty = true;
	save();
	return true;
}

bool config_manager::save(const char* filename)
{
	json j;

	// Shortcuts
	j["shortcuts"]["magic"] = m_magic_shortcut;
	j["shortcuts"]["slots"] = json::array();
	for (int i = 0; i < MAX_SHORTCUTS; i++)
	{
		j["shortcuts"]["slots"].push_back(m_shortcuts[i]);
	}

	// Audio settings
	j["audio"]["masterVolume"] = m_master_volume;
	j["audio"]["soundVolume"] = m_sound_volume;
	j["audio"]["musicVolume"] = m_music_volume;
	j["audio"]["ambientVolume"] = m_ambient_volume;
	j["audio"]["uiVolume"] = m_ui_volume;
	j["audio"]["masterEnabled"] = m_master_enabled;
	j["audio"]["soundEnabled"] = m_sound_enabled;
	j["audio"]["musicEnabled"] = m_music_enabled;
	j["audio"]["ambientEnabled"] = m_ambient_enabled;
	j["audio"]["uiEnabled"] = m_ui_enabled;

	// hb::shared::render::Window settings
	j["window"]["width"] = m_display.width;
	j["window"]["height"] = m_display.height;

	// Display/Detail settings
	j["display"]["showFps"] = m_show_fps;
	j["display"]["showLatency"] = m_show_latency;
	j["display"]["detailLevel"] = m_detail_level;
	j["display"]["dialogTransparency"] = m_dialog_trans;
	j["display"]["runningMode"] = m_running_mode;
	j["display"]["windowMode"] = std::string(render::window_mode_key(m_display.mode));
	j["display"]["lastFullscreenMode"] = std::string(render::window_mode_key(m_last_fullscreen_mode));
	j["display"]["windowedBorderless"] = m_display.windowed_borderless;
	j["display"]["captureMouse"] = m_capture_mouse;
	j["display"]["tileGrid"] = m_tile_grid;
	j["display"]["patchingGrid"] = m_patching_grid;
	j["display"]["vsync"] = m_display.vsync;
	j["display"]["refreshRate"] = m_display.refresh_rate;
	j["display"]["backgroundFpsThrottle"] = m_background_fps_throttle;
	j["display"]["aspect"] = std::string(render::aspect_mode_key(m_display.aspect));
	j["display"]["upscaleSmoothing"] = m_upscale_smoothing;
	j["display"]["fontSmoothing"] = m_font_smoothing;
	j["display"]["reducedMotion"] = m_reduced_motion;
	j["display"]["toggleToChat"] = m_toggle_to_chat;
	j["display"]["minimapZoomLevel"] = m_minimap_zoom_level;

	if (m_backup_pending)
	{
		// The file did not parse at load: keep the player's original once before overwriting it
		std::error_code ec;
		std::filesystem::copy_file(filename, std::string(filename) + ".bak",
			std::filesystem::copy_options::overwrite_existing, ec);
		if (ec)
			return false;  // no copy, no overwrite
		m_backup_pending = false;
	}

	std::ofstream file(filename);
	if (!file.is_open())
	{
		return false;
	}

	file << j.dump(4); // Pretty print with 4-space indent
	m_dirty = false;
	return true;
}

void config_manager::set_magic_shortcut(short slot)
{
	if (slot >= -1 && slot < MAX_MAGIC_SLOT)
	{
		if (m_magic_shortcut != slot)
		{
			m_magic_shortcut = slot;
			m_dirty = true;
		}
	}
}

short config_manager::get_shortcut(int index) const
{
	if (index >= 0 && index < MAX_SHORTCUTS)
	{
		return m_shortcuts[index];
	}
	return -1;
}

void config_manager::set_shortcut(int index, short slot)
{
	if (index >= 0 && index < MAX_SHORTCUTS)
	{
		if (slot >= -1 && slot < MAX_SHORTCUT_SLOT)
		{
			if (m_shortcuts[index] != slot)
			{
				m_shortcuts[index] = slot;
				m_dirty = true;
			}
		}
	}
}

void config_manager::set_master_volume(int volume)
{
	volume = clamp(volume, 0, 100);
	if (m_master_volume != volume)
	{
		m_master_volume = volume;
		m_dirty = true;
		save();
	}
}

void config_manager::set_master_enabled(bool enabled)
{
	if (m_master_enabled != enabled)
	{
		m_master_enabled = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_sound_volume(int volume)
{
	volume = clamp(volume, 0, 100);
	if (m_sound_volume != volume)
	{
		m_sound_volume = volume;
		m_dirty = true;
		save();
	}
}

void config_manager::set_music_volume(int volume)
{
	volume = clamp(volume, 0, 100);
	if (m_music_volume != volume)
	{
		m_music_volume = volume;
		m_dirty = true;
		save();
	}
}

void config_manager::set_sound_enabled(bool enabled)
{
	if (m_sound_enabled != enabled)
	{
		m_sound_enabled = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_music_enabled(bool enabled)
{
	if (m_music_enabled != enabled)
	{
		m_music_enabled = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_ambient_volume(int volume)
{
	volume = clamp(volume, 0, 100);
	if (m_ambient_volume != volume)
	{
		m_ambient_volume = volume;
		m_dirty = true;
		save();
	}
}

void config_manager::set_ui_volume(int volume)
{
	volume = clamp(volume, 0, 100);
	if (m_ui_volume != volume)
	{
		m_ui_volume = volume;
		m_dirty = true;
		save();
	}
}

void config_manager::set_ambient_enabled(bool enabled)
{
	if (m_ambient_enabled != enabled)
	{
		m_ambient_enabled = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_ui_enabled(bool enabled)
{
	if (m_ui_enabled != enabled)
	{
		m_ui_enabled = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_display_settings(const render::display_settings& settings)
{
	render::display_settings next = settings;
	next.width = clamp(next.width, 1, max_window_width);
	next.height = clamp(next.height, 1, max_window_height);
	next.refresh_rate = (std::max)(next.refresh_rate, 0);
	if (render::is_fullscreen(next.mode) && m_last_fullscreen_mode != next.mode)
	{
		m_last_fullscreen_mode = next.mode;
		m_dirty = true;
	}
	if (m_display != next || m_dirty)
	{
		m_display = next;
		m_dirty = true;
		save();
	}
}

void config_manager::set_show_fps_enabled(bool enabled)
{
	if (m_show_fps != enabled)
	{
		m_show_fps = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_show_latency_enabled(bool enabled)
{
	if (m_show_latency != enabled)
	{
		m_show_latency = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_detail_level(int level)
{
	level = clamp(level, 0, 2);
	if (m_detail_level != level)
	{
		m_detail_level = level;
		m_dirty = true;
		save();
	}
}

void config_manager::set_dialog_transparency_enabled(bool enabled)
{
	if (m_dialog_trans != enabled)
	{
		m_dialog_trans = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_running_mode_enabled(bool enabled)
{
	if (m_running_mode != enabled)
	{
		m_running_mode = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_mouse_capture_enabled(bool enabled)
{
	if (m_capture_mouse != enabled)
	{
		m_capture_mouse = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_tile_grid_enabled(bool enabled)
{
	if (m_tile_grid != enabled)
	{
		m_tile_grid = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_patching_grid_enabled(bool enabled)
{
	if (m_patching_grid != enabled)
	{
		m_patching_grid = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_background_fps_throttle_enabled(bool enabled)
{
	if (m_background_fps_throttle != enabled)
	{
		m_background_fps_throttle = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_upscale_smoothing_enabled(bool enabled)
{
	if (m_upscale_smoothing != enabled)
	{
		m_upscale_smoothing = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_font_smoothing_enabled(bool enabled)
{
	if (m_font_smoothing != enabled)
	{
		m_font_smoothing = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_reduced_motion_enabled(bool enabled)
{
	if (m_reduced_motion != enabled)
	{
		m_reduced_motion = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_toggle_to_chat_enabled(bool enabled)
{
	if (m_toggle_to_chat != enabled)
	{
		m_toggle_to_chat = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_minimap_zoom_level(int level)
{
	level = clamp(level, 0, minimap_zoom_max);
	if (m_minimap_zoom_level != level)
	{
		m_minimap_zoom_level = level;
		m_dirty = true;
		save();
	}
}
