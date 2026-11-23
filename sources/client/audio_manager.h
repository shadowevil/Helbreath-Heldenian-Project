#pragma once

#include "miniaudio.h"
#include <string>
#include <cstdint>
#include <vector>
#include <array>
#include <memory>

// Maximum sound effects per category
static const int AUDIO_MAX_CHARACTER_SOUNDS = 25;   // C1-C24
static const int AUDIO_MAX_MONSTER_SOUNDS = 160;    // M1-M156
static const int AUDIO_MAX_EFFECT_SOUNDS = 55;      // E1-E53

// Maximum concurrent playing sounds (active sound pool)
static const int AUDIO_MAX_ACTIVE_SOUNDS = 32;

// Sound types matching existing categories
enum class sound_type
{
	character,  // Combat sounds (C1-C24)
	monster,    // Magic/Monster sounds (M1-M156)
	effect      // Environmental/effect sounds (E1-E53)
};

// Decoded sound data stored in our own memory (bypasses miniaudio resource manager)
struct decoded_sound
{
	void* data = nullptr;
	ma_uint64 frameCount = 0;
	ma_format format = ma_format_f32;
	ma_uint32 channels = 0;
	ma_uint32 sampleRate = 0;
	bool loaded = false;
};

class audio_manager
{
public:
	static audio_manager& get();

	// Lifecycle
	bool initialize();
	void shutdown();

	// Sound loading - pre-loads all sounds into memory
	void load_sounds();
	void unload_sounds();
	void cleanup_finished_sounds();

	// Sound effect playback
	void play_game_sound(sound_type type, int index, int distance = 0, int pan = 0);
	void play_sound_loop(sound_type type, int index);
	void stop_sound(sound_type type, int index);
	void stop_all_sounds();

	// Background music
	void play_music(const char* trackName);
	void stop_music();
	bool is_music_playing() const;
	const std::string& get_current_music_track() const { return m_current_music_track; }

	// Volume control (0-100 scale)
	void set_master_volume(int volume);
	void set_sound_volume(int volume);
	void set_music_volume(int volume);
	void set_ambient_volume(int volume);
	void set_ui_volume(int volume);
	int get_master_volume() const { return m_master_volume; }
	int get_sound_volume() const { return m_sound_volume; }
	int get_music_volume() const { return m_music_volume; }
	int get_ambient_volume() const { return m_ambient_volume; }
	int get_ui_volume() const { return m_ui_volume; }

	// enable/disable
	void set_master_enabled(bool enabled);
	void set_sound_enabled(bool enabled);
	void set_music_enabled(bool enabled);
	void set_ambient_enabled(bool enabled);
	void set_ui_enabled(bool enabled);
	bool is_master_enabled() const { return m_master_enabled; }
	bool is_sound_enabled() const { return m_sound_enabled; }
	bool is_music_enabled() const { return m_music_enabled; }
	bool is_ambient_enabled() const { return m_ambient_enabled; }
	bool is_ui_enabled() const { return m_ui_enabled; }

	// Hardware availability
	bool is_sound_available() const { return m_sound_available; }

	// Listener position (for positional audio)
	void set_listener_position(int worldX, int worldY);
	int get_listener_x() const { return m_listener_x; }
	int get_listener_y() const { return m_listener_y; }

	// Per-frame update
	void update();

private:
	audio_manager() = default;
	~audio_manager() = default;
	audio_manager(const audio_manager&) = delete;
	audio_manager& operator=(const audio_manager&) = delete;

	// Decode a WAV file into a decoded_sound buffer (bypasses resource manager)
	bool decode_file(const char* filePath, decoded_sound& out);
	void free_decoded_sound(decoded_sound& sound);

	// Convert 0-100 volume to 0.0-1.0
	float volume_to_float(int volume) const;

	// get decoded sound data for a type/index
	decoded_sound* get_decoded_sound(sound_type type, int index);

	// get the appropriate sound group for a given sound type/index
	ma_sound_group* get_group_for_sound(sound_type type, int index);

	// Check if the appropriate category is enabled for a given sound
	bool is_category_enabled(sound_type type, int index) const;

	// miniaudio engine
	ma_engine m_engine;
	bool m_sound_available = false;
	bool m_initialized = false;

	// Sound effect groups (for separate volume control per category)
	ma_sound_group m_sfx_group;
	bool m_sfx_group_initialized = false;

	ma_sound_group m_ambient_group;
	bool m_ambient_group_initialized = false;

	ma_sound_group m_ui_group;
	bool m_ui_group_initialized = false;

	// Pre-decoded sound data (our own memory, no resource manager)
	std::array<decoded_sound, AUDIO_MAX_CHARACTER_SOUNDS> m_character_sounds = {};
	std::array<decoded_sound, AUDIO_MAX_MONSTER_SOUNDS> m_monster_sounds = {};
	std::array<decoded_sound, AUDIO_MAX_EFFECT_SOUNDS> m_effect_sounds = {};

	// Active sound pool for concurrent playback
	static const int MAX_INSTANCES_PER_SOUND = 2;

	struct active_sound {
		ma_audio_buffer_ref bufferRef;
		ma_sound sound;
		sound_type type = sound_type::character;
		int index = 0;
		uint32_t startOrder = 0;
		bool inUse = false;
		bool soundInitialized = false;
	};
	std::array<active_sound, AUDIO_MAX_ACTIVE_SOUNDS> m_active_sounds;
	uint32_t m_sound_order = 0;

	// Background music sound (still uses resource manager - single file, streamed)
	ma_sound m_bgm_sound;
	bool m_bgm_loaded = false;

	// Current music track name
	std::string m_current_music_track;

	// Volume (0-100)
	int m_master_volume = 100;
	int m_sound_volume = 100;
	int m_music_volume = 100;
	int m_ambient_volume = 100;
	int m_ui_volume = 100;

	// enable flags
	bool m_master_enabled = true;
	bool m_sound_enabled = true;
	bool m_music_enabled = true;
	bool m_ambient_enabled = true;
	bool m_ui_enabled = true;

	// Listener position (for positional audio)
	int m_listener_x = 0;
	int m_listener_y = 0;
};
