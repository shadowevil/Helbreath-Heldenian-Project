// map_data.h must come before weather_manager.h — it pulls in global_def.h which
// sets GLOBALDEF_H_RESOLUTION_FUNCTIONS, preventing render_constants.h from
// redefining LOGICAL_WIDTH/HEIGHT when renderer.h is included below.
#include "map_data.h"
#include "weather_manager.h"
#include "renderer.h"
#include "sprite_collection.h"
#include "camera.h"
#include "audio_manager.h"
#include "misc.h"
#include "common_types.h"

#include <cstdlib>

weather_manager& weather_manager::get()
{
	static weather_manager instance;
	return instance;
}

void weather_manager::initialize()
{
	m_is_active = false;
	m_effect_type = 0;
	m_weather_status = 0;
	m_last_update_time = 0;
	m_is_xmas = false;
	m_ambient_light_level = 1;
	reset_particles();
}

void weather_manager::shutdown()
{
	m_renderer = nullptr;
	m_effect_sprites = nullptr;
	m_camera = nullptr;
	m_map_data = nullptr;
}

void weather_manager::set_dependencies(hb::shared::render::IRenderer& renderer,
                                     hb::shared::sprite::SpriteCollection& effect_sprites,
                                     CCamera& camera)
{
	m_renderer = &renderer;
	m_effect_sprites = &effect_sprites;
	m_camera = &camera;
}

void weather_manager::set_map_data(CMapData* map_data)
{
	m_map_data = map_data;
}

void weather_manager::reset_particles()
{
	for (auto& p : m_particles)
	{
		p.x = 0;
		p.bx = 0;
		p.y = 0;
		p.step = 0;
	}
}

void weather_manager::draw()
{
	if (!m_effect_sprites || !m_camera) return;

	constexpr int MaxSnowAccum = 1000;
	static int ix1[MaxSnowAccum];
	static int iy2[MaxSnowAccum];
	static int frame[MaxSnowAccum];
	int i;
	short dX, dY, cnt;
	char temp_frame;

	switch (m_effect_type) {
	case 1:
	case 2:
	case 3: // rain
		switch (m_effect_type) {
		case 1: cnt = game_limits::max_weather_objects / 5; break;
		case 2:	cnt = game_limits::max_weather_objects / 2; break;
		case 3:	cnt = game_limits::max_weather_objects;     break;
		}

		for (i = 0; i < cnt; i++)
		{
			if ((m_particles[i].step >= 0) && (m_particles[i].step < 20) && (m_particles[i].x != 0))
			{
				dX = m_particles[i].x - m_camera->get_x();
				dY = m_particles[i].y - m_camera->get_y();
				temp_frame = 16 + (m_particles[i].step / 6);
				(*m_effect_sprites)[11]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());
			}
			else if ((m_particles[i].step >= 20) && (m_particles[i].step < 25) && (m_particles[i].x != 0))
			{
				dX = m_particles[i].x - m_camera->get_x();
				dY = m_particles[i].y - m_camera->get_y();
				(*m_effect_sprites)[11]->draw(dX, dY, m_particles[i].step, hb::shared::sprite::DrawParams::additive_no_color_key());
			}
		}
		break;

	case 4:
	case 5:
	case 6: // Snow
		switch (m_effect_type) {
		case 4: cnt = game_limits::max_weather_objects / 5; break;
		case 5:	cnt = game_limits::max_weather_objects / 2; break;
		case 6:	cnt = game_limits::max_weather_objects;     break;
		}
		for (i = 0; i < cnt; i++)
		{
			if ((m_particles[i].step >= 0) && (m_particles[i].step < 80))
			{
				dX = m_particles[i].x - m_camera->get_x();
				dY = m_particles[i].y - m_camera->get_y();

				// Snoopy: Snow on lower bar
				if (dY >= LOGICAL_HEIGHT() - 20)
				{
					temp_frame = 39 + (m_particles[i].step / 20) * 3;
					dX = m_particles[i].bx;
					dY = LOGICAL_HEIGHT() - 54;
				}
				else temp_frame = 39 + (m_particles[i].step / 20) * 3 + (rand() % 3);

				(*m_effect_sprites)[11]->draw(dX, dY, temp_frame, hb::shared::sprite::DrawParams::additive_no_color_key());

				if (m_is_xmas == true)
				{
					if (dY == LOGICAL_HEIGHT() - 55)
					{
						ix1[m_xmas_snow_count] = dX;
						iy2[m_xmas_snow_count] = dY + (rand() % 5);
						frame[m_xmas_snow_count] = temp_frame;
						m_xmas_snow_count++;
					}
					if (m_xmas_snow_count >= MaxSnowAccum) m_xmas_snow_count = 0;
				}
			}
		}
		if (m_is_xmas == true)
		{
			for (i = 0; i < MaxSnowAccum; i++)
			{
				if (iy2[i] > 10) (*m_effect_sprites)[11]->draw(ix1[i], iy2[i], frame[i], hb::shared::sprite::DrawParams::additive_no_color_key());
			}
		}
		break;
	}
}

