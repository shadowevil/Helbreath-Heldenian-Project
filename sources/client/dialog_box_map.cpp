#include "dialog_box_map.h"
#include "config_manager.h"
#include "game.h"
#include "global_def.h"
#include "game_fonts.h"
#include "text_lib_ext.h"
#include <format>
#include <string>
using namespace hb::client::sprite_id;

DialogBox_Map::DialogBox_Map(CGame* game)
	: IDialogBox(DialogBoxId::Map, game)
{
	set_default_rect(496 , 88 , 270, 346);
}

bool DialogBox_Map::on_enable(int type, int64_t v1, int v2, const char* string)
{
	m_map_zone = static_cast<int>(v1);
	m_map_id = v2;
	m_size_x = 290;
	m_size_y = 290;
	return true;
}

void DialogBox_Map::on_draw()
{
	short sX = m_x;
	short sY = m_y;
	const bool dialogTrans = config_manager::get().is_dialog_transparency_enabled();
	uint32_t time = m_game->m_cur_time;
	double v1, v2, v3;
	int tX, tY, size_x, size_y, dX, dY;

	size_x = 0;
	size_y = 0;

	switch (m_map_zone) {
	case 1:
		switch (m_map_id) {
		case 0: // aresden
			if (dialogTrans)
				m_game->m_sprite[InterfaceNewMaps1]->draw(sX, sY, 0, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
			else m_game->m_sprite[InterfaceNewMaps1]->draw(sX, sY, 0);
			dX = 19;
			dY = 20;
			size_x = 260;
			size_y = 260;
			break;

		case 1: // elvine
			if (dialogTrans)
				m_game->m_sprite[InterfaceNewMaps1]->draw(sX, sY, 1, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
			else m_game->m_sprite[InterfaceNewMaps1]->draw(sX, sY, 1);
			dX = 20;
			dY = 18;
			size_x = 260;
			size_y = 260;
			break;

		case 2: // middleland
			if (dialogTrans)
				m_game->m_sprite[InterfaceNewMaps2]->draw(sX, sY, 0, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
			else m_game->m_sprite[InterfaceNewMaps2]->draw(sX, sY, 0);
			dX = 11;
			dY = 31;
			size_x = 280;
			size_y = 253;
			break;

		case 3: // default
			if (dialogTrans)
				m_game->m_sprite[InterfaceNewMaps2]->draw(sX, sY, 1, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
			else m_game->m_sprite[InterfaceNewMaps2]->draw(sX, sY, 1);
			dX = 52;
			dY = 42;
			size_x = 200;
			size_y = 200;
			break;

		case 4: // aresden dungeon
			if (dialogTrans)
				m_game->m_sprite[InterfaceNewMaps3]->draw(sX, sY, 0, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
			else m_game->m_sprite[InterfaceNewMaps3]->draw(sX, sY, 0);
			dX = 40;
			dY = 40;
			size_x = 220;
			size_y = 220;
			break;

		case 5: // elvine dungeon
			if (dialogTrans)
				m_game->m_sprite[InterfaceNewMaps3]->draw(sX, sY, 1, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
			else m_game->m_sprite[InterfaceNewMaps3]->draw(sX, sY, 1);
			dX = 40;
			dY = 40;
			size_x = 220;
			size_y = 220;
			break;

		case 6: // aresden
			if (dialogTrans)
				m_game->m_sprite[InterfaceNewMaps4]->draw(sX, sY, 0, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
			else m_game->m_sprite[InterfaceNewMaps4]->draw(sX, sY, 0);
			dX = 40;
			dY = 40;
			size_x = 220;
			size_y = 220;
			break;

		case 7: // elvine
			if (dialogTrans)
				m_game->m_sprite[InterfaceNewMaps4]->draw(sX, sY, 1, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
			else m_game->m_sprite[InterfaceNewMaps4]->draw(sX, sY, 1);
			dX = 40;
			dY = 40;
			size_x = 220;
			size_y = 220;
			break;

		case 8: // aresden
			if (dialogTrans)
				m_game->m_sprite[InterfaceNewMaps5]->draw(sX, sY, 0, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
			else m_game->m_sprite[InterfaceNewMaps5]->draw(sX, sY, 0);
			dX = 40;
			dY = 32;
			size_x = 220;
			size_y = 220;
			break;

		case 9: // elvine
			if (dialogTrans)
				m_game->m_sprite[InterfaceNewMaps5]->draw(sX, sY, 1, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
			else m_game->m_sprite[InterfaceNewMaps5]->draw(sX, sY, 1);
			dX = 40;
			dY = 38;
			size_x = 220;
			size_y = 220;
			break;
		}

		v1 = static_cast<double>(m_game->m_map_data->m_map_size_x);
		v2 = static_cast<double>(player().m_player_x);
		v3 = (v2 * static_cast<double>(size_x)) / v1;
		tX = static_cast<int>(v3) + dX;

		v1 = static_cast<double>(m_game->m_map_data->m_map_size_y);
		if (v1 == 752) v1 = 680;
		v2 = static_cast<double>(player().m_player_y);
		v3 = (v2 * static_cast<double>(size_y)) / v1;
		tY = static_cast<int>(v3) + dY;

		draw_new_dialog_box(InterfaceNdGame4, sX + tX, sY + tY, 43);
		std::string coordBuf;
		coordBuf = std::format("{},{}", player().m_player_x, player().m_player_y);
		hb::shared::text::draw_text(GameFont::SprFont3_2, sX + 10 + tX - 5, sY + 10 + tY - 6, coordBuf.c_str(), hb::shared::text::TextStyle::with_two_point_shadow(GameColors::Yellow4x));
		break;
	}
}

bool DialogBox_Map::on_click()
{
	// Map dialog has no click handling - it just displays
	return false;
}
