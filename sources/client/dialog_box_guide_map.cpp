#include "dialog_box_guide_map.h"
#include "config_manager.h"
#include "cursor_target.h"
#include "game.h"
#include "global_def.h"
#include "lan_eng.h"
#include <algorithm>
#include <cmath>
#include <format>
#include "input.h"


using namespace hb::shared::action;
using namespace hb::client::sprite_id;

namespace
{
	constexpr int widget_size = 128;
	constexpr float event_marker_size = 28.0f;   // the old monster marker's frame size
	constexpr int edge_refusal_tiles = 30;   // a double-click this close to a map edge does nothing
}

DialogBox_GuideMap::DialogBox_GuideMap(CGame* game)
	: IDialogBox(DialogBoxId::GuideMap, game)
{
	set_default_rect(LOGICAL_MAX_X() - widget_size, 0, widget_size, widget_size);
}

void DialogBox_GuideMap::load_map(const std::string& map_name)
{
	if (m_game->m_Renderer == nullptr)
		return;
	const std::string sprite_dir = hb::shared::sprite::Sprites::get_sprite_path();
	if (!m_creature_icons.is_loaded())
		m_creature_icons.load(*m_game->m_Renderer, sprite_dir);
	m_minimap.load(*m_game->m_Renderer, sprite_dir, map_name);
	m_minimap.set_zoom_level(config_manager::get().get_minimap_zoom_level());
}

void DialogBox_GuideMap::step_zoom(int steps)
{
	const int level = std::clamp(config_manager::get().get_minimap_zoom_level() + steps, 0, hb::client::minimap::tier_count - 1);
	config_manager::get().set_minimap_zoom_level(level);
	m_minimap.set_zoom_level(level);
}

bool DialogBox_GuideMap::is_active() const
{
	return m_game->m_map_index >= 0 && !m_minimap.is_empty()
		&& m_game->m_map_data->m_map_size_x != 0 && m_game->m_map_data->m_map_size_y != 0;
}

bool DialogBox_GuideMap::is_hit_testable() const
{
	return is_active();
}

void DialogBox_GuideMap::widget_origin(short& sX, short& sY) const
{
	sX = m_x;
	sY = m_y;
	if (sX < 20) sX = 0;
	if (sY < 20) sY = 0;
	if (sX > LOGICAL_WIDTH() - widget_size - 20) sX = LOGICAL_WIDTH() - widget_size;
	if (sY > 547 - widget_size - 20) sY = 547 - widget_size;
}

bool DialogBox_GuideMap::mouse_in_widget(short sX, short sY) const
{
	const int mouse_x = hb::shared::input::get_mouse_x();
	const int mouse_y = hb::shared::input::get_mouse_y();
	return mouse_x >= sX && mouse_x < sX + widget_size && mouse_y >= sY && mouse_y < sY + widget_size;
}

void DialogBox_GuideMap::on_update()
{
	// clear expired monster event
	if (m_game->m_monster_event_time != 0 &&
		(m_game->m_cur_time - m_game->m_monster_event_time) >= 30000)
	{
		m_game->m_monster_event_time = 0;
		m_game->m_monster_id = 0;
	}

	if (!is_active())
		return;

	// the config holds the tier, so the sys menu and the keys stay in step with the map
	m_minimap.set_zoom_level(config_manager::get().get_minimap_zoom_level());

	const short wheel_delta = hb::shared::input::get_mouse_wheel_delta();
	if (wheel_delta != 0)
	{
		short sX, sY;
		widget_origin(sX, sY);
		if (mouse_in_widget(sX, sY))
			step_zoom(wheel_delta > 0 ? 1 : -1);
	}
}

void DialogBox_GuideMap::draw_border(short sX, short sY)
{
	m_game->m_Renderer->draw_rect_outline(sX - 2, sY - 2, widget_size + 4, widget_size + 4, hb::shared::render::Color(50, 50, 50), 2);
}

void DialogBox_GuideMap::draw_map(short sX, short sY)
{
	const auto& map = *m_game->m_map_data;
	const int player_x = player().m_player_x;
	const int player_y = player().m_player_y;
	// the old sprite path drew the map at alpha 0.25 (opacity) with dialog transparency on
	const float alpha = config_manager::get().is_dialog_transparency_enabled() ? 0.25f : 1.0f;
	m_minimap.draw(*m_game->m_Renderer, sX, sY, widget_size, map.m_map_size_x, map.m_map_size_y, player_x, player_y, alpha);

	float marker_x, marker_y;
	if (m_minimap.tile_to_widget(sX, sY, widget_size, map.m_map_size_x, map.m_map_size_y, player_x, player_y,
		player_x, player_y, marker_x, marker_y))
	{
		m_game->m_sprite[InterfaceNdCrusade]->draw(static_cast<int>(std::lround(marker_x)), static_cast<int>(std::lround(marker_y)), 37);
	}

	if ((m_game->m_cur_time - m_game->m_monster_event_time) < 30000 && (m_game->m_cur_time % 500) < 370)
	{
		if (m_minimap.tile_to_widget(sX, sY, widget_size, map.m_map_size_x, map.m_map_size_y, player_x, player_y,
			m_game->m_event_x, m_game->m_event_y, marker_x, marker_y))
		{
			m_creature_icons.draw(*m_game->m_Renderer, m_game->m_monster_id, marker_x, marker_y, event_marker_size);
		}
	}
}