void weather_manager::update(uint32_t current_time)
{
	if (!m_map_data || !m_camera) return;

	int i;
	short cnt;
	char  add;

	if ((current_time - m_last_update_time) < 30) return;
	m_last_update_time = current_time;

	switch (m_effect_type) {
	case 1:
	case 2:
	case 3: // Rain
		switch (m_effect_type) {
		case 1: cnt = game_limits::max_weather_objects / 5; break;
		case 2:	cnt = game_limits::max_weather_objects / 2; break;
		case 3:	cnt = game_limits::max_weather_objects;     break;
		}
		for (i = 0; i < cnt; i++)
		{
			m_particles[i].step++;
			if ((m_particles[i].step >= 0) && (m_particles[i].step < 20))
			{
				add = (40 - m_particles[i].step);
				if (add < 0) add = 0;
				m_particles[i].y = m_particles[i].y + add;
				if (add != 0)
					m_particles[i].x = m_particles[i].x - 1;
			}
			else if (m_particles[i].step >= 25)
			{
				if (m_is_active == false)
				{
					m_particles[i].x = 0;
					m_particles[i].y = 0;
					m_particles[i].step = 30;
				}
				else
				{
					m_particles[i].x = (m_map_data->m_pivot_x * 32) + ((rand() % 940) - 200) + 300;
					m_particles[i].y = (m_map_data->m_pivot_y * 32) + ((rand() % LOGICAL_WIDTH()) - LOGICAL_HEIGHT()) + 240;
					m_particles[i].step = -1 * (rand() % 10);
				}
			}
		}
		break;

	case 4:
	case 5:
	case 6:
		switch (m_effect_type) {
		case 4: cnt = game_limits::max_weather_objects / 5; break;
		case 5:	cnt = game_limits::max_weather_objects / 2; break;
		case 6:	cnt = game_limits::max_weather_objects;     break;
		}
		for (i = 0; i < cnt; i++)
		{
			m_particles[i].step++;
			if ((m_particles[i].step >= 0) && (m_particles[i].step < 80))
			{
				add = (80 - m_particles[i].step) / 10;
				if (add < 0) add = 0;
				m_particles[i].y = m_particles[i].y + add;

				//Snoopy: Snow on lower bar
				if (m_particles[i].y > (LOGICAL_HEIGHT() - 54 + m_camera->get_y()))
				{
					m_particles[i].y = LOGICAL_HEIGHT() - 10 + m_camera->get_y();
					if ((rand() % 10) != 2) m_particles[i].step--;
					if (m_particles[i].bx == 0) m_particles[i].bx = m_particles[i].x - m_camera->get_x();

				}
				else m_particles[i].x += 1 - (rand() % 3);
			}
			else if (m_particles[i].step >= 80)
			{
				if (m_is_active == false)
				{
					m_particles[i].x = 0;
					m_particles[i].y = 0;
					m_particles[i].bx = 0;
					m_particles[i].step = 80;
				}
				else
				{
					m_particles[i].x = (m_map_data->m_pivot_x * 32) + ((rand() % 940) - 200) + 300;
					m_particles[i].y = (m_map_data->m_pivot_y * 32) + ((rand() % LOGICAL_WIDTH()) - LOGICAL_HEIGHT()) + LOGICAL_HEIGHT();
					m_particles[i].step = -1 * (rand() % 10);
					m_particles[i].bx = 0;
				}
			}
		}
		break;
	}
}

void weather_manager::set_ambient_light(char level)
{
	m_ambient_light_level = level;
	if (m_renderer)
		m_renderer->set_ambient_light_level(level);
}

void weather_manager::set_weather(bool start, char effect_type)
{
	// Always stop weather sounds first when changing weather
	audio_manager::get().stop_sound(sound_type::effect, 38);

	if (start == true)
	{
		m_is_active = true;
		m_effect_type = effect_type;

		// Rain sound (types 1-3)
		if (audio_manager::get().is_sound_enabled() && (effect_type >= 1) && (effect_type <= 3))
			audio_manager::get().play_sound_loop(sound_type::effect, 38);

		for (auto& p : m_particles)
		{
			p.x = 1;
			p.bx = 1;
			p.y = 1;
			p.step = -1 * (rand() % 40);
		}
	}
	else
	{
		m_is_active = false;
		m_effect_type = 0;
	}

	// reset Xmas snow accumulation state so stale data doesn't persist across weather changes
	m_xmas_snow_count = 0;
}

