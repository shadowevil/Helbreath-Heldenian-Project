#include "DialogBox_Fishing.h"
#include "Game.h"
#include "PacketSendHelpers.h"

#include "ItemNameFormatter.h"
#include "ItemSpriteMetadata.h"
#include "GameFonts.h"
#include "GlobalDef.h"
#include "TextLibExt.h"
#include "lan_eng.h"
#include <format>
#include <string>
#include "IInput.h"
#include "Screen_OnGame.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
DialogBox_Fishing::DialogBox_Fishing(CGame* game)
	: IDialogBox(DialogBoxId::Fishing, game)
{
	set_default_rect(193 , 241 , 263, 100);
}

void DialogBox_Fishing::on_draw()
{
	short sX = m_x;
	short sY = m_y;
	uint32_t time = m_game->m_cur_time;
	std::string txt;

	draw_new_dialog_box(InterfaceNdGame1, sX, sY, 2);

	auto itemInfo = item_name_formatter::get().format(m_game->find_item_id_by_name(m_fish_name),  0);

	switch (m_mode)
	{
	case 0:
		{
			int fish_cfg_id = m_game->find_item_id_by_name(m_fish_name);
			CItem* fish_cfg = m_game->get_item_config(fish_cfg_id);
			auto fish_draw = m_game->get_item_draw(fish_cfg ? fish_cfg->m_display_id : 0, item_atlas::pack, fish_cfg ? fish_cfg->sprite_is_female() : false);
			fish_draw.sprite->draw(sX + 18 + 35, sY + 18 + 17, fish_draw.frame);
		}

		txt = itemInfo.name.c_str();
		put_string(sX + 98, sY + 14, txt.c_str(), GameColors::UIWhite);

		txt = std::format(DRAW_DIALOGBOX_FISHING1, m_fish_count);
		put_string(sX + 98, sY + 28, txt.c_str(), GameColors::UIBlack);

		put_string(sX + 97, sY + 43, DRAW_DIALOGBOX_FISHING2, GameColors::UIBlack);

		txt = std::format("{} %", m_catch_chance);
		hb::shared::text::draw_text(GameFont::Bitmap1, sX + 157, sY + 40, txt.c_str(), hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnFishRed));

		// "Try Now!" button
		if (mouse_in(btn_try_now))
			hb::shared::text::draw_text(GameFont::Bitmap1, sX + 160, sY + 70, "Try Now!", hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
		else
			hb::shared::text::draw_text(GameFont::Bitmap1, sX + 160, sY + 70, "Try Now!", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnNormal));
		break;
	}
}

bool DialogBox_Fishing::on_click()
{
	switch (m_mode)
	{
	case 0:
		if (mouse_in(btn_try_now))
		{
			m_game->send_game_packet(hb::net::make_common_command(CommonType::ReqGetFishThisTime, m_game->m_player->m_player_x, m_game->m_player->m_player_y));
			m_game->add_event_list(DLGBOX_CLICK_FISH1, 10);
			disable_dialog_box(DialogBoxId::Fishing);
			m_game->play_game_sound('E', 14, 5);
			return true;
		}
		break;
	}

	return false;
}

bool DialogBox_Fishing::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	m_mode = type;
	m_catch_chance = static_cast<int>(v1);
	m_fish_count = v2;
	if (string) std::snprintf(m_fish_name, sizeof(m_fish_name), "%s", string);
	m_game->on_game()->m_skill_using_status = true;
	return true;
}

bool DialogBox_Fishing::on_disable()
{
	m_game->on_game()->m_skill_using_status = false;
	return true;
}
