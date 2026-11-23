#include "audio_manager.h"
#include "resolution_config.h"
#include <cstdlib>
#include <format>
#include <string>

audio_manager& audio_manager::get()
{
	static audio_manager instance;
	return instance;
}

bool audio_manager::initialize()
{

	if (m_initialized)
		return m_sound_available;

	// initialize miniaudio engine
	ma_engine_config engineConfig = ma_engine_config_init();

	ma_result result = ma_engine_init(&engineConfig, &m_engine);
	if (result != MA_SUCCESS)
	{
		m_sound_available = false;
		m_initialized = true;
		return false;
	}

	// initialize sound effect group for separate volume control
	result = ma_sound_group_init(&m_engine, 0, NULL, &m_sfx_group);
	if (result == MA_SUCCESS)
	{
		m_sfx_group_initialized = true;
		ma_sound_group_set_volume(&m_sfx_group, volume_to_float(m_sound_volume));
	}

	// initialize ambient sound group
	result = ma_sound_group_init(&m_engine, 0, NULL, &m_ambient_group);
	if (result == MA_SUCCESS)
	{
		m_ambient_group_initialized = true;
		ma_sound_group_set_volume(&m_ambient_group, volume_to_float(m_ambient_volume));
	}

	// initialize UI sound group
	result = ma_sound_group_init(&m_engine, 0, NULL, &m_ui_group);
	if (result == MA_SUCCESS)
	{
		m_ui_group_initialized = true;
		ma_sound_group_set_volume(&m_ui_group, volume_to_float(m_ui_volume));
	}

	// Apply master volume to the engine
	ma_engine_set_volume(&m_engine, volume_to_float(m_master_volume));

	m_sound_available = true;
	m_initialized = true;
	return true;
}

bool audio_manager::decode_file(const char* filePath, decoded_sound& out)
{
	// Decode to f32 at the engine's sample rate so playback doesn't need resampling
	ma_uint32 engineSampleRate = ma_engine_get_sample_rate(&m_engine);
	ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 0, engineSampleRate);
	ma_decoder decoder;

	ma_result result = ma_decoder_init_file(filePath, &decoderConfig, &decoder);
	if (result != MA_SUCCESS)
		return false;

	ma_uint64 totalFrames = 0;
	result = ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
	if (result != MA_SUCCESS || totalFrames == 0)
	{
		ma_decoder_uninit(&decoder);
		return false;
	}

	ma_uint32 bytesPerFrame = ma_get_bytes_per_frame(decoder.outputFormat, decoder.outputChannels);
	size_t dataSize = (size_t)(totalFrames * bytesPerFrame);

	void* data = std::malloc(dataSize);
	if (data == nullptr)
	{
		ma_decoder_uninit(&decoder);
		return false;
	}

	ma_uint64 framesRead = 0;
	result = ma_decoder_read_pcm_frames(&decoder, data, totalFrames, &framesRead);

	out.data = data;
	out.frameCount = framesRead;
	out.format = decoder.outputFormat;
	out.channels = decoder.outputChannels;
	out.sampleRate = decoder.outputSampleRate;
	out.loaded = true;

	ma_decoder_uninit(&decoder);
	return true;
}

void audio_manager::free_decoded_sound(decoded_sound& sound)
{
	if (sound.data != nullptr)
	{
		std::free(sound.data);
		sound.data = nullptr;
	}
	sound.frameCount = 0;
	sound.loaded = false;
}

void audio_manager::load_sounds()
{
	if (!m_sound_available)
		return;

	// load Character sounds (C1-C24)
	for (int i = 1; i < AUDIO_MAX_CHARACTER_SOUNDS; i++)
	{
		auto filename = std::format("sounds/character/c{}.wav", i);
		decode_file(filename.c_str(), m_character_sounds[i]);
	}

	// load Monster/Magic sounds (M1-M156)
	for (int i = 1; i < AUDIO_MAX_MONSTER_SOUNDS; i++)
	{
		auto filename = std::format("sounds/monster/m{}.wav", i);
		decode_file(filename.c_str(), m_monster_sounds[i]);
	}

	// load Effect sounds (E1-E53)
	for (int i = 1; i < AUDIO_MAX_EFFECT_SOUNDS; i++)
	{
		auto filename = std::format("sounds/effects/e{}.wav", i);
		decode_file(filename.c_str(), m_effect_sounds[i]);
	}
}

