#include "DialogBox_Magic.h"
#include "ConfigManager.h"
#include "Game.h"
#include "MagicCastingSystem.h"
#include "GameFonts.h"
#include "IInput.h"
#include "Misc.h"
#include "TextLibExt.h"
#include "lan_eng.h"
#include "WeatherManager.h"
#include "AudioManager.h"
#include <format>
#include <string>

using namespace hb::shared::item;
using namespace hb::client::sprite_id;

bool DialogBox_Magic::on_enable(int type, int64_t v1, int v2, const char* string)
{
	// type=1: explicit circle selection (Ctrl+number keys)
	if (type == 1 && v1 >= 0 && v1 <= 9)
		m_circle_view = static_cast<short>(v1);
	return true;
}

DialogBox_Magic::DialogBox_Magic(CGame* game)
	: IDialogBox(DialogBoxId::Magic, game)
{
	m_can_close_on_right_click = true;
	set_default_rect(497 , 57 , 258, 328);
}

void DialogBox_Magic::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
	if (!m_game->ensure_magic_configs_loaded()) return;
	short sX, sY;
	int c_pivot, i, yloc, magic_circle, mana_cost;
	const bool dialogTrans = config_manager::get().is_dialog_transparency_enabled();
	std::string mana;

	char txt[120];
	double v1, v2, v3, v4;
	int result;
	short level_magic;
	uint32_t time = m_game->m_cur_time;

	sX = m_x;
	sY = m_y;

	draw_new_dialog_box(InterfaceNdGame1, sX, sY, 1, false, dialogTrans);
	draw_new_dialog_box(InterfaceNdText, sX, sY, 7, false, dialogTrans);

	// Handle scroll wheel input
	if (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::Magic && z != 0)
	{
		if (z > 0) m_circle_view--;
		if (z < 0) m_circle_view++;
	}
	if (m_circle_view < 0) m_circle_view = 9;
	if (m_circle_view > 9) m_circle_view = 0;

	// Circle title
	static constexpr const char* kMagicCircleNames[] = {
		DRAW_DIALOGBOX_MAGIC1, DRAW_DIALOGBOX_MAGIC2, DRAW_DIALOGBOX_MAGIC3,
		DRAW_DIALOGBOX_MAGIC4, DRAW_DIALOGBOX_MAGIC5, DRAW_DIALOGBOX_MAGIC6,
		DRAW_DIALOGBOX_MAGIC7, DRAW_DIALOGBOX_MAGIC8, DRAW_DIALOGBOX_MAGIC9,
		DRAW_DIALOGBOX_MAGIC10
	};
	const char* circle_name = kMagicCircleNames[m_circle_view];
	put_aligned_string(sX + 3, sX + 256, sY + 50, circle_name);
	put_aligned_string(sX + 4, sX + 257, sY + 50, circle_name);

	// Column headers
	hb::shared::text::draw_text(GameFont::Default, sX + 30, sY + 57, "Spell", hb::shared::text::TextStyle::from_color(GameColors::UILabel).with_bold());
	hb::shared::text::draw_text(GameFont::Default, sX + 206, sY + 57, "Cost", hb::shared::text::TextStyle::from_color(GameColors::UILabel).with_bold());

	c_pivot = m_circle_view * 10;
	yloc = 0;
	int player_int = player().m_int + player().m_angelic_int;
	for (i = 0; i < 9; i++)
	{
		if ((player().m_magic_mastery[c_pivot + i] != 0) && (m_game->m_magic_cfg_list[c_pivot + i] != 0))
		{
			std::snprintf(txt, sizeof(txt), "%s", m_game->m_magic_cfg_list[c_pivot + i]->m_name.c_str());
			CMisc::replace_string(txt, '-', ' ');

			mana_cost = magic_casting_system::get().get_mana_cost(c_pivot + i);
			int int_req = m_game->m_magic_cfg_list[c_pivot + i]->m_value_2;

			if (int_req > player_int)
			{
				hb::shared::text::draw_text(GameFont::Bitmap1, sX + 30, sY + 70 + yloc, txt, hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicDisabled));
				mana = std::format("{:3}", mana_cost);
				hb::shared::text::draw_text(GameFont::Bitmap1, sX + 206, sY + 70 + yloc, mana.c_str(), hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicDisabled));
			}
			else if (mana_cost > player().m_mp)
			{
				hb::shared::text::draw_text(GameFont::Bitmap1, sX + 30, sY + 70 + yloc, txt, hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicPurple));
				mana = std::format("{:3}", mana_cost);
				hb::shared::text::draw_text(GameFont::Bitmap1, sX + 206, sY + 70 + yloc, mana.c_str(), hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicPurple));
			}
			else if ((mouse_x >= sX + 30) && (mouse_x <= sX + 240) && (mouse_y >= sY + 70 + yloc) && (mouse_y <= sY + 70 + 14 + yloc))
			{
				hb::shared::text::draw_text(GameFont::Bitmap1, sX + 30, sY + 70 + yloc, txt, hb::shared::text::TextStyle::with_highlight(GameColors::UINearWhite));
				mana = std::format("{:3}", mana_cost);
				hb::shared::text::draw_text(GameFont::Bitmap1, sX + 206, sY + 70 + yloc, mana.c_str(), hb::shared::text::TextStyle::with_highlight(GameColors::UINearWhite));
			}
			else
			{
				hb::shared::text::draw_text(GameFont::Bitmap1, sX + 30, sY + 70 + yloc, txt, hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
				mana = std::format("{:3}", mana_cost);
				hb::shared::text::draw_text(GameFont::Bitmap1, sX + 206, sY + 70 + yloc, mana.c_str(), hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
			}

			yloc += 18;
		}
	}

	// No spells message
	if (yloc == 0)
	{
		put_aligned_string(sX + 3, sX + 256, sY + 100, DRAW_DIALOGBOX_MAGIC11);
		put_aligned_string(sX + 3, sX + 256, sY + 115, DRAW_DIALOGBOX_MAGIC12);
		put_aligned_string(sX + 3, sX + 256, sY + 130, DRAW_DIALOGBOX_MAGIC13);
		put_aligned_string(sX + 3, sX + 256, sY + 145, DRAW_DIALOGBOX_MAGIC14);
		put_aligned_string(sX + 3, sX + 256, sY + 160, DRAW_DIALOGBOX_MAGIC15);
	}

	// Circle selector bar
	m_game->m_sprite[InterfaceSprFonts]->draw(sX + 30, sY + 250, 19);

	// Circle selector highlight
	switch (m_circle_view) {
	case 0: m_game->m_sprite[InterfaceSprFonts]->draw(sX + 30, sY + 250, 20); break;
	case 1: m_game->m_sprite[InterfaceSprFonts]->draw(sX + 43, sY + 250, 21); break;
	case 2: m_game->m_sprite[InterfaceSprFonts]->draw(sX + 61, sY + 250, 22); break;
	case 3: m_game->m_sprite[InterfaceSprFonts]->draw(sX + 86, sY + 250, 23); break;
	case 4: m_game->m_sprite[InterfaceSprFonts]->draw(sX + 106, sY + 250, 24); break;
	case 5: m_game->m_sprite[InterfaceSprFonts]->draw(sX + 121, sY + 250, 25); break;
	case 6: m_game->m_sprite[InterfaceSprFonts]->draw(sX + 142, sY + 250, 26); break;
	case 7: m_game->m_sprite[InterfaceSprFonts]->draw(sX + 169, sY + 250, 27); break;
	case 8: m_game->m_sprite[InterfaceSprFonts]->draw(sX + 202, sY + 250, 28); break;
	case 9: m_game->m_sprite[InterfaceSprFonts]->draw(sX + 222, sY + 250, 29); break;
	}

	// Calculate magic probability
	magic_circle = m_circle_view + 1;

	static int _tmp_iMCProb[] = { 0, 300, 250, 200, 150, 100, 80, 70, 60, 50, 40 };
	static int _tmp_iMLevelPenalty[] = { 0,   5,   5,   8,   8,  10, 14, 28, 32, 36, 40 };

	if (player().m_skill_mastery[4] == 0)
		v1 = 1.0f;
	else
		v1 = static_cast<double>(player().m_skill_mastery[4]);

	v2 = static_cast<double>(v1 / 100.0f);
	v3 = static_cast<double>(_tmp_iMCProb[magic_circle]);
	v1 = v2 * v3;
	result = static_cast<int>(v1);

	if ((player().m_int + player().m_angelic_int) > 50)
		result += ((player().m_int + player().m_angelic_int) - 50) / 2;

	level_magic = (player().m_level / 10);
	if (magic_circle != level_magic)
	{
		if (magic_circle > level_magic)
		{
			v1 = static_cast<double>(player().m_level - level_magic * 10);
			v2 = static_cast<double>(abs(magic_circle - level_magic)) * _tmp_iMLevelPenalty[magic_circle];
			v3 = static_cast<double>(abs(magic_circle - level_magic)) * 10;
			v4 = (v1 / v3) * v2;
			result -= abs(abs(magic_circle - level_magic) * _tmp_iMLevelPenalty[magic_circle] - static_cast<int>(v4));
		}
		else
		{
			result += 5 * abs(magic_circle - level_magic);
		}
	}

	// Weather modifier
	switch (weather_manager::get().get_weather_status())
	{
	case 0: break;
	case 1: result = result - (result / 24); break;
	case 2: result = result - (result / 12); break;
	case 3: result = result - (result / 5);  break;
	}

	// Equipment magic bonus
	for (i = 0; i < hb::shared::limits::MaxItems; i++)
	{
		if (player().m_item_list[i] == 0) continue;
		if (m_game->m_is_item_equipped[i] == true)
		{
			if (player().m_item_list[i]->m_instance.prefix_type == static_cast<uint8_t>(hb::shared::item::AttributePrefixType::Special))
			{
				v1 = static_cast<double>(result);
				v2 = static_cast<double>(player().m_item_list[i]->m_instance.prefix_value * m_game->m_prefix_multiplier[10]);
				v3 = v1 + v2;
				result = static_cast<int>(v3);
				break;
			}
		}
	}

	if (result > 100) result = 100;
	if (player().m_sp < 1) result = result * 9 / 10;
	if (result < 1) result = 1;

	// Display magic probability
	auto cTxt_str = std::format(DRAW_DIALOGBOX_MAGIC16, result);
	put_aligned_string(sX, sX + 256, sY + 267, cTxt_str.c_str());
	put_aligned_string(sX + 1, sX + 257, sY + 267, cTxt_str.c_str());

	// Alchemy button
	if (mouse_in(btn_alchemy))
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + 285, 49, false, dialogTrans);
	else
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + 285, 48, false, dialogTrans);
}

