#include "dialog_box_map.h"
#include "config_manager.h"
#include "game.h"
#include "global_def.h"
#include "game_fonts.h"
#include "text_lib_ext.h"
#include <array>
#include <cmath>
#include <format>
#include <string>
using namespace hb::client::sprite_id;

namespace
{
	// The parchment square: the old newmaps frames are 300x300 and drawn at the dialog origin
	constexpr int parchment_units = 300;
	// The old frames' opacity with dialog transparency on
	constexpr float transparent_alpha = 0.25f;

	// The server's frames (map_type - 1): newmaps sprite and frame, and the hand-fitted player mapping
	struct legacy_frame
	{
		int sprite;
		int frame;
		int offset_x, offset_y;   // frame pixel of tile 0
		int size_x, size_y;       // frame pixels the whole map spans
	};
	constexpr std::array<legacy_frame, 10> legacy_frames{{
		{InterfaceNewMaps1, 0, 19, 20, 260, 260},   // aresden
		{InterfaceNewMaps1, 1, 20, 18, 260, 260},   // elvine
		{InterfaceNewMaps2, 0, 11, 31, 280, 253},   // middleland
		{InterfaceNewMaps2, 1, 52, 42, 200, 200},   // default
		{InterfaceNewMaps3, 0, 40, 40, 220, 220},   // huntzone2
		{InterfaceNewMaps3, 1, 40, 40, 220, 220},   // huntzone1
		{InterfaceNewMaps4, 0, 40, 40, 220, 220},   // huntzone4
		{InterfaceNewMaps4, 1, 40, 40, 220, 220},   // huntzone3
		{InterfaceNewMaps5, 0, 40, 32, 220, 220},   // arefarm
		{InterfaceNewMaps5, 1, 40, 38, 220, 220},   // elvfarm
	}};
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
	// the kit draws every map it has; the server's frames remain for maps it lacks
	const bool kit = load_kit_for_current_map();
	if (!kit && v2 == no_legacy_frame)
		return false;
	m_map_zone = static_cast<int>(v1);
	m_map_id = v2;
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
	draw_new_dialog_box(InterfaceNdGame4, sX + tX, sY + tY, 43);
	const std::string coords = std::format("{},{}", player().m_player_x, player().m_player_y);
	hb::shared::text::draw_text(GameFont::SprFont3_2, sX + 10 + tX - 5, sY + 10 + tY - 6, coords.c_str(), hb::shared::text::TextStyle::with_two_point_shadow(GameColors::Yellow4x));
}

void DialogBox_Map::draw_legacy(short sX, short sY, bool dialog_trans)
{
	if (m_map_zone != 1 || m_map_id < 0 || m_map_id >= static_cast<int>(legacy_frames.size()))
		return;
	const legacy_frame& f = legacy_frames[static_cast<size_t>(m_map_id)];
	if (dialog_trans)
		m_game->m_sprite[f.sprite]->draw(sX, sY, f.frame, hb::shared::sprite::DrawParams::alpha_blend(transparent_alpha));
	else
		m_game->m_sprite[f.sprite]->draw(sX, sY, f.frame);

	const double map_x = static_cast<double>(m_game->m_map_data->m_map_size_x);
	double map_y = static_cast<double>(m_game->m_map_data->m_map_size_y);
	if (map_y == 752) map_y = 680;
	const int tX = static_cast<int>(static_cast<double>(player().m_player_x) * f.size_x / map_x) + f.offset_x;
	const int tY = static_cast<int>(static_cast<double>(player().m_player_y) * f.size_y / map_y) + f.offset_y;
	draw_marker(sX, sY, tX, tY);
}

void DialogBox_Map::on_draw()
{
	const short sX = m_x;
	const short sY = m_y;
	const bool dialog_trans = config_manager::get().is_dialog_transparency_enabled();

	if (m_game->m_map_name != m_opened_map)
	{
		// the player changed map with the dialog open: follow with the kit, else close (the server's
		// frame was for the old map)
		if (!load_kit_for_current_map())
		{
			disable_this_dialog();
			return;
		}
		m_opened_map = m_game->m_map_name;
	}

	if (!m_kit.is_loaded())
	{
		draw_legacy(sX, sY, dialog_trans);
		return;
	}

	m_kit.draw(*m_game->m_Renderer, sX, sY, parchment_units, dialog_trans ? transparent_alpha : 1.0f);
	float marker_x, marker_y;
	if (m_kit.tile_to_square(sX, sY, parchment_units, player().m_player_x, player().m_player_y, marker_x, marker_y))
	{
		const int mx = static_cast<int>(std::lround(marker_x));
		const int my = static_cast<int>(std::lround(marker_y));
		// the shipped game_dialogs pak has no frame 43 in InterfaceNdGame4, so the legacy marker draws only
		// its coordinates; the kit adds the guide map's player dot (centred by its pivot) on the tile
		m_game->m_sprite[InterfaceNdCrusade]->draw(mx, my, 37);
		draw_marker(sX, sY, mx - sX, my - sY);
	}
}

bool DialogBox_Map::on_click()
{
	// Map dialog has no click handling - it just displays
	return false;
}