void audio_manager::shutdown()
{
	if (!m_initialized)
		return;

	// stop and unload music
	stop_music();

	// Unload all sounds
	unload_sounds();

	// Uninitialize sound groups
	if (m_sfx_group_initialized)
	{
		ma_sound_group_uninit(&m_sfx_group);
		m_sfx_group_initialized = false;
	}
	if (m_ambient_group_initialized)
	{
		ma_sound_group_uninit(&m_ambient_group);
		m_ambient_group_initialized = false;
	}
	if (m_ui_group_initialized)
	{
		ma_sound_group_uninit(&m_ui_group);
		m_ui_group_initialized = false;
	}

	// Uninitialize engine
	if (m_sound_available)
	{
		ma_engine_uninit(&m_engine);
	}

	m_initialized = false;
	m_sound_available = false;
}

void audio_manager::unload_sounds()
{
	// stop and uninit active sounds
	for (auto& active : m_active_sounds)
	{
		if (active.inUse)
		{
			if (active.soundInitialized)
			{
				ma_sound_stop(&active.sound);
				ma_sound_uninit(&active.sound);
				active.soundInitialized = false;
			}
			active.inUse = false;
		}
	}

	// Free decoded Character sounds
	for (int i = 0; i < AUDIO_MAX_CHARACTER_SOUNDS; i++)
		free_decoded_sound(m_character_sounds[i]);

	// Free decoded Monster sounds
	for (int i = 0; i < AUDIO_MAX_MONSTER_SOUNDS; i++)
		free_decoded_sound(m_monster_sounds[i]);

	// Free decoded Effect sounds
	for (int i = 0; i < AUDIO_MAX_EFFECT_SOUNDS; i++)
		free_decoded_sound(m_effect_sounds[i]);

	// stop music
	stop_music();
}

void audio_manager::cleanup_finished_sounds()
{
	for (auto& active : m_active_sounds)
	{
		if (active.inUse && active.soundInitialized && !ma_sound_is_playing(&active.sound))
		{
			ma_sound_uninit(&active.sound);
			active.soundInitialized = false;
			active.inUse = false;
		}
	}
}

float audio_manager::volume_to_float(int volume) const
{
	// Convert 0-100 to 0.0-1.0
	if (volume <= 0) return 0.0f;
	if (volume >= 100) return 1.0f;
	return volume / 100.0f;
}

decoded_sound* audio_manager::get_decoded_sound(sound_type type, int index)
{
	switch (type)
	{
	case sound_type::character:
		if (index >= 0 && index < AUDIO_MAX_CHARACTER_SOUNDS && m_character_sounds[index].loaded)
			return &m_character_sounds[index];
		break;
	case sound_type::monster:
		if (index >= 0 && index < AUDIO_MAX_MONSTER_SOUNDS && m_monster_sounds[index].loaded)
			return &m_monster_sounds[index];
		break;
	case sound_type::effect:
		if (index >= 0 && index < AUDIO_MAX_EFFECT_SOUNDS && m_effect_sounds[index].loaded)
			return &m_effect_sounds[index];
		break;
	}
	return nullptr;
}

ma_sound_group* audio_manager::get_group_for_sound(sound_type type, int index)
{
	if (type == sound_type::effect)
	{
		// Ambient: E38 (rain loop)
		if (index == 38)
			return &m_ambient_group;

		// UI: E14 (click), E23 (notification), E24 (transaction), E25 (war notification), E29 (item place), E53 (error)
		if (index == 14 || index == 23 || index == 24 || index == 25 || index == 29 || index == 53)
			return &m_ui_group;
	}

	// Everything else (all C sounds, all M sounds, remaining E sounds) → effects group
	return &m_sfx_group;
}

