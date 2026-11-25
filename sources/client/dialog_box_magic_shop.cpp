#include "dialog_box_magic_shop.h"
#include "game.h"
#include "packet_send_helpers.h"

#include "input.h"
#include "misc.h"
#include "lan_eng.h"
#include "game_fonts.h"
#include "text_lib_ext.h"
#include "audio_manager.h"
#include <format>
#include <string>

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
DialogBox_MagicShop::DialogBox_MagicShop(CGame* game)
	: IDialogBox(DialogBoxId::MagicShop, game)
{
	set_default_rect(30 , 30 , 304, 328);
}

void DialogBox_MagicShop::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;

	// A plain kit frame (the original's hexagram is dropped, user ruling) with the title and the sunken panel at
	// the original sprite's places (HH-0066), else the original sprite frame and title bar
	if (hb::client::panel_kit* panel_kit = m_game->get_panel_kit())
	{
		constexpr hb::client::kit_rect content_panel{16, 36, 272, 271};
		const hb::client::kit_dialog kit(*panel_kit, hb::client::kit_point{304, 328}, hb::client::kit_point{sX, sY},
			1.0f, kit_frame_style(kit_frame::rivet_box));
		kit.title("Spells", 24, kit_layout::title_fill, "divider_wave", 14);
		kit.inset(content_panel);
	}
	else
	{
		m_game->draw_new_dialog_box(InterfaceNdGame4, sX, sY, 1);
		m_game->draw_new_dialog_box(InterfaceNdText, sX, sY, 14);
	}

	// Mouse wheel scrolling - read and consume input directly
	if (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::MagicShop)
	{
		short wheel_delta = hb::shared::input::get_mouse_wheel_delta();
		if (wheel_delta != 0)
		{
			if (wheel_delta > 0)
				m_page--;
			if (wheel_delta < 0)
				m_page++;
			// Consume the input
		}
	}

	// clamp view (pages 0-9)
	if (m_page < 0)
		m_page = 9;
	if (m_page > 9)
		m_page = 0;

	// Column headers
	hb::shared::text::draw_text(GameFont::Default, sX - 20 + 60 - 17, sY - 35 + 90, DRAW_DIALOGBOX_MAGICSHOP11, hb::shared::text::TextStyle::from_color(GameColors::UILabel)); // "Spell Name"
	hb::shared::text::draw_text(GameFont::Default, sX - 20 + 232 - 20, sY - 35 + 90, DRAW_DIALOGBOX_MAGICSHOP12, hb::shared::text::TextStyle::from_color(GameColors::UILabel)); // "Req.Int"
	hb::shared::text::draw_text(GameFont::Default, sX - 20 + 270, sY - 35 + 90, DRAW_DIALOGBOX_MAGICSHOP13, hb::shared::text::TextStyle::from_color(GameColors::UILabel)); // "Cost"

	draw_spell_list(sX, sY);
	draw_page_indicator(sX, sY);

	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 275, sX + m_size_x - sX, 15, DRAW_DIALOGBOX_MAGICSHOP14, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
}