void weather_manager::draw_thunder_effect(int sX, int sY, int dX, int dY, int rX, int rY, char type)
{
	if (!m_renderer || !m_effect_sprites) return;

	int j, err, prev_x1, prev_y1, x1, y1, tX, tY;
	direction dir;
	prev_x1 = x1 = tX = sX;
	prev_y1 = y1 = tY = sY;

	for (j = 0; j < 100; j++)
	{
		switch (type) {
		case 1:
			m_renderer->draw_line(prev_x1, prev_y1, x1, y1, hb::shared::render::Color(15, 15, 20), hb::shared::render::BlendMode::Additive);
			m_renderer->draw_line(prev_x1 - 1, prev_y1, x1 - 1, y1, GameColors::NightBlueMid, hb::shared::render::BlendMode::Additive);
			m_renderer->draw_line(prev_x1 + 1, prev_y1, x1 + 1, y1, GameColors::NightBlueMid, hb::shared::render::BlendMode::Additive);
			m_renderer->draw_line(prev_x1, prev_y1 - 1, x1, y1 - 1, GameColors::NightBlueMid, hb::shared::render::BlendMode::Additive);
			m_renderer->draw_line(prev_x1, prev_y1 + 1, x1, y1 + 1, GameColors::NightBlueMid, hb::shared::render::BlendMode::Additive);

			m_renderer->draw_line(prev_x1 - 2, prev_y1, x1 - 2, y1, GameColors::NightBlueDark, hb::shared::render::BlendMode::Additive);
			m_renderer->draw_line(prev_x1 + 2, prev_y1, x1 + 2, y1, GameColors::NightBlueDark, hb::shared::render::BlendMode::Additive);
			m_renderer->draw_line(prev_x1, prev_y1 - 2, x1, y1 - 2, GameColors::NightBlueDark, hb::shared::render::BlendMode::Additive);
			m_renderer->draw_line(prev_x1, prev_y1 + 2, x1, y1 + 2, GameColors::NightBlueDark, hb::shared::render::BlendMode::Additive);

			m_renderer->draw_line(prev_x1 - 1, prev_y1 - 1, x1 - 1, y1 - 1, GameColors::NightBlueDeep, hb::shared::render::BlendMode::Additive);
			m_renderer->draw_line(prev_x1 + 1, prev_y1 - 1, x1 + 1, y1 - 1, GameColors::NightBlueDeep, hb::shared::render::BlendMode::Additive);
			m_renderer->draw_line(prev_x1 + 1, prev_y1 - 1, x1 + 1, y1 - 1, GameColors::NightBlueDeep, hb::shared::render::BlendMode::Additive);
			m_renderer->draw_line(prev_x1 - 1, prev_y1 + 1, x1 - 1, y1 + 1, GameColors::NightBlueDeep, hb::shared::render::BlendMode::Additive);
			break;

		case 2:
			m_renderer->draw_line(prev_x1, prev_y1, x1, y1, hb::shared::render::Color(GameColors::NightBlueBright.r, GameColors::NightBlueBright.g, GameColors::NightBlueBright.b, 128), hb::shared::render::BlendMode::Additive);
			break;
		}
		err = 0;
		CMisc::get_point(sX, sY, dX, dY, &tX, &tY, &err, j * 10);
		prev_x1 = x1;
		prev_y1 = y1;
		dir = CMisc::get_next_move_dir(x1, y1, tX, tY);
		switch (dir) {
		case 1:	rY -= 5; break;
		case 2: rY -= 5; rX += 5; break;
		case 3:	rX += 5; break;
		case 4: rX += 5; rY += 5; break;
		case 5: rY += 5; break;
		case 6: rX -= 5; rY += 5; break;
		case 7: rX -= 5; break;
		case 8: rX -= 5; rY -= 5; break;
		}
		if (rX < -20) rX = -20;
		if (rX > 20) rX = 20;
		if (rY < -20) rY = -20;
		if (rY > 20) rY = 20;
		x1 = x1 + rX;
		y1 = y1 + rY;
		if ((abs(tX - dX) < 5) && (abs(tY - dY) < 5)) break;
	}
	switch (type) {
	case 1:
		(*m_effect_sprites)[6]->draw(x1, y1, (rand() % 2), hb::shared::sprite::DrawParams::alpha_blend(0.75f));
		break;
	}
}
