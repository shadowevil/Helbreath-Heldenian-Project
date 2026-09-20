#include "DialogBox_ItemDropAmount.h"
#include "Game.h"
#include "InventoryManager.h"
#include "ItemNameFormatter.h"
#include "GlobalDef.h"
#include "lan_eng.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include <format>
#include <string>
#include "TextInputManager.h"
#include "TextFieldRenderer.h"
using namespace hb::client::sprite_id;

DialogBox_ItemDropAmount::DialogBox_ItemDropAmount(CGame* game)
	: IDialogBox(DialogBoxId::ItemDropExternal, game)
{
	set_default_rect(0 , 0 , 215, 87);
}

void DialogBox_ItemDropAmount::on_draw()
{
	short sX = m_x;
	short sY = m_y;
	std::string txt;


	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 5);

	switch (m_mode)
	{
	case mode::input:
	{
		auto itemInfo = item_name_formatter::get().format(player().m_item_list[m_item_index].get());

		if (m_label[0] == '\0')
			txt = std::format(DRAW_DIALOGBOX_QUERY_DROP_ITEM_AMOUNT1, itemInfo.name.c_str());
		else
			txt = std::format(DRAW_DIALOGBOX_QUERY_DROP_ITEM_AMOUNT2, itemInfo.name.c_str(), m_label);

		if (m_max_amount < 1000)
			put_string(sX + 30, sY + 20, txt.c_str(), GameColors::UILabel);

		put_string(sX + 30, sY + 35, DRAW_DIALOGBOX_QUERY_DROP_ITEM_AMOUNT3, GameColors::UILabel);

		if (m_game->get_dialog_box_manager().get_top_id() != DialogBoxId::ItemDropExternal)
			hb::shared::text::draw_text(GameFont::Default, sX + 40, sY + 57, m_game->m_amount_string.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite));

		txt = std::format("__________ (0 ~ {})", player().m_item_list[m_item_index]->m_instance.count);
		put_string(sX + 38, sY + 62, txt.c_str(), GameColors::UILabel);
		break;
	}

	case mode::selected:
	{
		auto itemInfo2 = item_name_formatter::get().format(player().m_item_list[m_item_index].get());

		if (m_label[0] == '\0')
			txt = std::format(DRAW_DIALOGBOX_QUERY_DROP_ITEM_AMOUNT1, itemInfo2.name.c_str());
		else
			txt = std::format(DRAW_DIALOGBOX_QUERY_DROP_ITEM_AMOUNT2, itemInfo2.name.c_str(), m_label);

		if (m_max_amount < 1000)
			put_string(sX + 30, sY + 20, txt.c_str(), GameColors::UILabel);

		put_string(sX + 30, sY + 35, DRAW_DIALOGBOX_QUERY_DROP_ITEM_AMOUNT3, GameColors::UILabel);
		hb::shared::text::draw_text(GameFont::Default, sX + 40, sY + 57, m_game->m_amount_string.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite));

		txt = std::format("__________ (0 ~ {})", player().m_item_list[m_item_index]->m_instance.count);
		put_string(sX + 38, sY + 62, txt.c_str(), GameColors::UILabel);
		break;
	}
	}
}

bool DialogBox_ItemDropAmount::on_click()
{
	// Click handling for this dialog is done elsewhere (text input handling)
	return false;
}

bool DialogBox_ItemDropAmount::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (!is_enabled())
	{
		m_mode = mode::input;
		m_item_index = type;
		text_input_manager::get().end_input();
		m_game->m_amount_string = std::to_string(v1);
		text_input_manager::get().start_input(m_x + 40, m_y + 57, CGame::AmountStringMaxLen, m_game->m_amount_string, false, hb::client::digits_only);
	}
	else
	{
		if (m_mode == mode::input)
		{
			text_input_manager::get().end_input();
			text_input_manager::get().start_input(m_x + 40, m_y + 57, CGame::AmountStringMaxLen, m_game->m_amount_string, false, hb::client::digits_only);
		}
	}
	return true;
}

bool DialogBox_ItemDropAmount::on_disable()
{
	if (m_mode == mode::input) {
		text_input_manager::get().end_input();
		{ int idx = m_item_index;
		inventory_manager::get().unlock_item(idx); }
	}
	return true;
}