void DialogBox_MagicShop::draw_spell_list(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	int c_pivot = m_page * 10;
	int yloc = 0;
	std::string mana;

	char txt[120];

	for (int i = 0; i < 9; i++)
	{
		if ((m_game->m_magic_cfg_list[c_pivot + i] != nullptr) &&
			(m_game->m_magic_cfg_list[c_pivot + i]->m_is_visible))
		{
			std::snprintf(txt, sizeof(txt), "%s", m_game->m_magic_cfg_list[c_pivot + i]->m_name.c_str());
			CMisc::replace_string(txt, '-', ' ');

			if (player().m_magic_mastery[c_pivot + i] != 0)
			{
				// Already mastered - purple color
				hb::shared::text::draw_text(GameFont::Bitmap1, sX + 24, sY + 70 + yloc, txt, hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicPurple));
				mana = std::format("{:3}", m_game->m_magic_cfg_list[c_pivot + i]->m_value_2);
				hb::shared::text::draw_text(GameFont::Bitmap1, sX + 200, sY + 70 + yloc, mana.c_str(), hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicPurple));
				mana = std::format("{:3}", m_game->m_magic_cfg_list[c_pivot + i]->m_value_3);
				hb::shared::text::draw_text(GameFont::Bitmap1, sX + 241, sY + 70 + yloc, mana.c_str(), hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicPurple));
			}
			else if ((mouse_x >= sX + 24) && (mouse_x <= sX + 24 + 135) &&
				(mouse_y >= sY + 70 + yloc) && (mouse_y <= sY + 70 + 14 + yloc))
			{
				// Hovering - white color
				hb::shared::text::draw_text(GameFont::Bitmap1, sX - 20 + 44, sY + 70 + yloc, txt, hb::shared::text::TextStyle::with_highlight(GameColors::UINearWhite));
				mana = std::format("{:3}", m_game->m_magic_cfg_list[c_pivot + i]->m_value_2);
				hb::shared::text::draw_text(GameFont::Bitmap1, sX - 20 + 220, sY + 70 + yloc, mana.c_str(), hb::shared::text::TextStyle::with_highlight(GameColors::UINearWhite));
				mana = std::format("{:3}", m_game->m_magic_cfg_list[c_pivot + i]->m_value_3);
				hb::shared::text::draw_text(GameFont::Bitmap1, sX - 20 + 261, sY + 70 + yloc, mana.c_str(), hb::shared::text::TextStyle::with_highlight(GameColors::UINearWhite));
			}
			else
			{
				hb::shared::text::draw_text(GameFont::Bitmap1, sX - 20 + 44, sY + 70 + yloc, txt, hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
				mana = std::format("{:3}", m_game->m_magic_cfg_list[c_pivot + i]->m_value_2);
				hb::shared::text::draw_text(GameFont::Bitmap1, sX - 20 + 220, sY + 70 + yloc, mana.c_str(), hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
				mana = std::format("{:3}", m_game->m_magic_cfg_list[c_pivot + i]->m_value_3);
				hb::shared::text::draw_text(GameFont::Bitmap1, sX - 20 + 261, sY + 70 + yloc, mana.c_str(), hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
			}
			yloc += 18;
		}
	}
}

void DialogBox_MagicShop::draw_page_indicator(short sX, short sY)
{
	uint32_t time = m_game->m_cur_time;

	// draw page number strip
	m_game->m_sprite[InterfaceSprFonts]->draw(sX + 55, sY + 250, 19);

	// Highlight current page
	short view = m_page;
	switch (view)
	{
	case 0: m_game->m_sprite[InterfaceSprFonts]->draw(sX - 20 + 44 + 31, sY + 250, 20); break;
	case 1: m_game->m_sprite[InterfaceSprFonts]->draw(sX - 20 + 57 + 31, sY + 250, 21); break;
	case 2: m_game->m_sprite[InterfaceSprFonts]->draw(sX - 20 + 75 + 31, sY + 250, 22); break;
	case 3: m_game->m_sprite[InterfaceSprFonts]->draw(sX - 20 + 100 + 31, sY + 250, 23); break;
	case 4: m_game->m_sprite[InterfaceSprFonts]->draw(sX - 20 + 120 + 31, sY + 250, 24); break;
	case 5: m_game->m_sprite[InterfaceSprFonts]->draw(sX - 20 + 135 + 31, sY + 250, 25); break;
	case 6: m_game->m_sprite[InterfaceSprFonts]->draw(sX - 20 + 156 + 31, sY + 250, 26); break;
	case 7: m_game->m_sprite[InterfaceSprFonts]->draw(sX - 20 + 183 + 31, sY + 250, 27); break;
	case 8: m_game->m_sprite[InterfaceSprFonts]->draw(sX - 20 + 216 + 31, sY + 250, 28); break;
	case 9: m_game->m_sprite[InterfaceSprFonts]->draw(sX - 20 + 236 + 31, sY + 250, 29); break;
	}
}

bool DialogBox_MagicShop::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;

	if (handle_spell_click(sX, sY))
		return true;

	handle_page_click(sX, sY);
	return false;
}