bool audio_manager::is_category_enabled(sound_type type, int index) const
{
	if (type == sound_type::effect)
	{
		if (index == 38)
			return m_ambient_enabled;

		if (index == 14 || index == 23 || index == 24 || index == 25 || index == 29 || index == 53)
			return m_ui_enabled;
	}

	return m_sound_enabled;
}

void audio_manager::play_game_sound(sound_type type, int index, int distance, int pan)
{
	if (!m_sound_available || !is_category_enabled(type, index))
		return;

	// Clean up finished sounds first
	cleanup_finished_sounds();

	// get the decoded sound data
	decoded_sound* decoded = get_decoded_sound(type, index);
	if (decoded == nullptr)
		return;

	// Calculate volume based on distance
	float volume = 1.0f;

	// Distance attenuation — derive max audible range from resolution
	// Center-to-edge is view_center_tile_x/Y; add a few tiles beyond screen edge
	if (distance > 0)
	{
		const auto& res = hb::shared::render::ResolutionConfig::get();
		int maxDist = std::max(res.view_center_tile_x(), res.view_center_tile_y()) + 4;
		if (maxDist < 10) maxDist = 10;
		if (distance >= maxDist)
			return;
		volume *= (1.0f - (static_cast<float>(distance) / maxDist));
	}

	// Don't play if too quiet
	if (volume < 0.01f)
		return;

	// Limit concurrent instances of the same sound — stop oldest if at cap
	int instanceCount = 0;
	active_sound* oldest = nullptr;
	for (auto& active : m_active_sounds)
	{
		if (active.inUse && active.soundInitialized && active.type == type && active.index == index
			&& ma_sound_is_playing(&active.sound))
		{
			instanceCount++;
			if (oldest == nullptr || active.startOrder < oldest->startOrder)
				oldest = &active;
		}
	}
	if (instanceCount >= MAX_INSTANCES_PER_SOUND && oldest != nullptr)
	{
		ma_sound_stop(&oldest->sound);
		ma_sound_uninit(&oldest->sound);
		oldest->soundInitialized = false;
		oldest->inUse = false;
	}

	// Find a free slot in the active sound pool
	active_sound* slot = nullptr;
	for (auto& active : m_active_sounds)
	{
		if (!active.inUse)
		{
			slot = &active;
			break;
		}
	}

	// No free slot - try to reclaim a finished one
	if (slot == nullptr)
	{
		for (auto& active : m_active_sounds)
		{
			if (active.soundInitialized && !ma_sound_is_playing(&active.sound))
			{
				ma_sound_uninit(&active.sound);
				active.soundInitialized = false;
				active.inUse = false;
				slot = &active;
				break;
			}
		}
	}

	// Still no slot available - skip this sound
	if (slot == nullptr)
		return;

	// Create an audio buffer ref pointing to our decoded data
	ma_result result = ma_audio_buffer_ref_init(decoded->format, decoded->channels, decoded->data, decoded->frameCount, &slot->bufferRef);
	if (result != MA_SUCCESS)
		return;

	// Create a sound from the audio buffer ref (bypasses resource manager entirely)
	ma_sound_group* group = get_group_for_sound(type, index);
	result = ma_sound_init_from_data_source(&m_engine, &slot->bufferRef, MA_SOUND_FLAG_NO_SPATIALIZATION, group, &slot->sound);
	if (result != MA_SUCCESS)
		return;

	slot->inUse = true;
	slot->soundInitialized = true;
	slot->type = type;
	slot->index = index;
	slot->startOrder = ++m_sound_order;

	// Set volume for this instance
	ma_sound_set_volume(&slot->sound, volume);

	// Apply panning (-100 to 100 maps to -1.0 to 1.0)
	if (pan != 0)
	{
		float panValue = pan / 100.0f;
		if (panValue < -1.0f) panValue = -1.0f;
		if (panValue > 1.0f) panValue = 1.0f;
		ma_sound_set_pan(&slot->sound, panValue);
	}

	// start playback
	ma_sound_start(&slot->sound);
}

