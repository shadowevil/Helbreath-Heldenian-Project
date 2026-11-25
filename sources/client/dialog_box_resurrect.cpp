#include "dialog_box_resurrect.h"
#include "game.h"
#include "input.h"
#include "packet/shared_packets.h"
#include "screen_on_game.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
DialogBox_Resurrect::DialogBox_Resurrect(CGame* game)
	: IDialogBox(DialogBoxId::Resurrect, game)
{
	set_default_rect(185 , 100 , 270, 105);
}

void DialogBox_Resurrect::on_draw()
{
	short sX = m_x;
	short sY = m_y;

	if (!draw_kit_background(kit_frame::gold_box, sX, sY, 270, 104))   // the Game1 frame 2 box's size
		draw_new_dialog_box(InterfaceNdGame1, sX, sY, 2);

	put_string(sX + 50, sY + 20, "Someone intend to resurrect you.", GameColors::UIMagicBlue);
	put_string(sX + 80, sY + 35, "Will you revive here?", GameColors::UIMagicBlue);

	// Yes button
	if (mouse_in(btn_yes))
		draw_new_dialog_box(InterfaceNdButton, sX + 30, sY + 55, 19);
	else
		draw_new_dialog_box(InterfaceNdButton, sX + 30, sY + 55, 18);

	// No button
	if (mouse_in(btn_no))
		draw_new_dialog_box(InterfaceNdButton, sX + 170, sY + 55, 3);
	else
		draw_new_dialog_box(InterfaceNdButton, sX + 170, sY + 55, 2);
}

bool DialogBox_Resurrect::on_click()
{
	// Yes button
	if (mouse_in(btn_yes))
	{
		{
			hb::net::PacketRequestHeaderOnly req{};
			req.header.msg_id = MsgId::RequestResurrectYes;
			req.header.msg_type = 0;
			send_game_packet(req);
		}
		disable_this_dialog();
		return true;
	}

	// No button
	if (mouse_in(btn_no))
	{
		{
			hb::net::PacketRequestHeaderOnly req{};
			req.header.msg_id = MsgId::RequestResurrectNo;
			req.header.msg_type = 0;
			send_game_packet(req);
		}
		disable_this_dialog();
		return true;
	}

	return false;
}

bool DialogBox_Resurrect::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	m_x = 185;
	m_y = 100;
	m_mode = 0;
	m_view = 0;
	m_game->on_game()->m_skill_using_status = false;
	return true;
}
