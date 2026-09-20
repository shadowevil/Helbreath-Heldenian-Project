#include "DialogBox_GuideMap.h"
#include "ConfigManager.h"
#include "CursorTarget.h"
#include "Game.h"
#include "GlobalDef.h"
#include "lan_eng.h"
#include <format>
#include "IInput.h"


using namespace hb::shared::action;
using namespace hb::client::sprite_id;

DialogBox_GuideMap::DialogBox_GuideMap(CGame* game)
	: IDialogBox(DialogBoxId::GuideMap, game)
{
	set_default_rect(LOGICAL_MAX_X() - 128, 0, 128, 128);
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
}

void DialogBox_GuideMap::draw_border(short sX, short sY)
{
	m_game->m_Renderer->draw_rect_outline(sX - 2, sY - 2, 132, 132, hb::shared::render::Color(50, 50, 50), 2);
}

void DialogBox_GuideMap::draw_zoomed_map(short sX, short sY)
{
	if (m_game->m_map_data->m_map_size_x == 0 || m_game->m_map_data->m_map_size_y == 0)
		return;

	int m_iMaxMapIndex = InterfaceGuideMap + m_game->m_map_index + 1;
	uint32_t time = m_game->m_cur_time;

	if (m_game->m_map_index >= 35)
		m_iMaxMapIndex = InterfaceGuideMap + m_game->m_map_index + 1;

	short shX = player().m_player_x - 64;
	short shY = player().m_player_y - 64;
	if (shX < 0) shX = 0;
	if (shY < 0) shY = 0;
	if (shX > m_game->m_map_data->m_map_size_x - 128) shX = m_game->m_map_data->m_map_size_x - 128;
	if (shY > m_game->m_map_data->m_map_size_y - 128) shY = m_game->m_map_data->m_map_size_y - 128;

	if (config_manager::get().is_dialog_transparency_enabled())
		m_game->m_sprite[m_iMaxMapIndex]->DrawShifted(sX, sY, shX, shY, 0, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
	else
		m_game->m_sprite[m_iMaxMapIndex]->DrawShifted(sX, sY, shX, shY, 0);

	m_game->m_sprite[InterfaceNdCrusade]->draw(sX - shX + player().m_player_x, sY - shY + player().m_player_y, 37);

	if ((m_game->m_cur_time - m_game->m_monster_event_time) < 30000)
	{
		if ((m_game->m_cur_time % 500) < 370)
		{
			if (m_game->m_event_x >= shX && m_game->m_event_x <= shX + 128 &&
				m_game->m_event_y >= shY && m_game->m_event_y <= shY + 128)
			{
				m_game->m_sprite[InterfaceMonster]->draw(sX + m_game->m_event_x - shX, sY + m_game->m_event_y - shY, m_game->m_monster_id);
			}
		}
	}
}

void DialogBox_GuideMap::draw_full_map(short sX, short sY)
{
	if (m_game->m_map_data->m_map_size_x == 0 || m_game->m_map_data->m_map_size_y == 0)
		return;

	int m_iMinMapIndex = InterfaceGuideMap;
	int m_iMinMapSquare = m_game->m_map_index;
	uint32_t time = m_game->m_cur_time;

	if (m_game->m_map_index >= 35)
	{
		m_iMinMapIndex = InterfaceGuideMap + 35;
		m_iMinMapSquare = m_game->m_map_index - 35;
	}

	if (config_manager::get().is_dialog_transparency_enabled())
		m_game->m_sprite[m_iMinMapIndex]->draw(sX, sY, m_iMinMapSquare, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
	else
		m_game->m_sprite[m_iMinMapIndex]->draw(sX, sY, m_iMinMapSquare, hb::shared::sprite::DrawParams::no_color_key());

	short shX = (player().m_player_x * 128) / (m_game->m_map_data->m_map_size_x);
	short shY = (player().m_player_y * 128) / (m_game->m_map_data->m_map_size_y);
	m_game->m_sprite[InterfaceNdCrusade]->draw(sX + shX, sY + shY, 37);

	if ((m_game->m_cur_time - m_game->m_monster_event_time) < 30000)
	{
		if ((m_game->m_cur_time % 500) < 370)
		{
			shX = (m_game->m_event_x * 128) / (m_game->m_map_data->m_map_size_x);
			shY = (m_game->m_event_y * 128) / (m_game->m_map_data->m_map_size_y);
			m_game->m_sprite[InterfaceMonster]->draw(sX + shX, sY + shY, m_game->m_monster_id);
		}
	}
}

void DialogBox_GuideMap::draw_location_tooltip(short mouse_x, short mouse_y, short sX, short sY)
{
	std::string G_cTxt;
	short shX, shY;
	short size_y = m_size_y;

	if (config_manager::get().is_zoom_map_enabled())
	{
		shX = player().m_player_x - 64;
		shY = player().m_player_y - 64;
		if (shX < 0) shX = 0;
		if (shY < 0) shY = 0;
		if (shX > m_game->m_map_data->m_map_size_x - 128) shX = m_game->m_map_data->m_map_size_x - 128;
		if (shY > m_game->m_map_data->m_map_size_y - 128) shY = m_game->m_map_data->m_map_size_y - 128;
		shX += mouse_x - sX;
		shY += mouse_y - sY;
	}
	else
	{
		shX = (mouse_x - sX) * m_game->m_map_data->m_map_size_x / 128;
		shY = (mouse_y - sY) * m_game->m_map_data->m_map_size_y / 128;
	}

	G_cTxt = std::format("{}, {}", shX, shY);

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
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	char lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;
	if (m_game->m_map_index < 0) return;

	short sX = m_x;
	short sY = m_y;
	short size_x = m_size_x;
	short size_y = m_size_y;

	// clamp position
	if (sX < 20) sX = 0;
	if (sY < 20) sY = 0;
	if (sX > LOGICAL_WIDTH() - 128 - 20) sX = LOGICAL_WIDTH() - 128;
	if (sY > 547 - 128 - 20) sY = 547 - 128;

	draw_border(sX, sY);

	if (config_manager::get().is_zoom_map_enabled())
		draw_zoomed_map(sX, sY);
	else
		draw_full_map(sX, sY);

	if (lb != 0) return;

	// Mouse hover - show zoom toggle hint and location tooltip
	if (mouse_x >= sX && mouse_x < sX + size_y && mouse_y >= sY && mouse_y < sY + size_y)
	{
		short shY;
		if (sY > 213) shY = sY - 17;
		else shY = sY + size_y + 4;

		if (config_manager::get().is_zoom_map_enabled())
			put_string(sX, shY, DEF_MSG_GUIDEMAP_MIN, GameColors::UIPaleYellow);
		else
			put_string(sX, shY, DEF_MSG_GUIDEMAP_MAX, GameColors::UIPaleYellow);

		draw_location_tooltip(mouse_x, mouse_y, sX, sY);
	}
}

bool DialogBox_GuideMap::on_click()
{
	return false;
}

bool DialogBox_GuideMap::on_double_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	if (CursorTarget::get_cursor_type() != cursor_type::arrow) return false;
	if (m_game->m_map_index < 0) return false;

	short sX = m_x;
	short sY = m_y;
	short size_x = m_size_x;
	short size_y = m_size_y;

	// clamp position (same as on_draw)
	if (sX < 20) sX = 0;
	if (sY < 20) sY = 0;
	if (sX > LOGICAL_MAX_X() - 128 - 20) sX = LOGICAL_MAX_X() - 128;
	if (sY > 547 - 128 - 20) sY = 547 - 128;

	short shX, shY;
	if (config_manager::get().is_zoom_map_enabled())
	{
		shX = player().m_player_x - 64;
		shY = player().m_player_y - 64;
		if (shX < 0) shX = 0;
		if (shY < 0) shY = 0;
		if (shX > m_game->m_map_data->m_map_size_x - 128) shX = m_game->m_map_data->m_map_size_x - 128;
		if (shY > m_game->m_map_data->m_map_size_y - 128) shY = m_game->m_map_data->m_map_size_y - 128;
		shX = shX + mouse_x - sX;
		shY = shY + mouse_y - sY;
	}
	else
	{
		shX = (m_game->m_map_data->m_map_size_x * (mouse_x - sX)) / 128;
		shY = (m_game->m_map_data->m_map_size_y * (mouse_y - sY)) / 128;
	}

	if (shX < 30 || shY < 30) return false;
	if (shX > m_game->m_map_data->m_map_size_x - 30 || shY > m_game->m_map_data->m_map_size_y - 30) return false;

	if (config_manager::get().is_running_mode_enabled() && player().m_sp > 0)
		player().m_Controller.set_command(Type::Run);
	else
		player().m_Controller.set_command(Type::Move);

	player().m_Controller.set_destination(shX, shY);
	player().m_Controller.calculate_player_turn(player().m_player_x, player().m_player_y, m_game->m_map_data.get());

	return true;
}