void audio_manager::play_sound_loop(sound_type type, int index)
{
	if (!m_sound_available || !is_category_enabled(type, index))
		return;

	// get the decoded sound data
	decoded_sound* decoded = get_decoded_sound(type, index);
	if (decoded == nullptr)
		return;

	// Clean up finished sounds first
	cleanup_finished_sounds();

	// Find a free slot
	active_sound* slot = nullptr;
	for (auto& active : m_active_sounds)
	{
		if (!active.inUse)
		{
			slot = &active;
			break;
		}
	}

	if (slot == nullptr)
		return;

	// Create an audio buffer ref pointing to our decoded data
	ma_result result = ma_audio_buffer_ref_init(decoded->format, decoded->channels, decoded->data, decoded->frameCount, &slot->bufferRef);
	if (result != MA_SUCCESS)
		return;

	// Create a sound from the audio buffer ref
	ma_sound_group* group = get_group_for_sound(type, index);
	result = ma_sound_init_from_data_source(&m_engine, &slot->bufferRef, MA_SOUND_FLAG_NO_SPATIALIZATION, group, &slot->sound);
	if (result != MA_SUCCESS)
		return;

	slot->inUse = true;
	slot->soundInitialized = true;
	ma_sound_set_looping(&slot->sound, MA_TRUE);
	ma_sound_start(&slot->sound);
}

void audio_manager::stop_sound(sound_type type, int index)
{
	for (auto& active : m_active_sounds)
	{
		if (active.inUse && active.soundInitialized && active.type == type && active.index == index)
		{
			ma_sound_stop(&active.sound);
			ma_sound_uninit(&active.sound);
			active.soundInitialized = false;
			active.inUse = false;
		}
	}
}

void audio_manager::stop_all_sounds()
{
	// stop and uninit all active sounds
	for (auto& active : m_active_sounds)
	{
		if (active.inUse)
		{
			if (active.soundInitialized)
			{
				ma_sound_stop(&active.sound);
				ma_sound_uninit(&active.sound);
				active.soundInitialized = false;
			}
			active.inUse = false;
		}
	}
}

void audio_manager::play_music(const char* trackName)
{
	if (!m_sound_available)
		return;

	if (trackName == nullptr || trackName[0] == '\0')
		return;

	// Check if already playing this track
	if (m_bgm_loaded && m_current_music_track == trackName)
		return;

	// stop existing music
	stop_music();

	// Don't play if music is disabled
	if (!m_music_enabled)
		return;

	// Build full path
	std::string filename = std::string("music/") + trackName + ".wav";

	// initialize the music sound - stream from disk (music files are large)
	ma_uint32 flags = MA_SOUND_FLAG_STREAM;
	ma_result result = ma_sound_init_from_file(&m_engine, filename.c_str(), flags, NULL, NULL, &m_bgm_sound);

	if (result != MA_SUCCESS)
	{
		m_bgm_loaded = false;
		return;
	}

	m_bgm_loaded = true;
	m_current_music_track = trackName;

	// Set volume and looping
	ma_sound_set_volume(&m_bgm_sound, volume_to_float(m_music_volume));
	ma_sound_set_looping(&m_bgm_sound, MA_TRUE);

	// start playback
	ma_sound_start(&m_bgm_sound);
}

void audio_manager::stop_music()
{
	if (m_bgm_loaded)
	{
		ma_sound_stop(&m_bgm_sound);
		ma_sound_uninit(&m_bgm_sound);
		m_bgm_loaded = false;
	}
	m_current_music_track.clear();
}