bool DialogBox_MagicShop::handle_spell_click(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	int adj_x = -20;
	int adj_y = -35;
	int c_pivot = m_page * 10;
	int yloc = 0;

	for (int i = 0; i < 9; i++)
	{
		if ((m_game->m_magic_cfg_list[c_pivot + i] != nullptr) &&
			(m_game->m_magic_cfg_list[c_pivot + i]->m_is_visible))
		{
			if ((mouse_x >= sX + adj_x + 44) && (mouse_x <= sX + adj_x + 135 + 44) &&
				(mouse_y >= sY + adj_y + 70 + yloc + 35) && (mouse_y <= sY + adj_y + 70 + 14 + yloc + 35))
			{
				if (player().m_magic_mastery[c_pivot + i] == 0)
				{
					{
						auto pkt = hb::net::make_common_command_str(CommonType::ReqStudyMagic, m_game->m_player->m_player_x, m_game->m_player->m_player_y);
						std::snprintf(pkt.text, sizeof(pkt.text), "%s", m_game->m_magic_cfg_list[c_pivot + i]->m_name.c_str());
						m_game->send_game_packet(pkt);
					}
					audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
				}
				return true;
			}
			yloc += 18;
		}
	}
	return false;
}

void DialogBox_MagicShop::handle_page_click(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	int adj_x = -20;
	int adj_y = -35;

	if ((mouse_x >= sX + adj_x + 42 + 31) && (mouse_x <= sX + adj_x + 50 + 31) &&
		(mouse_y >= sY + adj_y + 248 + 35) && (mouse_y <= sY + adj_y + 260 + 35))
		m_page = 0;

	if ((mouse_x >= sX + adj_x + 55 + 31) && (mouse_x <= sX + adj_x + 68 + 31) &&
		(mouse_y >= sY + adj_y + 248 + 35) && (mouse_y <= sY + adj_y + 260 + 35))
		m_page = 1;

	if ((mouse_x >= sX + adj_x + 73 + 31) && (mouse_x <= sX + adj_x + 93 + 31) &&
		(mouse_y >= sY + adj_y + 248 + 35) && (mouse_y <= sY + adj_y + 260 + 35))
		m_page = 2;

	if ((mouse_x >= sX + adj_x + 98 + 31) && (mouse_x <= sX + adj_x + 113 + 31) &&
		(mouse_y >= sY + adj_y + 248 + 35) && (mouse_y <= sY + adj_y + 260 + 35))
		m_page = 3;

	if ((mouse_x >= sX + adj_x + 118 + 31) && (mouse_x <= sX + adj_x + 129 + 31) &&
		(mouse_y >= sY + adj_y + 248 + 35) && (mouse_y <= sY + adj_y + 260 + 35))
		m_page = 4;

	if ((mouse_x >= sX + adj_x + 133 + 31) && (mouse_x <= sX + adj_x + 150 + 31) &&
		(mouse_y >= sY + adj_y + 248 + 35) && (mouse_y <= sY + adj_y + 260 + 35))
		m_page = 5;

	if ((mouse_x >= sX + adj_x + 154 + 31) && (mouse_x <= sX + adj_x + 177 + 31) &&
		(mouse_y >= sY + adj_y + 248 + 35) && (mouse_y <= sY + adj_y + 260 + 35))
		m_page = 6;

	if ((mouse_x >= sX + adj_x + 181 + 31) && (mouse_x <= sX + adj_x + 210 + 31) &&
		(mouse_y >= sY + adj_y + 248 + 35) && (mouse_y <= sY + adj_y + 260 + 35))
		m_page = 7;

	if ((mouse_x >= sX + adj_x + 214 + 31) && (mouse_x <= sX + adj_x + 230 + 31) &&
		(mouse_y >= sY + adj_y + 248 + 35) && (mouse_y <= sY + adj_y + 260 + 35))
		m_page = 8;

	if ((mouse_x >= sX + adj_x + 234 + 31) && (mouse_x <= sX + adj_x + 245 + 31) &&
		(mouse_y >= sY + adj_y + 248 + 35) && (mouse_y <= sY + adj_y + 260 + 35))
		m_page = 9;
}

bool DialogBox_MagicShop::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	if (player().m_skill_mastery[4] == 0) {
		enable_dialog_box(DialogBoxId::NpcTalk, 0, 480, 0);
		return false;
	}
	m_mode = 0;
	m_page = 0;
	return true;
}
