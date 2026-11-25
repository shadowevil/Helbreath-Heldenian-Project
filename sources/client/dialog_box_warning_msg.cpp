#include "dialog_box_warning_msg.h"
#include "game.h"
#include "inventory_manager.h"
#include "lan_eng.h"
#include "game_fonts.h"
#include "text_lib_ext.h"
#include "input.h"
using namespace hb::client::sprite_id;

DialogBox_WarningMsg::DialogBox_WarningMsg(CGame* game)
	: IDialogBox(DialogBoxId::WarningBattleArea, game)
{
	set_default_rect(0 , 0 , 310, 170);
}

void DialogBox_WarningMsg::on_draw()
{
	short sX = m_x;
	short sY = m_y;

	draw_kit_notice_frame(m_game->get_panel_kit(), sX, sY, DEF_MSG_WARNING1, 40);
	put_string(sX + 30, sY + 57, DEF_MSG_WARNING2, GameColors::UIOrange);
	put_string(sX + 30, sY + 74, DEF_MSG_WARNING3, GameColors::UIOrange);
	put_string(sX + 30, sY + 92, DEF_MSG_WARNING4, GameColors::UIOrange);
	put_string(sX + 30, sY + 110, DEF_MSG_WARNING5, GameColors::UIOrange);

	// OK button
	if (mouse_in(btn_ok))
		draw_text_button(sX + 122, sY + 127, 1);
	else
		draw_text_button(sX + 122, sY + 127, 0);
}

bool DialogBox_WarningMsg::on_click()
{
	// OK button click
	if (mouse_in(btn_ok))
	{
		disable_this_dialog();
		return true;
	}

	return false;
}

bool DialogBox_WarningMsg::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	m_item_index = type;
	return true;
}

bool DialogBox_WarningMsg::on_disable()
{
	{ int idx = m_item_index;
	inventory_manager::get().unlock_item(idx); }
	return true;
}
