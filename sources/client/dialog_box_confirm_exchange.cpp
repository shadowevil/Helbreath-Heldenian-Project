#include "dialog_box_confirm_exchange.h"
#include "dialog_box_exchange.h"
#include "game.h"
#include "packet_send_helpers.h"

#include "input.h"
#include "audio_manager.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
DialogBox_ConfirmExchange::DialogBox_ConfirmExchange(CGame* game)
	: IDialogBox(DialogBoxId::ConfirmExchange, game)
{
	set_default_rect(285 , 200 , 270, 105);
}

void DialogBox_ConfirmExchange::on_draw()
{
	short sX = m_x;
	short sY = m_y;

	draw_new_dialog_box(InterfaceNdGame1, sX, sY, 2);

	switch (m_mode)
	{
	case mode::question:
		put_string(sX + 35, sY + 30, "Do you really want to exchange?", GameColors::UIMagicBlue);
		put_string(sX + 36, sY + 30, "Do you really want to exchange?", GameColors::UIMagicBlue);

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
		break;

	case mode::waiting:
		put_string(sX + 45, sY + 36, "Waiting for response...", GameColors::UIMagicBlue);
		put_string(sX + 46, sY + 36, "Waiting for response...", GameColors::UIMagicBlue);
		break;
	}
}

bool DialogBox_ConfirmExchange::on_click()
{
	short sX = m_x;
	short sY = m_y;

	switch (m_mode)
	{
	case mode::question:
		// Yes button
		if (mouse_in(btn_yes))
		{
			if ((get_dialog_box_as<DialogBox_Exchange>(DialogBoxId::Exchange)->m_slots[0].v1 != -1) && (get_dialog_box_as<DialogBox_Exchange>(DialogBoxId::Exchange)->m_slots[4].v1 != -1))
			{
				{
					auto pkt = hb::net::make_common_command(CommonType::confirm_exchange_item, player().m_player_x, player().m_player_y);
					pkt.v1 = get_dialog_box_as<DialogBox_Exchange>(DialogBoxId::Exchange)->m_slots[0].v1;
					pkt.v2 = // ItemID
					get_dialog_box_as<DialogBox_Exchange>(DialogBoxId::Exchange)->m_slots[0].v3;
					pkt.v3 = // Amount
					0;
					send_game_packet(pkt);
				}
				audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
				get_dialog_box_as<DialogBox_Exchange>(DialogBoxId::Exchange)->m_mode = DialogBox_Exchange::mode::confirmed;
				m_mode = mode::waiting;
			}
			return true;
		}

		// No button
		if (mouse_in(btn_no))
		{
			disable_this_dialog();
			disable_dialog_box(DialogBoxId::Exchange);
			disable_dialog_box(DialogBoxId::Map);
			send_game_packet(hb::net::make_common_command(CommonType::cancel_exchange_item, player().m_player_x, player().m_player_y));
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
		break;

	case mode::waiting:
		break;
	}

	return false;
}
