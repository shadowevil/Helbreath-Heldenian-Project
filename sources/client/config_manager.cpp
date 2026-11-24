#include "config_manager.h"
#include "global_def.h"
#include "json.hpp"
#include "minimap.h"
#include <fstream>
#include <cstring>
#include <cstdlib>

using json = nlohmann::json;

// 4:3 resolutions for validation (must match DialogBox_SysMenu)
static constexpr int minimap_zoom_max = hb::client::minimap::tier_count - 1; // tiers: 0 whole map, 1 mid

static const struct { int w, h; } s_ValidResolutions[] = {
	//{ 640, 480 },
	{ 800, 600 },
	{ 1024, 768 },
	{ 1280, 960 },
	{ 1440, 1080 },
	{ 1920, 1440 }
};
static const int s_NumValidResolutions = sizeof(s_ValidResolutions) / sizeof(s_ValidResolutions[0]);

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

	// hb::shared::render::Window defaults
	m_window_width = 800;
	m_window_height = 600;

	// Display/Detail defaults
	m_show_fps = false;
	m_show_latency = false;
	m_detail_level = 2;
	m_dialog_trans = false;
	m_running_mode = false;
#ifdef DEF_WINDOWED_MODE
	m_fullscreen = false;
#else
	m_fullscreen = true;
#endif
	m_capture_mouse = true;
	m_borderless = true;
	m_vsync = false;
	m_fps_limit = 60;
	m_background_fps_throttle = true;
	m_fullscreen_stretch = false;
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
				m_window_width = clamp(window["width"].get<int>(), 640, 3840);
			}
			if (window.contains("height"))
			{
				m_window_height = clamp(window["height"].get<int>(), 480, 2160);
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
			if (display.contains("fullscreen"))
			{
				m_fullscreen = display["fullscreen"].get<bool>();
			}
			if (display.contains("captureMouse"))
			{
				m_capture_mouse = display["captureMouse"].get<bool>();
			}
			if (display.contains("borderless"))
			{
				m_borderless = display["borderless"].get<bool>();
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
				m_vsync = display["vsync"].get<bool>();
			}
			if (display.contains("fpsLimit"))
			{
				m_fps_limit = display["fpsLimit"].get<int>();
				if (m_fps_limit < 0) m_fps_limit = 0;
			}
			if (display.contains("backgroundFpsThrottle"))
			{
				m_background_fps_throttle = display["backgroundFpsThrottle"].get<bool>();
			}
			if (display.contains("fullscreenStretch"))
			{
				m_fullscreen_stretch = display["fullscreenStretch"].get<bool>();
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

		// Validate resolution to nearest 4:3 option
		bool valid_resolution = false;
		for (int i = 0; i < s_NumValidResolutions; i++) {
			if (m_window_width == s_ValidResolutions[i].w && m_window_height == s_ValidResolutions[i].h) {
				valid_resolution = true;
				break;
			}
		}
		if (!valid_resolution) {
			// Find nearest 4:3 resolution
			int bestIndex = 0;
			int bestDiff = abs(s_ValidResolutions[0].w - m_window_width) + abs(s_ValidResolutions[0].h - m_window_height);
			for (int i = 1; i < s_NumValidResolutions; i++) {
				int diff = abs(s_ValidResolutions[i].w - m_window_width) + abs(s_ValidResolutions[i].h - m_window_height);
				if (diff < bestDiff) {
					bestDiff = diff;
					bestIndex = i;
				}
			}
			m_window_width = s_ValidResolutions[bestIndex].w;
			m_window_height = s_ValidResolutions[bestIndex].h;
			m_dirty = true; // Mark dirty so corrected value is saved
		}
	}
	catch (const json::exception&)
	{
		// Parse error - keep defaults
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
	j["window"]["width"] = m_window_width;
	j["window"]["height"] = m_window_height;

	// Display/Detail settings
	j["display"]["showFps"] = m_show_fps;
	j["display"]["showLatency"] = m_show_latency;
	j["display"]["detailLevel"] = m_detail_level;
	j["display"]["dialogTransparency"] = m_dialog_trans;
	j["display"]["runningMode"] = m_running_mode;
	j["display"]["fullscreen"] = m_fullscreen;
	j["display"]["captureMouse"] = m_capture_mouse;
	j["display"]["borderless"] = m_borderless;
	j["display"]["tileGrid"] = m_tile_grid;
	j["display"]["patchingGrid"] = m_patching_grid;
	j["display"]["vsync"] = m_vsync;
	j["display"]["fpsLimit"] = m_fps_limit;
	j["display"]["backgroundFpsThrottle"] = m_background_fps_throttle;
	j["display"]["fullscreenStretch"] = m_fullscreen_stretch;
	j["display"]["upscaleSmoothing"] = m_upscale_smoothing;
	j["display"]["fontSmoothing"] = m_font_smoothing;
	j["display"]["reducedMotion"] = m_reduced_motion;
	j["display"]["toggleToChat"] = m_toggle_to_chat;
	j["display"]["minimapZoomLevel"] = m_minimap_zoom_level;

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

void config_manager::set_window_size(int width, int height)
{
	// Validate to nearest 4:3 resolution
	bool valid = false;
	for (int i = 0; i < s_NumValidResolutions; i++) {
		if (width == s_ValidResolutions[i].w && height == s_ValidResolutions[i].h) {
			valid = true;
			break;
		}
	}
	if (!valid) {
		// Snap to nearest valid resolution
		int bestIndex = 0;
		int bestDiff = abs(s_ValidResolutions[0].w - width) + abs(s_ValidResolutions[0].h - height);
		for (int i = 1; i < s_NumValidResolutions; i++) {
			int diff = abs(s_ValidResolutions[i].w - width) + abs(s_ValidResolutions[i].h - height);
			if (diff < bestDiff) {
				bestDiff = diff;
				bestIndex = i;
			}
		}
		width = s_ValidResolutions[bestIndex].w;
		height = s_ValidResolutions[bestIndex].h;
	}

	if (m_window_width != width || m_window_height != height)
	{
		m_window_width = width;
		m_window_height = height;
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

void config_manager::set_fullscreen_enabled(bool enabled)
{
	if (m_fullscreen != enabled)
	{
		m_fullscreen = enabled;
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

void config_manager::set_borderless_enabled(bool enabled)
{
	if (m_borderless != enabled)
	{
		m_borderless = enabled;
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

void config_manager::set_vsync_enabled(bool enabled)
{
	if (m_vsync != enabled)
	{
		m_vsync = enabled;
		m_dirty = true;
		save();
	}
}

void config_manager::set_fps_limit(int limit)
{
	if (limit < 0) limit = 0;
	if (m_fps_limit != limit)
	{
		m_fps_limit = limit;
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

void config_manager::set_fullscreen_stretch_enabled(bool enabled)
{
	if (m_fullscreen_stretch != enabled)
	{
		m_fullscreen_stretch = enabled;
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
