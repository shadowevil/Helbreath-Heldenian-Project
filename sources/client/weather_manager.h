#pragma once

#include <cstdint>
#include <array>
#include "game_constants.h"

// Forward declarations only — full headers in .cpp to avoid GlobalDef.h/RenderConstants.h ordering
namespace hb::shared::render { class IRenderer; }
namespace hb::shared::sprite { class SpriteCollection; }
class CCamera;
class CMapData;

struct weather_particle
{
	short x = 0;
	short y = 0;
	short bx = 0;
	char step = 0;
};

class weather_manager
{
public:
	static weather_manager& get();

	// Lifecycle
	void initialize();
	void shutdown();

	// Core API
	void draw();
	void update(uint32_t current_time);
	void set_weather(bool start, char effect_type);
	void draw_thunder_effect(int sX, int sY, int dX, int dY, int rX, int rY, char type);
	void reset_particles();

	// Dependencies (call from Screen_Loading after loading effect sprites)
	void set_dependencies(hb::shared::render::IRenderer& renderer,
	                     hb::shared::sprite::SpriteCollection& effect_sprites,
	                     CCamera& camera);
	void set_map_data(CMapData* map_data);
	void set_xmas(bool is_xmas) { m_is_xmas = is_xmas; }
	bool is_xmas() const { return m_is_xmas; }

	// Accessors
	char get_effect_type() const { return m_effect_type; }
	char get_weather_status() const { return m_weather_status; }
	void set_weather_status(char status) { m_weather_status = status; }
	bool is_active() const { return m_is_active; }
	bool is_raining() const { return m_effect_type >= 1 && m_effect_type <= 3; }
	bool is_snowing() const { return m_effect_type >= 4; }

	// Ambient light (day/night)
	void set_ambient_light(char level);
	char get_ambient_light() const { return m_ambient_light_level; }
	bool is_night() const { return m_ambient_light_level == 2; }

private:
	weather_manager() = default;
	~weather_manager() = default;
	weather_manager(const weather_manager&) = delete;
	weather_manager& operator=(const weather_manager&) = delete;

	// Particle state
	std::array<weather_particle, game_limits::max_weather_objects> m_particles{};

	// Weather state
	bool m_is_active = false;
	char m_effect_type = 0;
	char m_weather_status = 0;
	uint32_t m_last_update_time = 0;
	bool m_is_xmas = false;
	char m_ambient_light_level = 1;
	int m_xmas_snow_count = 0;

	// Dependencies (non-owning)
	hb::shared::render::IRenderer* m_renderer = nullptr;
	hb::shared::sprite::SpriteCollection* m_effect_sprites = nullptr;
	CCamera* m_camera = nullptr;
	CMapData* m_map_data = nullptr;
};
