#pragma once

#include "display_settings.h"
#include <cstdint>

// Maximum shortcuts matching game limits
static const int MAX_SHORTCUTS = 5;
static const int MAX_MAGIC_SLOT = 100;
static const int MAX_SHORTCUT_SLOT = 200;

class config_manager
{
public:
	static config_manager& get();

	// Lifecycle
	void initialize();
	void shutdown();

	// File operations - replaces registry-based read_settings/write_settings
	bool load(const char* filename = "settings.json");
	bool save(const char* filename = "settings.json");
	// False when the file existed but could not be parsed: the values are partial, so start-up must not save
	// (the first later save backs the original up to <file>.bak)
	bool is_loaded() const { return !m_load_failed; }

	// Shortcuts - replaces registry storage
	// Magic shortcut: -1 = none, 0-99 = valid slot
	short get_magic_shortcut() const { return m_magic_shortcut; }
	void set_magic_shortcut(short slot);

	// Item/Skill shortcuts: -1 = none, 0-199 = valid slot
	short get_shortcut(int index) const;
	void set_shortcut(int index, short slot);

	// Recent shortcut (runtime only, not persisted)
	short get_recent_shortcut() const { return m_recent_shortcut; }
	void set_recent_shortcut(short slot) { m_recent_shortcut = slot; }

	// Audio settings - adds persistence to existing runtime values
	int get_master_volume() const { return m_master_volume; }
	int get_sound_volume() const { return m_sound_volume; }
	int get_music_volume() const { return m_music_volume; }
	int get_ambient_volume() const { return m_ambient_volume; }
	int get_ui_volume() const { return m_ui_volume; }
	bool is_master_enabled() const { return m_master_enabled; }
	bool is_sound_enabled() const { return m_sound_enabled; }
	bool is_music_enabled() const { return m_music_enabled; }
	bool is_ambient_enabled() const { return m_ambient_enabled; }
	bool is_ui_enabled() const { return m_ui_enabled; }

	void set_master_volume(int volume);
	void set_sound_volume(int volume);
	void set_music_volume(int volume);
	void set_ambient_volume(int volume);
	void set_ui_volume(int volume);
	void set_master_enabled(bool enabled);
	void set_sound_enabled(bool enabled);
	void set_music_enabled(bool enabled);
	void set_ambient_enabled(bool enabled);
	void set_ui_enabled(bool enabled);

	// The display model (window mode, resolution, refresh rate, vsync, aspect, windowed decoration).
	// The window applies it (IWindow::apply_display_settings); this only persists it.
	const hb::shared::render::display_settings& get_display_settings() const { return m_display; }
	void set_display_settings(const hb::shared::render::display_settings& settings);

	// Display/Detail settings
	bool is_show_fps_enabled() const { return m_show_fps; }
	bool is_show_latency_enabled() const { return m_show_latency; }
	int get_detail_level() const { return m_detail_level; }
	bool is_dialog_transparency_enabled() const { return m_dialog_trans; }
	bool is_running_mode_enabled() const { return m_running_mode; }

	void set_show_fps_enabled(bool enabled);
	void set_show_latency_enabled(bool enabled);
	void set_detail_level(int level);
	void set_dialog_transparency_enabled(bool enabled);
	void set_running_mode_enabled(bool enabled);

	// Mouse capture
	bool is_mouse_capture_enabled() const { return m_capture_mouse; }
	void set_mouse_capture_enabled(bool enabled);

	// Tile grid overlay (simple dark lines)
	bool is_tile_grid_enabled() const { return m_tile_grid; }
	void set_tile_grid_enabled(bool enabled);

	// Patching grid overlay (debug with zone colors)
	bool is_patching_grid_enabled() const { return m_patching_grid; }
	void set_patching_grid_enabled(bool enabled);

	// Background FPS throttle: reduce rendering when window loses focus
	bool is_background_fps_throttle_enabled() const { return m_background_fps_throttle; }
	void set_background_fps_throttle_enabled(bool enabled);

	// Smooth upscale filtering (true = bilinear/smooth, false = nearest/crisp)
	bool is_upscale_smoothing_enabled() const { return m_upscale_smoothing; }
	void set_upscale_smoothing_enabled(bool enabled);

	// TTF font smoothing/antialiasing
	bool is_font_smoothing_enabled() const { return m_font_smoothing; }
	void set_font_smoothing_enabled(bool enabled);

	// Reduced motion - disables camera shake effects
	bool is_reduced_motion_enabled() const { return m_reduced_motion; }
	void set_reduced_motion_enabled(bool enabled);

	// Toggle to Chat - when ON (default), Enter required to start chat; when OFF, any printable key activates chat
	bool is_toggle_to_chat_enabled() const { return m_toggle_to_chat; }
	void set_toggle_to_chat_enabled(bool enabled);

	// Minimap zoom level (0=overview, 1-3=progressively closer)
	int get_minimap_zoom_level() const { return m_minimap_zoom_level; }
	void set_minimap_zoom_level(int level);

	// Quick Actions - always enabled (pickup during movement, 95% unlock, responsive stops)
	bool is_quick_actions_enabled() const { return true; }

	// Dirty flag - indicates unsaved changes
	bool is_dirty() const { return m_dirty; }
	void mark_clean() { m_dirty = false; }

private:
	config_manager() = default;
	~config_manager() = default;
	config_manager(const config_manager&) = delete;
	config_manager& operator=(const config_manager&) = delete;

	void set_defaults();
	int clamp(int value, int min, int max);

	// Shortcuts (matches m_magic_short_cut, m_short_cut[5], m_recent_short_cut)
	short m_magic_shortcut;
	short m_shortcuts[MAX_SHORTCUTS];
	short m_recent_shortcut;

	// Audio (matches m_cSoundVolume, m_cMusicVolume, m_bSoundStat, m_bMusicStat)
	int m_master_volume;
	int m_sound_volume;
	int m_music_volume;
	int m_ambient_volume;
	int m_ui_volume;
	bool m_master_enabled;
	bool m_sound_enabled;
	bool m_music_enabled;
	bool m_ambient_enabled;
	bool m_ui_enabled;

	// Display model
	hb::shared::render::display_settings m_display;

	// Display/Detail
	bool m_show_fps;
	bool m_show_latency;
	int m_detail_level;
	bool m_dialog_trans;
	bool m_running_mode;
	bool m_capture_mouse;
	bool m_background_fps_throttle;
	bool m_upscale_smoothing;
	bool m_font_smoothing;
	bool m_tile_grid;
	bool m_patching_grid;
	bool m_reduced_motion;
	bool m_toggle_to_chat;
	int m_minimap_zoom_level;

	// State
	bool m_dirty;
	bool m_initialized;
	bool m_load_failed = false;      // the file existed but did not parse
	bool m_backup_pending = false;   // copy it to <file>.bak before the first save overwrites it
};
