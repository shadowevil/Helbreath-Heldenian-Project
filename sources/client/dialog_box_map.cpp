#include "dialog_box_map.h"
#include "config_manager.h"
#include "game.h"
#include "global_def.h"
#include "game_fonts.h"
#include "text_lib_ext.h"
#include <cmath>
#include <format>
#include <string>
using namespace hb::client::sprite_id;

namespace
{
	// The parchment square, drawn at the dialog origin
	constexpr int parchment_units = 300;
	// The old frames' opacity with dialog transparency on
	constexpr float transparent_alpha = 0.25f;
}

DialogBox_Map::DialogBox_Map(CGame* game)
	: IDialogBox(DialogBoxId::Map, game)
{
	set_default_rect(496 , 88 , 270, 346);
}

bool DialogBox_Map::kit_has_map(const std::string& map_name)
{
	return hb::client::parchment_kit::has_map(hb::shared::sprite::Sprites::get_sprite_path(), map_name);
}

bool DialogBox_Map::load_kit_for_current_map()
{
	if (m_game->m_Renderer == nullptr)
		return false;
	if (m_kit.is_loaded() && m_opened_map == m_game->m_map_name)
		return true;
	m_kit.unload();
	if (!kit_has_map(m_game->m_map_name))
		return false;
	return m_kit.load(*m_game->m_Renderer, hb::shared::sprite::Sprites::get_sprite_path(), m_game->m_map_name);
}

bool DialogBox_Map::on_enable(int type, int64_t v1, int v2, const char* string)
{
	// the dialog opens only for a map the kit has
	if (!load_kit_for_current_map())
		return false;
	m_opened_map = m_game->m_map_name;
	m_size_x = 290;
	m_size_y = 290;
	return true;
}

bool DialogBox_Map::on_disable()
{
	m_kit.unload();
	return true;
}

void DialogBox_Map::draw_marker(short sX, short sY, int tX, int tY)
{
	const std::string coords = std::format("{},{}", player().m_player_x, player().m_player_y);
	hb::shared::text::draw_text(GameFont::SprFont3_2, sX + 10 + tX - 5, sY + 10 + tY - 6, coords.c_str(), hb::shared::text::TextStyle::with_two_point_shadow(GameColors::Yellow4x));
}

void DialogBox_Map::on_draw()
{
	const short sX = m_x;
	const short sY = m_y;
	const bool dialog_trans = config_manager::get().is_dialog_transparency_enabled();

	if (m_game->m_map_name != m_opened_map)
	{
		// the player changed map with the dialog open: follow with the kit, else close
		if (!load_kit_for_current_map())
		{
			disable_this_dialog();
			return;
		}
		m_opened_map = m_game->m_map_name;
	}

	if (!m_kit.is_loaded())
		return;

	m_kit.draw(*m_game->m_Renderer, sX, sY, parchment_units, dialog_trans ? transparent_alpha : 1.0f);
	float marker_x, marker_y;
	if (m_kit.tile_to_square(sX, sY, parchment_units, player().m_player_x, player().m_player_y, marker_x, marker_y))
	{
		const int mx = static_cast<int>(std::lround(marker_x));
		const int my = static_cast<int>(std::lround(marker_y));
		// the legacy marker draws only its coordinates (its old Game4 frame 43 never existed in the shipped sheet; the
		// draw was dropped with the sheet, HH-0094); the kit adds the guide map's player dot (centred by its pivot)
		m_game->m_sprite[InterfaceNdCrusade]->draw(mx, my, 37);
		draw_marker(sX, sY, mx - sX, my - sY);
	}
}

bool DialogBox_Map::on_click()
{
	// Map dialog has no click handling - it just displays
	return false;
}
