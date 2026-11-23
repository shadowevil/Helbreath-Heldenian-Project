#include "DialogBox_Help.h"
#include "Game.h"
#include "lan_eng.h"
#include "IInput.h"
#include "Screen_OnGame.h"
using namespace hb::client::sprite_id;

DialogBox_Help::DialogBox_Help(CGame* game)
	: IDialogBox(DialogBoxId::Help, game)
{
	set_default_rect(518 , 65 , 258, 339);
}

bool DialogBox_Help::is_mouse_over_item(short mouse_x, short mouse_y, short sX, short sY, int item)
{
	return (mouse_x >= sX + 25) && (mouse_x <= sX + 248) &&
	       (mouse_y >= sY + 50 + 15 * item) && (mouse_y < sY + 50 + 15 * (item + 1));
}

void DialogBox_Help::draw_help_item(short sX, short size_x, short sY, int item, const char* text, bool highlight)
{
	if (highlight)
		put_aligned_string(sX, sX + size_x, sY + 50 + 15 * item, (char*)text, GameColors::UIWhite);
	else
		put_aligned_string(sX, sX + size_x, sY + 50 + 15 * item, (char*)text, GameColors::UIMagicBlue);
}

void DialogBox_Help::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;
	short size_x = m_size_x;

	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 2);

	// Help topics list
	draw_help_item(sX, size_x, sY, 0, DRAW_DIALOGBOX_HELP2, is_mouse_over_item(mouse_x, mouse_y, sX, sY, 0));
	draw_help_item(sX, size_x, sY, 1, DRAW_DIALOGBOX_HELP1, is_mouse_over_item(mouse_x, mouse_y, sX, sY, 1));
	draw_help_item(sX, size_x, sY, 2, DRAW_DIALOGBOX_HELP3, is_mouse_over_item(mouse_x, mouse_y, sX, sY, 2));
	draw_help_item(sX, size_x, sY, 3, DRAW_DIALOGBOX_HELP4, is_mouse_over_item(mouse_x, mouse_y, sX, sY, 3));
	draw_help_item(sX, size_x, sY, 4, DRAW_DIALOGBOX_HELP5, is_mouse_over_item(mouse_x, mouse_y, sX, sY, 4));
	draw_help_item(sX, size_x, sY, 5, DRAW_DIALOGBOX_HELP6, is_mouse_over_item(mouse_x, mouse_y, sX, sY, 5));
	draw_help_item(sX, size_x, sY, 6, DRAW_DIALOGBOX_HELP7, is_mouse_over_item(mouse_x, mouse_y, sX, sY, 6));
	draw_help_item(sX, size_x, sY, 7, DRAW_DIALOGBOX_HELP8, is_mouse_over_item(mouse_x, mouse_y, sX, sY, 7));
	draw_help_item(sX, size_x, sY, 8, DRAW_DIALOGBOX_HELP9, is_mouse_over_item(mouse_x, mouse_y, sX, sY, 8));
	draw_help_item(sX, size_x, sY, 9, DRAW_DIALOGBOX_HELP10, is_mouse_over_item(mouse_x, mouse_y, sX, sY, 9));
	draw_help_item(sX, size_x, sY, 10, DRAW_DIALOGBOX_HELP11, is_mouse_over_item(mouse_x, mouse_y, sX, sY, 10));
	draw_help_item(sX, size_x, sY, 11, DRAW_DIALOGBOX_HELP12, is_mouse_over_item(mouse_x, mouse_y, sX, sY, 11));
	draw_help_item(sX, size_x, sY, 12, "F.A.Q.", is_mouse_over_item(mouse_x, mouse_y, sX, sY, 12));
	draw_help_item(sX, size_x, sY, 13, DRAW_DIALOGBOX_HELP13, is_mouse_over_item(mouse_x, mouse_y, sX, sY, 13));

	// Close button
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) &&
	    (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
	else
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
}

bool DialogBox_Help::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;

	// Help topic clicks - each opens a different Text dialog
	if (is_mouse_over_item(mouse_x, mouse_y, sX, sY, 0)) {
		disable_dialog_box(DialogBoxId::Text);
		enable_dialog_box(DialogBoxId::Text, 1000, 0, 0);
		return true;
	}

	if (is_mouse_over_item(mouse_x, mouse_y, sX, sY, 1)) {
		disable_dialog_box(DialogBoxId::Text);
		enable_dialog_box(DialogBoxId::Text, 900, 0, 0);
		return true;
	}

	if (is_mouse_over_item(mouse_x, mouse_y, sX, sY, 2)) {
		disable_dialog_box(DialogBoxId::Text);
		enable_dialog_box(DialogBoxId::Text, 901, 0, 0);
		return true;
	}

	if (is_mouse_over_item(mouse_x, mouse_y, sX, sY, 3)) {
		disable_dialog_box(DialogBoxId::Text);
		enable_dialog_box(DialogBoxId::Text, 902, 0, 0);
		return true;
	}

	if (is_mouse_over_item(mouse_x, mouse_y, sX, sY, 4)) {
		disable_dialog_box(DialogBoxId::Text);
		enable_dialog_box(DialogBoxId::Text, 903, 0, 0);
		m_game->on_game()->m_is_f1_help_window_enabled = true;
		return true;
	}

	if (is_mouse_over_item(mouse_x, mouse_y, sX, sY, 5)) {
		disable_dialog_box(DialogBoxId::Text);
		enable_dialog_box(DialogBoxId::Text, 904, 0, 0);
		return true;
	}

	if (is_mouse_over_item(mouse_x, mouse_y, sX, sY, 6)) {
		disable_dialog_box(DialogBoxId::Text);
		enable_dialog_box(DialogBoxId::Text, 905, 0, 0);
		return true;
	}

	if (is_mouse_over_item(mouse_x, mouse_y, sX, sY, 7)) {
		disable_dialog_box(DialogBoxId::Text);
		enable_dialog_box(DialogBoxId::Text, 906, 0, 0);
		return true;
	}

	if (is_mouse_over_item(mouse_x, mouse_y, sX, sY, 8)) {
		disable_dialog_box(DialogBoxId::Text);
		enable_dialog_box(DialogBoxId::Text, 907, 0, 0);
		return true;
	}

	if (is_mouse_over_item(mouse_x, mouse_y, sX, sY, 9)) {
		disable_dialog_box(DialogBoxId::Text);
		enable_dialog_box(DialogBoxId::Text, 908, 0, 0);
		return true;
	}

	if (is_mouse_over_item(mouse_x, mouse_y, sX, sY, 10)) {
		disable_dialog_box(DialogBoxId::Text);
		enable_dialog_box(DialogBoxId::Text, 909, 0, 0);
		return true;
	}

	if (is_mouse_over_item(mouse_x, mouse_y, sX, sY, 11)) {
		disable_dialog_box(DialogBoxId::Text);
		enable_dialog_box(DialogBoxId::Text, 910, 0, 0);
		return true;
	}

	if (is_mouse_over_item(mouse_x, mouse_y, sX, sY, 12)) {
		disable_dialog_box(DialogBoxId::Text);
		enable_dialog_box(DialogBoxId::Text, 911, 0, 0); // FAQ
		return true;
	}

	if (is_mouse_over_item(mouse_x, mouse_y, sX, sY, 13)) {
		disable_dialog_box(DialogBoxId::Text);
		enable_dialog_box(DialogBoxId::Text, 912, 0, 0);
		return true;
	}

	// Close button
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) &&
	    (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y)) {
		play_sound_effect('E', 14, 5);
		disable_this_dialog();
		return true;
	}

	return false;
}