void DialogBox_GuideMap::draw_location_tooltip(short mouse_x, short mouse_y, int shX, int shY)
{
	std::string G_cTxt = std::format("{}, {}", shX, shY);

	// Aresden map locations
	if (m_game->m_map_index == 11)
	{
		if (shX > 46 && shX < 66 && shY > 107 && shY < 127) G_cTxt = DEF_MSG_MAPNAME_MAGICTOWER;
		else if (shX > 103 && shX < 123 && shY > 86 && shY < 116) G_cTxt = DEF_MSG_MAPNAME_GUILDHALL;
		else if (shX > 176 && shX < 196 && shY > 62 && shY < 82) G_cTxt = DEF_MSG_MAPNAME_CATH;
		else if (shX > 135 && shX < 155 && shY > 113 && shY < 133) G_cTxt = DEF_MSG_MAPNAME_CITYHALL;
		else if (shX > 97 && shX < 117 && shY > 175 && shY < 195) G_cTxt = DEF_MSG_MAPNAME_WAREHOUSE;
		else if (shX > 223 && shX < 243 && shY > 124 && shY < 144) G_cTxt = DEF_MSG_MAPNAME_WAREHOUSE;
		else if (shX > 118 && shX < 138 && shY > 157 && shY < 177) G_cTxt = DEF_MSG_MAPNAME_SHOP;
		else if (shX > 148 && shX < 178 && shY > 188 && shY < 208) G_cTxt = DEF_MSG_MAPNAME_BLACKSMITH;
		else if (shX > 69 && shX < 89 && shY > 199 && shY < 219) G_cTxt = DEF_MSG_MAPNAME_DUNGEON;
		else if (shX > 21 && shX < 41 && shY > 266 && shY < 286) G_cTxt = DEF_MSG_MAPNAME_HUNT;
		else if (shX > 20 && shX < 40 && shY > 13 && shY < 33) G_cTxt = DEF_MSG_MAPNAME_ML;
		else if (shX > 246 && shX < 266 && shY > 16 && shY < 36) G_cTxt = DEF_MSG_MAPNAME_ML;
		else if (shX > 265 && shX < 285 && shY > 195 && shY < 215) G_cTxt = DEF_MSG_MAPNAME_FARM;
		else if (shX > 88 && shX < 108 && shY > 150 && shY < 170) G_cTxt = DEF_MSG_MAPNAME_CMDHALL;
	}
	// Elvine map locations
	else if (m_game->m_map_index == 3)
	{
		if (shX > 170 && shX < 190 && shY > 65 && shY < 85) G_cTxt = DEF_MSG_MAPNAME_MAGICTOWER;
		else if (shX > 67 && shX < 87 && shY > 130 && shY < 150) G_cTxt = DEF_MSG_MAPNAME_GUILDHALL;
		else if (shX > 121 && shX < 141 && shY > 66 && shY < 86) G_cTxt = DEF_MSG_MAPNAME_CATH;
		else if (shX > 135 && shX < 155 && shY > 117 && shY < 137) G_cTxt = DEF_MSG_MAPNAME_CITYHALL;
		else if (shX > 190 && shX < 213 && shY > 118 && shY < 138) G_cTxt = DEF_MSG_MAPNAME_WAREHOUSE;
		else if (shX > 73 && shX < 103 && shY > 165 && shY < 185) G_cTxt = DEF_MSG_MAPNAME_WAREHOUSE;
		else if (shX > 217 && shX < 237 && shY > 142 && shY < 162) G_cTxt = DEF_MSG_MAPNAME_SHOP;
		else if (shX > 216 && shX < 256 && shY > 99 && shY < 119) G_cTxt = DEF_MSG_MAPNAME_BLACKSMITH;
		else if (shX > 251 && shX < 271 && shY > 73 && shY < 93) G_cTxt = DEF_MSG_MAPNAME_DUNGEON;
		else if (shX > 212 && shX < 232 && shY > 13 && shY < 33) G_cTxt = DEF_MSG_MAPNAME_HUNT;
		else if (shX > 16 && shX < 36 && shY > 262 && shY < 282) G_cTxt = DEF_MSG_MAPNAME_ML;
		else if (shX > 244 && shX < 264 && shY > 248 && shY < 268) G_cTxt = DEF_MSG_MAPNAME_ML;
		else if (shX > 264 && shX < 284 && shY > 177 && shY < 207) G_cTxt = DEF_MSG_MAPNAME_FARM;
		else if (shX > 207 && shX < 227 && shY > 79 && shY < 99) G_cTxt = DEF_MSG_MAPNAME_CMDHALL;
	}
	// Elvine Farm
	else if (m_game->m_map_index == 5)
	{
		if (shX > 62 && shX < 82 && shY > 187 && shY < 207) G_cTxt = DEF_MSG_MAPNAME_WAREHOUSE;
		else if (shX > 81 && shX < 101 && shY > 169 && shY < 189) G_cTxt = DEF_MSG_MAPNAME_SHOP;
		else if (shX > 101 && shX < 131 && shY > 180 && shY < 200) G_cTxt = DEF_MSG_MAPNAME_BLACKSMITH;
		else if (shX > 130 && shX < 150 && shY > 195 && shY < 215) G_cTxt = DEF_MSG_MAPNAME_DUNGEON;
		else if (shX > 86 && shX < 106 && shY > 139 && shY < 159) G_cTxt = DEF_MSG_MAPNAME_BARRACK;
	}
	// Aresden Farm
	else if (m_game->m_map_index == 6)
	{
		if (shX > 30 && shX < 50 && shY > 80 && shY < 100) G_cTxt = DEF_MSG_MAPNAME_WAREHOUSE;
		else if (shX > 55 && shX < 85 && shY > 80 && shY < 100) G_cTxt = DEF_MSG_MAPNAME_BLACKSMITH;
		else if (shX > 52 && shX < 72 && shY > 80 && shY < 100) G_cTxt = DEF_MSG_MAPNAME_SHOP;
		else if (shX > 70 && shX < 90 && shY > 60 && shY < 80) G_cTxt = DEF_MSG_MAPNAME_DUNGEON;
		else if (shX > 45 && shX < 65 && shY > 123 && shY < 143) G_cTxt = DEF_MSG_MAPNAME_BARRACK;
	}

	put_string(mouse_x - 10, mouse_y - 13, G_cTxt.c_str(), GameColors::UIPaleYellow);
}