bool audio_manager::is_music_playing() const
{
	if (!m_bgm_loaded)
		return false;

	return ma_sound_is_playing(&m_bgm_sound) == MA_TRUE;
}

void audio_manager::set_master_volume(int volume)
{
	if (volume < 0) volume = 0;
	if (volume > 100) volume = 100;
	m_master_volume = volume;

	// Engine volume scales all output (SFX groups + music)
	if (m_initialized && m_sound_available)
	{
		ma_engine_set_volume(&m_engine, m_master_enabled ? volume_to_float(volume) : 0.0f);
	}
}

void audio_manager::set_master_enabled(bool enabled)
{
	m_master_enabled = enabled;

	if (m_initialized && m_sound_available)
	{
		ma_engine_set_volume(&m_engine, enabled ? volume_to_float(m_master_volume) : 0.0f);
	}
}

void audio_manager::set_sound_volume(int volume)
{
	if (volume < 0) volume = 0;
	if (volume > 100) volume = 100;
	m_sound_volume = volume;

	// Set sound group volume (affects all sounds in the group)
	if (m_sfx_group_initialized)
	{
		ma_sound_group_set_volume(&m_sfx_group, volume_to_float(volume));
	}
}

void audio_manager::set_music_volume(int volume)
{
	if (volume < 0) volume = 0;
	if (volume > 100) volume = 100;
	m_music_volume = volume;

	// update currently playing music volume
	if (m_bgm_loaded)
	{
		ma_sound_set_volume(&m_bgm_sound, volume_to_float(volume));
	}
}

void audio_manager::set_sound_enabled(bool enabled)
{
	m_sound_enabled = enabled;

	// stop all sounds if disabling
	if (!enabled)
	{
		stop_all_sounds();
	}
}

void audio_manager::set_music_enabled(bool enabled)
{
	bool wasEnabled = m_music_enabled;
	m_music_enabled = enabled;

	// stop music if disabling
	if (wasEnabled && !enabled)
	{
		stop_music();
	}
}

void audio_manager::set_ambient_volume(int volume)
{
	if (volume < 0) volume = 0;
	if (volume > 100) volume = 100;
	m_ambient_volume = volume;

	if (m_ambient_group_initialized)
	{
		ma_sound_group_set_volume(&m_ambient_group, volume_to_float(volume));
	}
}

void audio_manager::set_ui_volume(int volume)
{
	if (volume < 0) volume = 0;
	if (volume > 100) volume = 100;
	m_ui_volume = volume;

	if (m_ui_group_initialized)
	{
		ma_sound_group_set_volume(&m_ui_group, volume_to_float(volume));
	}
}

void audio_manager::set_ambient_enabled(bool enabled)
{
	m_ambient_enabled = enabled;

	// stop ambient sounds if disabling (stop all and let non-ambient ones replay naturally)
	if (!enabled)
	{
		// Mute the ambient group by setting volume to 0
		if (m_ambient_group_initialized)
		{
			ma_sound_group_set_volume(&m_ambient_group, 0.0f);
		}
	}
	else
	{
		// Restore ambient group volume
		if (m_ambient_group_initialized)
		{
			ma_sound_group_set_volume(&m_ambient_group, volume_to_float(m_ambient_volume));
		}
	}
}

void audio_manager::set_ui_enabled(bool enabled)
{
	m_ui_enabled = enabled;

	if (!enabled)
	{
		if (m_ui_group_initialized)
		{
			ma_sound_group_set_volume(&m_ui_group, 0.0f);
		}
	}
	else
	{
		if (m_ui_group_initialized)
		{
			ma_sound_group_set_volume(&m_ui_group, volume_to_float(m_ui_volume));
		}
	}
}

void audio_manager::set_listener_position(int worldX, int worldY)
{
	m_listener_x = worldX;
	m_listener_y = worldY;
}

void audio_manager::update()
{
	// Periodically clean up finished sounds
	cleanup_finished_sounds();
}
