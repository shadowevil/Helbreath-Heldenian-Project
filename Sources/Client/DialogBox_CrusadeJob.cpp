#include "DialogBox_CrusadeJob.h"
#include "Game.h"
#include "PacketSendHelpers.h"

#include "GameFonts.h"
#include "GlobalDef.h"
#include "TextLibExt.h"
#include "lan_eng.h"
#include "IInput.h"
#include "AudioManager.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
DialogBox_CrusadeJob::DialogBox_CrusadeJob(CGame* game)
	: IDialogBox(DialogBoxId::CrusadeJob, game)
{
	set_default_rect(520 , 65 , 258, 339);
}

void DialogBox_CrusadeJob::draw_mode_select_job(short sX, short sY)
{
	put_aligned_string(sX + 24, sX + 246, sY + 45 + 20, DRAWDIALOGBOX_CRUSADEJOB1);
	put_aligned_string(sX + 24, sX + 246, sY + 60 + 20, DRAWDIALOGBOX_CRUSADEJOB2);
	put_aligned_string(sX + 24, sX + 246, sY + 75 + 20, DRAWDIALOGBOX_CRUSADEJOB3);
	put_aligned_string(sX + 24, sX + 246, sY + 90 + 20, DRAWDIALOGBOX_CRUSADEJOB4);

	if (player().m_citizen)
	{
		// Soldier option
		if (mouse_in(link_job_1))
			put_aligned_string(sX + 24, sX + 246, sY + 150, DRAWDIALOGBOX_CRUSADEJOB7, GameColors::UIWhite);
		else
			put_aligned_string(sX + 24, sX + 246, sY + 150, DRAWDIALOGBOX_CRUSADEJOB7, GameColors::UIMagicBlue);
	}

	put_aligned_string(sX + 24, sX + 246, sY + 290 - 40, DRAWDIALOGBOX_CRUSADEJOB10);
	put_aligned_string(sX + 24, sX + 246, sY + 305 - 40, DRAWDIALOGBOX_CRUSADEJOB17);

	// Help button
	if (mouse_in(btn_help))
		hb::shared::text::draw_text(GameFont::Bitmap1, sX + 50 + 160, sY + 296, "Help", hb::shared::text::TextStyle::with_highlight(GameColors::UIMagicBlue));
	else
		hb::shared::text::draw_text(GameFont::Bitmap1, sX + 50 + 160, sY + 296, "Help", hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnNormal));
}

void DialogBox_CrusadeJob::draw_mode_confirm(short sX, short sY)
{
	put_aligned_string(sX + 24, sX + 246, sY + 90 + 20, DRAWDIALOGBOX_CRUSADEJOB18);

	switch (player().m_crusade_duty)
	{
	case 1: put_aligned_string(sX + 24, sX + 246, sY + 125, DRAWDIALOGBOX_CRUSADEJOB19); break;
	case 2: put_aligned_string(sX + 24, sX + 246, sY + 125, DRAWDIALOGBOX_CRUSADEJOB20); break;
	case 3: put_aligned_string(sX + 24, sX + 246, sY + 125, DRAWDIALOGBOX_CRUSADEJOB21); break;
	}

	put_aligned_string(sX + 24, sX + 246, sY + 145, DRAWDIALOGBOX_CRUSADEJOB22);

	// "View details" link
	if (mouse_in(link_details))
		put_aligned_string(sX + 24, sX + 246, sY + 160, DRAWDIALOGBOX_CRUSADEJOB23, GameColors::UIWhite);
	else
		put_aligned_string(sX + 24, sX + 246, sY + 160, DRAWDIALOGBOX_CRUSADEJOB23, GameColors::UIMagicBlue);

	put_aligned_string(sX + 24, sX + 246, sY + 175, DRAWDIALOGBOX_CRUSADEJOB25);
	put_aligned_string(sX + 24, sX + 246, sY + 190, DRAWDIALOGBOX_CRUSADEJOB26);

	// OK button
	if (mouse_in(btn_ok))
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
	else
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
}

void DialogBox_CrusadeJob::on_draw()
{
	short sX = m_x;
	short sY = m_y;

	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 0);

	switch (m_mode)
	{
	case mode::select_job:
		draw_mode_select_job(sX, sY);
		break;
	case mode::confirm:
		draw_mode_confirm(sX, sY);
		break;
	}
}

bool DialogBox_CrusadeJob::on_click()
{
	switch (m_mode)
	{
	case mode::select_job:
		if (!player().m_citizen)
		{
			disable_dialog_box(DialogBoxId::CrusadeJob);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}

		// Soldier option
		if (mouse_in(link_job_1))
		{
			{
				auto pkt = hb::net::make_common_command(CommonType::RequestSelectCrusadeDuty, m_game->m_player->m_player_x, m_game->m_player->m_player_y);
				pkt.v1 = 1;
				m_game->send_game_packet(pkt);
			}
			disable_dialog_box(DialogBoxId::CrusadeJob);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}

		// Help button
		if (mouse_in(btn_help))
		{
			m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Text);
			m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Text, 813, 0, 0);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
		break;

	case mode::confirm:
		// View details link
		if (mouse_in(link_details))
		{
			switch (player().m_crusade_duty)
			{
			case 1: m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Text, 803, 0, 0); break;
			case 2: m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Text, 805, 0, 0); break;
			case 3: m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Text, 808, 0, 0); break;
			}
			return true;
		}

		// OK button
		if (mouse_in(btn_ok))
		{
			disable_dialog_box(DialogBoxId::CrusadeJob);
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
		break;
	}

	return false;
}

bool DialogBox_CrusadeJob::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if ((player().m_hp <= 0) || (player().m_citizen == false))
		return false;
	if (!is_enabled())
	{
		m_mode = static_cast<mode>(type);
		m_x = 520;
		m_y = 65;
		m_npc_type = static_cast<int>(v1);
	}
	return true;
}