bool DialogBox_Magic::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	int i, c_pivot, yloc;
	short sX, sY;

	sX = m_x;
	sY = m_y;
	c_pivot = m_circle_view * 10;
	yloc = 0;
	int click_player_int = player().m_int + player().m_angelic_int;
	for (i = 0; i < 9; i++)
	{
		if ((player().m_magic_mastery[c_pivot + i] != 0) && (m_game->m_magic_cfg_list[c_pivot + i] != 0))
		{
			if ((mouse_x >= sX + 30) && (mouse_x <= sX + 240) && (mouse_y >= sY + 70 + yloc) && (mouse_y <= sY + 70 + 18 + yloc))
			{
				int int_req = m_game->m_magic_cfg_list[c_pivot + i]->m_value_2;
				if (int_req > click_player_int)
				{
					m_game->add_event_list(DLGBOX_CLICK_MAGIC3, 10);
					return true;
				}
				magic_casting_system::get().begin_cast(c_pivot + i);
				audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
				return true;
			}
			yloc += 18;
		}
	}
	if (mouse_in(circle_0)) m_circle_view = 0;
	if (mouse_in(circle_1)) m_circle_view = 1;
	if (mouse_in(circle_2)) m_circle_view = 2;
	if (mouse_in(circle_3)) m_circle_view = 3;
	if (mouse_in(circle_4)) m_circle_view = 4;
	if (mouse_in(circle_5)) m_circle_view = 5;
	if (mouse_in(circle_6)) m_circle_view = 6;
	if (mouse_in(circle_7)) m_circle_view = 7;
	if (mouse_in(circle_8)) m_circle_view = 8;
	if (mouse_in(circle_9)) m_circle_view = 9;

	if (mouse_in(btn_alchemy))
	{
		if (player().m_skill_mastery[12] == 0) add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY16, 10);
		else
		{
			for (i = 0; i < hb::shared::limits::MaxItems; i++)
			{
				if (player().m_item_list[i] == 0) continue;
				CItem* cfg = m_game->get_item_config(player().m_item_list[i]->m_id_num);
				if (cfg && (cfg->get_item_type() == hb::shared::item::item_type::tool) &&
					(player().m_item_list[i]->m_id_num == 227)) // Alchemy Bowl
				{
					enable_dialog_box(DialogBoxId::Manufacture, 1, 0, 0, 0);
					add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY10, 10);
					audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
					return true;
				}
			}
			add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY15, 10);
		}
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
	}
	return false;
}
