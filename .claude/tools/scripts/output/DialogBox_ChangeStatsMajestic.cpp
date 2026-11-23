#include "DialogBox_ChangeStatsMajestic.h"
#include "Game.h"
#include "lan_eng.h"
#include "GlobalDef.h"
#include "SpriteID.h"
#include "NetMessages.h"
#include <format>
#include <string>
#include "IInput.h"
#include "Packet/SharedPackets.h"
#include "Screen_OnGame.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
static constexpr int POINTS_PER_MAJESTIC = 3;
static constexpr int MIN_STAT_VALUE = 10;

DialogBox_ChangeStatsMajestic::DialogBox_ChangeStatsMajestic(CGame* game)
	: IDialogBox(DialogBoxId::ChangeStatsMajestic, game)
{
	set_default_rect(0 , 0 , 258, 339);
}

static int GetPendingMajesticCost(CGame* game)
{
	int total_reduction = -(game->m_player->m_lu_str + game->m_player->m_lu_vit +
		game->m_player->m_lu_dex + game->m_player->m_lu_int +
		game->m_player->m_lu_mag + game->m_player->m_lu_char);
	return total_reduction / POINTS_PER_MAJESTIC;
}

void DialogBox_ChangeStatsMajestic::draw_stat_row(short sX, short sY, int y_offset,
	const char* label, int current_stat, int16_t pending_change,
	short mouse_x, short mouse_y, int arrow_y_offset, bool can_undo, bool can_reduce)
{
	std::string txt;

	put_string(sX + 24, sY + y_offset, (char*)label, GameColors::UIBlack);

	txt = std::format("{}", current_stat);
	put_string(sX + 109, sY + y_offset, txt.c_str(), GameColors::UILabel);

	int new_stat = current_stat + pending_change;
	txt = std::format("{}", new_stat);
	if (new_stat != current_stat)
		put_string(sX + 162, sY + y_offset, txt.c_str(), GameColors::UIRed);
	else
		put_string(sX + 162, sY + y_offset, txt.c_str(), GameColors::UILabel);

	// UP arrow highlight (undo reduction)
	if ((mouse_x >= sX + 195) && (mouse_x <= sX + 205) && (mouse_y >= sY + arrow_y_offset) && (mouse_y <= sY + arrow_y_offset + 6) && can_undo)
		m_game->m_sprite[InterfaceNdGame4]->draw(sX + 195, sY + arrow_y_offset, 5);

	// DOWN arrow highlight (reduce stat)
	if ((mouse_x >= sX + 210) && (mouse_x <= sX + 220) && (mouse_y >= sY + arrow_y_offset) && (mouse_y <= sY + arrow_y_offset + 6) && can_reduce)
		m_game->m_sprite[InterfaceNdGame4]->draw(sX + 210, sY + arrow_y_offset, 6);
}

void DialogBox_ChangeStatsMajestic::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;
	short size_x = m_size_x;
	std::string txt;

	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 0);
	draw_new_dialog_box(InterfaceNdText, sX, sY, 2);
	draw_new_dialog_box(InterfaceNdGame4, sX + 16, sY + 100, 4);

	put_aligned_string(sX, sX + size_x, sY + 50, DRAW_DIALOGBOX_LEVELUP_SETTING14);
	put_aligned_string(sX, sX + size_x, sY + 65, DRAW_DIALOGBOX_LEVELUP_SETTING15);

	// Majestic Points Remaining (total - pending cost)
	int pending_cost = GetPendingMajesticCost(m_game);
	int remaining = m_game->on_game()->m_gizon_item_upgrade_left - pending_cost;

	put_string(sX + 20, sY + 85, DRAW_DIALOGBOX_LEVELUP_SETTING16, GameColors::UIBlack);
	txt = std::format("{}", remaining);
	if (remaining > 0)
		put_string(sX + 73, sY + 102, txt.c_str(), GameColors::UIGreen);
	else
		put_string(sX + 73, sY + 102, txt.c_str(), GameColors::UIBlack);

	bool can_afford = (remaining > 0);

	draw_stat_row(sX, sY, 125, DRAW_DIALOGBOX_LEVELUP_SETTING4,
		player().m_str, player().m_lu_str, mouse_x, mouse_y, 127,
		(player().m_lu_str < 0),
		can_afford && (player().m_str + player().m_lu_str - POINTS_PER_MAJESTIC >= MIN_STAT_VALUE));

	draw_stat_row(sX, sY, 144, DRAW_DIALOGBOX_LEVELUP_SETTING5,
		player().m_vit, player().m_lu_vit, mouse_x, mouse_y, 146,
		(player().m_lu_vit < 0),
		can_afford && (player().m_vit + player().m_lu_vit - POINTS_PER_MAJESTIC >= MIN_STAT_VALUE));

	draw_stat_row(sX, sY, 163, DRAW_DIALOGBOX_LEVELUP_SETTING6,
		player().m_dex, player().m_lu_dex, mouse_x, mouse_y, 165,
		(player().m_lu_dex < 0),
		can_afford && (player().m_dex + player().m_lu_dex - POINTS_PER_MAJESTIC >= MIN_STAT_VALUE));

	draw_stat_row(sX, sY, 182, DRAW_DIALOGBOX_LEVELUP_SETTING7,
		player().m_int, player().m_lu_int, mouse_x, mouse_y, 184,
		(player().m_lu_int < 0),
		can_afford && (player().m_int + player().m_lu_int - POINTS_PER_MAJESTIC >= MIN_STAT_VALUE));

	draw_stat_row(sX, sY, 201, DRAW_DIALOGBOX_LEVELUP_SETTING8,
		player().m_mag, player().m_lu_mag, mouse_x, mouse_y, 203,
		(player().m_lu_mag < 0),
		can_afford && (player().m_mag + player().m_lu_mag - POINTS_PER_MAJESTIC >= MIN_STAT_VALUE));

	draw_stat_row(sX, sY, 220, DRAW_DIALOGBOX_LEVELUP_SETTING9,
		player().m_charisma, player().m_lu_char, mouse_x, mouse_y, 222,
		(player().m_lu_char < 0),
		can_afford && (player().m_charisma + player().m_lu_char - POINTS_PER_MAJESTIC >= MIN_STAT_VALUE));

	// Cancel button (left)
	if (mouse_in(btn_cancel))
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 17);
	else draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 16);

	// Confirm button (right) — show as active only when there are pending changes
	if (pending_cost > 0)
	{
		if (mouse_in(btn_confirm))
			draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else
			draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
	}
}

