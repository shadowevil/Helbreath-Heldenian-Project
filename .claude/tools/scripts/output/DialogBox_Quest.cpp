#include "DialogBox_Quest.h"
#include "Game.h"
#include "lan_eng.h"
#include <format>
#include <string>
#include "IInput.h"
#include "Screen_OnGame.h"
using namespace hb::client::sprite_id;

DialogBox_Quest::DialogBox_Quest(CGame* game)
	: IDialogBox(DialogBoxId::Quest, game)
{
	set_default_rect(0 , 0 , 258, 339);
}

void DialogBox_Quest::on_draw()
{
	short sX = m_x;
	short sY = m_y;
	short size_x = m_size_x;
	std::string txt;

	char temp[21];

	m_game->draw_new_dialog_box(InterfaceNdGame2, sX, sY, 2);
	m_game->draw_new_dialog_box(InterfaceNdText, sX, sY, 4);

	switch (m_mode) {
	case mode::details:
		switch (m_game->on_game()->m_quest.quest_type) {
		case 0:
			put_aligned_string(sX, sX + size_x, sY + 50 + 115 - 30, DRAW_DIALOGBOX_QUEST1, GameColors::UILabel);
			break;

		case 1: // Hunt
			if (m_game->on_game()->m_quest.is_quest_completed == false)
				put_aligned_string(sX, sX + size_x, sY + 50, DRAW_DIALOGBOX_QUEST2, GameColors::UILabel);
			else
				put_aligned_string(sX, sX + size_x, sY + 50, DRAW_DIALOGBOX_QUEST3, GameColors::UILabel);

			txt = std::format("Rest Monster : {}", m_game->on_game()->m_quest.current_count);
			put_aligned_string(sX, sX + size_x, sY + 50 + 20, txt.c_str(), GameColors::UILabel);

			std::memset(temp, 0, sizeof(temp));
			switch (m_game->on_game()->m_quest.who) {
			case 1:
			case 2:
			case 3: break;
			case 4: std::snprintf(temp, sizeof(temp), "%s", "William"); break;
			case 5:
			case 6:
			case 7: break;
			}
			txt = std::format(DRAW_DIALOGBOX_QUEST5, temp);
			put_aligned_string(sX, sX + size_x, sY + 50 + 45, txt.c_str(), GameColors::UILabel);

			std::snprintf(temp, sizeof(temp), "%s", m_game->get_npc_config_name_by_id(m_game->on_game()->m_quest.target_type));
			txt = std::format(NPC_TALK_HANDLER16, m_game->on_game()->m_quest.target_count, temp);
			put_aligned_string(sX, sX + size_x, sY + 50 + 60, txt.c_str(), GameColors::UILabel);

			if (m_game->on_game()->m_quest.target_name.starts_with("NONE")) {
				txt = DRAW_DIALOGBOX_QUEST31;
				put_aligned_string(sX, sX + size_x, sY + 50 + 75, txt.c_str(), GameColors::UILabel);
			}
			else {
				std::memset(temp, 0, sizeof(temp));
				m_game->get_official_map_name(m_game->on_game()->m_quest.target_name.c_str(), temp);
				txt = std::format(DRAW_DIALOGBOX_QUEST32, temp);
				put_aligned_string(sX, sX + size_x, sY + 50 + 75, txt.c_str(), GameColors::UILabel);

				if (m_game->on_game()->m_quest.x != 0) {
					txt = std::format(DRAW_DIALOGBOX_QUEST33, m_game->on_game()->m_quest.x, m_game->on_game()->m_quest.y, m_game->on_game()->m_quest.range);
					put_aligned_string(sX, sX + size_x, sY + 50 + 90, txt.c_str(), GameColors::UILabel);
				}
			}

			txt = std::format(DRAW_DIALOGBOX_QUEST34, m_game->on_game()->m_quest.contribution);
			put_aligned_string(sX, sX + size_x, sY + 50 + 105, txt.c_str(), GameColors::UILabel);
			break;

		case 7:
			if (m_game->on_game()->m_quest.is_quest_completed == false)
				put_aligned_string(sX, sX + size_x, sY + 50, DRAW_DIALOGBOX_QUEST26, GameColors::UILabel);
			else
				put_aligned_string(sX, sX + size_x, sY + 50, DRAW_DIALOGBOX_QUEST27, GameColors::UILabel);

			std::memset(temp, 0, sizeof(temp));
			switch (m_game->on_game()->m_quest.who) {
			case 1:
			case 2:
			case 3: break;
			case 4: std::snprintf(temp, sizeof(temp), "%s", "William"); break;
			case 5:
			case 6:
			case 7: break;
			}
			txt = std::format(DRAW_DIALOGBOX_QUEST29, temp);
			put_aligned_string(sX, sX + size_x, sY + 50 + 45, txt.c_str(), GameColors::UILabel);

			put_aligned_string(sX, sX + size_x, sY + 50 + 60, DRAW_DIALOGBOX_QUEST30, GameColors::UILabel);

			if (m_game->on_game()->m_quest.target_name.starts_with("NONE")) {
				txt = DRAW_DIALOGBOX_QUEST31;
				put_aligned_string(sX, sX + size_x, sY + 50 + 75, txt.c_str(), GameColors::UILabel);
			}
			else {
				std::memset(temp, 0, sizeof(temp));
				m_game->get_official_map_name(m_game->on_game()->m_quest.target_name.c_str(), temp);
				txt = std::format(DRAW_DIALOGBOX_QUEST32, temp);
				put_aligned_string(sX, sX + size_x, sY + 50 + 75, txt.c_str(), GameColors::UILabel);

				if (m_game->on_game()->m_quest.x != 0) {
					txt = std::format(DRAW_DIALOGBOX_QUEST33, m_game->on_game()->m_quest.x, m_game->on_game()->m_quest.y, m_game->on_game()->m_quest.range);
					put_aligned_string(sX, sX + size_x, sY + 50 + 90, txt.c_str(), GameColors::UILabel);
				}
			}

			txt = std::format(DRAW_DIALOGBOX_QUEST34, m_game->on_game()->m_quest.contribution);
			put_aligned_string(sX, sX + size_x, sY + 50 + 105, txt.c_str(), GameColors::UILabel);
			break;
		}
		break;

	case mode::unavailable:
		put_aligned_string(sX, sX + size_x, sY + 50 + 115 - 30, DRAW_DIALOGBOX_QUEST35, GameColors::UILabel);
		break;
	}

	if (mouse_in(btn_ok))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
	else
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
}

bool DialogBox_Quest::on_click()
{
	if (mouse_in(btn_ok)) {
		m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Quest);
		play_sound_effect('E', 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_Quest::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	m_mode = static_cast<mode>(type);
	auto* charDlg = get_dialog_box(DialogBoxId::CharacterInfo);
	if (charDlg) { m_x = charDlg->m_x + 20; m_y = charDlg->m_y + 20; }
	return true;
}