void DialogBox_GuideMap::on_draw()
{
	if (!is_active()) return;

	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	char lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;

	short sX, sY;
	widget_origin(sX, sY);

	draw_border(sX, sY);
	draw_map(sX, sY);

	if (lb != 0) return;

	// Mouse hover - location tooltip
	if (mouse_in_widget(sX, sY))
	{
		const auto& map = *m_game->m_map_data;
		int tile_x, tile_y;
		if (m_minimap.widget_to_tile(sX, sY, widget_size, map.m_map_size_x, map.m_map_size_y,
			player().m_player_x, player().m_player_y,
			static_cast<float>(mouse_x), static_cast<float>(mouse_y), tile_x, tile_y))
		{
			draw_location_tooltip(mouse_x, mouse_y, tile_x, tile_y);
		}
	}
}

bool DialogBox_GuideMap::on_click()
{
	return false;
}

bool DialogBox_GuideMap::on_double_click()
{
	if (CursorTarget::get_cursor_type() != cursor_type::arrow) return false;
	if (!is_active()) return false;

	short sX, sY;
	widget_origin(sX, sY);

	const auto& map = *m_game->m_map_data;
	int shX, shY;
	if (!m_minimap.widget_to_tile(sX, sY, widget_size, map.m_map_size_x, map.m_map_size_y,
		player().m_player_x, player().m_player_y,
		static_cast<float>(hb::shared::input::get_mouse_x()), static_cast<float>(hb::shared::input::get_mouse_y()), shX, shY))
		return false;

	if (shX < edge_refusal_tiles || shY < edge_refusal_tiles) return false;
	if (shX > map.m_map_size_x - edge_refusal_tiles || shY > map.m_map_size_y - edge_refusal_tiles) return false;

	if (config_manager::get().is_running_mode_enabled() && player().m_sp > 0)
		player().m_Controller.set_command(Type::Run);
	else
		player().m_Controller.set_command(Type::Move);

	player().m_Controller.set_destination(shX, shY);
	player().m_Controller.calculate_player_turn(player().m_player_x, player().m_player_y, m_game->m_map_data.get());

	return true;
}