bool DialogBox_ChangeStatsMajestic::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;

	int pending_cost = GetPendingMajesticCost(m_game);
	int remaining = m_game->on_game()->m_gizon_item_upgrade_left - pending_cost;

	struct StatEntry {
		int16_t* pending;
		int current_stat;
		int arrow_y;
	};

	StatEntry stats[] = {
		{ &player().m_lu_str,  player().m_str,      127 },
		{ &player().m_lu_vit,  player().m_vit,      146 },
		{ &player().m_lu_dex,  player().m_dex,      165 },
		{ &player().m_lu_int,  player().m_int,      184 },
		{ &player().m_lu_mag,  player().m_mag,      203 },
		{ &player().m_lu_char, player().m_charisma, 222 },
	};

	for (auto& s : stats)
	{
		// UP arrow — undo a pending reduction (restore 3 points)
		if ((mouse_x >= sX + 195) && (mouse_x <= sX + 205) && (mouse_y >= sY + s.arrow_y) && (mouse_y <= sY + s.arrow_y + 6))
		{
			if (*s.pending < 0)
			{
				*s.pending += POINTS_PER_MAJESTIC;
				play_sound_effect('E', 14, 5);
			}
		}

		// DOWN arrow — reduce stat by 3 (costs 1 majestic point)
		if ((mouse_x >= sX + 210) && (mouse_x <= sX + 220) && (mouse_y >= sY + s.arrow_y) && (mouse_y <= sY + s.arrow_y + 6))
		{
			if (remaining > 0 && (s.current_stat + *s.pending - POINTS_PER_MAJESTIC >= MIN_STAT_VALUE))
			{
				*s.pending -= POINTS_PER_MAJESTIC;
				remaining--;
				play_sound_effect('E', 14, 5);
			}
		}
	}

	// Confirm button (right) — send all pending reductions to server
	// Don't close the dialog here; the success/failure handler will close it after applying changes
	if (mouse_in(btn_confirm))
	{
		if (pending_cost > 0)
		{
			{
		hb::net::PacketRequestStateChange req{};
		req.header.msg_id = MsgId::StateChangePoint;
		req.header.msg_type = 0;
		req.str = static_cast<int16_t>(-player().m_lu_str);
		req.vit = static_cast<int16_t>(-player().m_lu_vit);
		req.dex = static_cast<int16_t>(-player().m_lu_dex);
		req.intel = static_cast<int16_t>(-player().m_lu_int);
		req.mag = static_cast<int16_t>(-player().m_lu_mag);
		req.chr = static_cast<int16_t>(-player().m_lu_char);
		send_game_packet(req);
	}
			play_sound_effect('E', 14, 5);
		}
	}

	// Cancel button (left) — discard changes and close
	if (mouse_in(btn_cancel))
	{
		player().m_lu_str = player().m_lu_vit = player().m_lu_dex = 0;
		player().m_lu_int = player().m_lu_mag = player().m_lu_char = 0;
		disable_dialog_box(DialogBoxId::ChangeStatsMajestic);
		play_sound_effect('E', 14, 5);
	}

	return false;
}

bool DialogBox_ChangeStatsMajestic::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	auto* luDlg = get_dialog_box(DialogBoxId::LevelUpSetting);
	if (luDlg) { m_x = luDlg->m_x + 10; m_y = luDlg->m_y + 10; }
	m_mode = 0;
	m_view = 0;
	player().m_lu_str = player().m_lu_vit = player().m_lu_dex = 0;
	player().m_lu_int = player().m_lu_mag = player().m_lu_char = 0;
	m_game->on_game()->m_skill_using_status = false;
	return true;
}

bool DialogBox_ChangeStatsMajestic::on_disable()
{
	player().m_lu_str = 0;
	player().m_lu_vit = 0;
	player().m_lu_dex = 0;
	player().m_lu_int = 0;
	player().m_lu_mag = 0;
	player().m_lu_char = 0;
	return true;
}
