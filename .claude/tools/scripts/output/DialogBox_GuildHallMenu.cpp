#include "DialogBox_GuildHallMenu.h"
#include "Game.h"
#include "TeleportManager.h"
#include "lan_eng.h"
#include "GlobalDef.h"
#include "SpriteID.h"
#include "NetMessages.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include <format>
#include <string>
#include "IInput.h"
#include "Packet/SharedPackets.h"
#include "PacketSendHelpers.h"
#include "Screen_OnGame.h"


using namespace hb::shared::net;
using namespace hb::client::net;
using namespace hb::client::sprite_id;
DialogBox_GuildHallMenu::DialogBox_GuildHallMenu(CGame* game)
	: IDialogBox(DialogBoxId::GuildHallMenu, game)
{
	set_default_rect(497 , 57 , 258, 339);
}

void DialogBox_GuildHallMenu::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX, sY, size_x;
	char txt[120];
	sX = m_x;
	sY = m_y;
	size_x = m_size_x;
	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 2);

	switch (m_mode) {
	case mode::main_menu:
		if (mouse_in(link_1))
			put_aligned_string(sX, sX + size_x, sY + 70, "Teleport to Battle Field", GameColors::UIWhite);
		else put_aligned_string(sX, sX + size_x, sY + 70, "Teleport to Battle Field", GameColors::UIMagicBlue);

		if (mouse_in(link_2))
			put_aligned_string(sX, sX + size_x, sY + 95, "Hire a soldier", GameColors::UIWhite);
		else put_aligned_string(sX, sX + size_x, sY + 95, "Hire a soldier", GameColors::UIMagicBlue);

		if (mouse_in(link_3))
			put_aligned_string(sX, sX + size_x, sY + 120, "Taking Flags", GameColors::UIWhite);
		else put_aligned_string(sX, sX + size_x, sY + 120, "Taking Flags", GameColors::UIMagicBlue);

		if (mouse_in(link_4))
			put_aligned_string(sX, sX + size_x, sY + 145, "Receive a Tutelary Angel", GameColors::UIWhite);
		else put_aligned_string(sX, sX + size_x, sY + 145, "Receive a Tutelary Angel", GameColors::UIMagicBlue);
		break;

	case mode::teleport:
		if (teleport_manager::get().get_map_count() > 0)
		{
			std::string teleportBuf;
			hb::shared::text::draw_text(GameFont::Default, sX + 35, sY + 250, DRAW_DIALOGBOX_CITYHALL_MENU72_1, hb::shared::text::TextStyle::with_shadow(GameColors::UILabel));
			for (int i = 0; i < teleport_manager::get().get_map_count(); i++)
			{
				std::memset(txt, 0, sizeof(txt));
				m_game->get_official_map_name(teleport_manager::get().get_list()[i].mapname.c_str(), txt);
				teleportBuf = std::format(DRAW_DIALOGBOX_CITYHALL_MENU77, txt, teleport_manager::get().get_list()[i].cost);
				if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + 130 + i * 15) && (mouse_y <= sY + 144 + i * 15))
					put_aligned_string(sX, sX + size_x, sY + 130 + i * 15, teleportBuf.c_str(), GameColors::UIWhite);
				else put_aligned_string(sX, sX + size_x, sY + 130 + i * 15, teleportBuf.c_str(), GameColors::UIMenuHighlight);
			}
		}
		else if (teleport_manager::get().get_map_count() == -1)
		{
			put_aligned_string(sX, sX + size_x, sY + 125, DRAW_DIALOGBOX_CITYHALL_MENU73, GameColors::UILabel);
			put_aligned_string(sX, sX + size_x, sY + 150, DRAW_DIALOGBOX_CITYHALL_MENU74, GameColors::UILabel);
			put_aligned_string(sX, sX + size_x, sY + 175, DRAW_DIALOGBOX_CITYHALL_MENU75, GameColors::UILabel);
		}
		else
		{
			put_aligned_string(sX, sX + size_x, sY + 175, DRAW_DIALOGBOX_CITYHALL_MENU76, GameColors::UILabel);
		}
		break;

	case mode::hire_soldier:
		put_aligned_string(sX, sX + size_x, sY + 45, "You will hire a soldier by summon points", GameColors::UIWhite);
		if ((player().m_construction_point >= 2000) && (m_game->on_game()->m_is_crusade_mode == false))
		{
			if (mouse_in(link_1))
				put_aligned_string(sX, sX + size_x, sY + 70, "Sorceress             2000 Point", GameColors::UIWhite);
			else put_aligned_string(sX, sX + size_x, sY + 70, "Sorceress             2000 Point", GameColors::UIMagicBlue);
		}
		else put_aligned_string(sX, sX + size_x, sY + 70, "Sorceress             2000 Point", GameColors::UIDisabled);

		if ((player().m_construction_point >= 3000) && (m_game->on_game()->m_is_crusade_mode == false))
		{
			if (mouse_in(link_2))
				put_aligned_string(sX, sX + size_x, sY + 95, "Ancient Temple Knight 3000 Point", GameColors::UIWhite);
			else put_aligned_string(sX, sX + size_x, sY + 95, "Ancient Temple Knight 3000 Point", GameColors::UIMagicBlue);
		}
		else put_aligned_string(sX, sX + size_x, sY + 95, "Ancient Temple Knight 3000 Point", GameColors::UIDisabled);

		if ((player().m_construction_point >= 1500) && (m_game->on_game()->m_is_crusade_mode == false))
		{
			if (mouse_in(link_3))
				put_aligned_string(sX, sX + size_x, sY + 120, "Elf Master            1500 Point", GameColors::UIWhite);
			else put_aligned_string(sX, sX + size_x, sY + 120, "Elf Master            1500 Point", GameColors::UIMagicBlue);
		}
		else put_aligned_string(sX, sX + size_x, sY + 120, "Elf Master            1500 Point", GameColors::UIDisabled);

		if ((player().m_construction_point >= 3000) && (m_game->on_game()->m_is_crusade_mode == false))
		{
			if (mouse_in(link_4))
				put_aligned_string(sX, sX + size_x, sY + 145, "Dark Shadow Knight    3000 Point", GameColors::UIWhite);
			else put_aligned_string(sX, sX + size_x, sY + 145, "Dark Shadow Knight    3000 Point", GameColors::UIMagicBlue);
		}
		else put_aligned_string(sX, sX + size_x, sY + 145, "Dark Shadow Knight    3000 Point", GameColors::UIDisabled);

		if ((player().m_construction_point >= 4000) && (m_game->on_game()->m_is_crusade_mode == false))
		{
			if (mouse_in(link_5))
				put_aligned_string(sX, sX + size_x, sY + 170, "Heavy Battle Tank     4000 Point", GameColors::UIWhite);
			else put_aligned_string(sX, sX + size_x, sY + 170, "Heavy Battle Tank     4000 Point", GameColors::UIMagicBlue);
		}
		else put_aligned_string(sX, sX + size_x, sY + 170, "Heavy Battle Tank     4000 Point", GameColors::UIDisabled);

		if ((player().m_construction_point >= 3000) && (m_game->on_game()->m_is_crusade_mode == false))
		{
			if (mouse_in(link_6))
				put_aligned_string(sX, sX + size_x, sY + 195, "Barbarian             3000 Point", GameColors::UIWhite);
			else put_aligned_string(sX, sX + size_x, sY + 195, "Barbarian             3000 Point", GameColors::UIMagicBlue);
		}
		else put_aligned_string(sX, sX + size_x, sY + 195, "Barbarian             3000 Point", GameColors::UIDisabled);

		put_aligned_string(sX, sX + size_x, sY + 220, "You should join a guild to hire soldiers.", GameColors::UIMagicBlue);
		char pointsBuf[64];
		snprintf(pointsBuf, sizeof(pointsBuf), "Summon points : %d", player().m_construction_point);
		put_aligned_string(sX, sX + size_x, sY + 250, pointsBuf, GameColors::UIMagicBlue);
		put_aligned_string(sX, sX + size_x, sY + 280, "Maximum summon points : 12000 points.", GameColors::UIMagicBlue);
		put_aligned_string(sX, sX + size_x, sY + 300, "Maximum hiring number : 5 ", GameColors::UIMagicBlue);
		break;

	case mode::take_flag:
		put_aligned_string(sX, sX + size_x, sY + 45, "You may acquire Flags with EK points.", GameColors::UIMagicBlue);
		put_aligned_string(sX, sX + size_x, sY + 70, "Price is 10 EK per Flag.", GameColors::UIMagicBlue);
		if (mouse_in(link_take_flag))
			put_aligned_string(sX, sX + size_x, sY + 140, "Take a Flag", GameColors::UIWhite);
		else
			put_aligned_string(sX, sX + size_x, sY + 140, "Take a Flag", GameColors::UIMenuHighlight);
		break;

	case mode::tutelary_angel:
		put_aligned_string(sX, sX + size_x, sY + 45, "5 majestic points will be deducted", GameColors::UIMagicBlue);
		put_aligned_string(sX, sX + size_x, sY + 80, "upon receiving the Pendant of Tutelary Angel.", GameColors::UIMagicBlue);
		put_aligned_string(sX, sX + size_x, sY + 105, "Would you like to receive the Tutelary Angel?", GameColors::UIMagicBlue);
		auto angelBuf = std::format(DRAW_DIALOGBOX_ITEMUPGRADE11, m_game->on_game()->m_gizon_item_upgrade_left);
		put_aligned_string(sX, sX + size_x, sY + 140, angelBuf.c_str(), GameColors::UIBlack);

		if (mouse_in(link_angel_str) && (m_game->on_game()->m_gizon_item_upgrade_left > 4))
			put_aligned_string(sX, sX + size_x, sY + 175, "Tutelary Angel (STR) will be handed out.", GameColors::UIWhite);
		else put_aligned_string(sX, sX + size_x, sY + 175, "Tutelary Angel (STR) will be handed out.", GameColors::UIMenuHighlight);

		if (mouse_in(link_angel_dex) && (m_game->on_game()->m_gizon_item_upgrade_left > 4))
			put_aligned_string(sX, sX + size_x, sY + 200, "Tutelary Angel (DEX) will be handed out.", GameColors::UIWhite);
		else put_aligned_string(sX, sX + size_x, sY + 200, "Tutelary Angel (DEX) will be handed out.", GameColors::UIMenuHighlight);

		if (mouse_in(link_angel_int) && (m_game->on_game()->m_gizon_item_upgrade_left > 4))
			put_aligned_string(sX, sX + size_x, sY + 225, "Tutelary Angel (INT) will be handed out.", GameColors::UIWhite);
		else put_aligned_string(sX, sX + size_x, sY + 225, "Tutelary Angel (INT) will be handed out.", GameColors::UIMenuHighlight);

		if (mouse_in(link_angel_mag) && (m_game->on_game()->m_gizon_item_upgrade_left > 4))
			put_aligned_string(sX, sX + size_x, sY + 250, "Tutelary Angel (MAG) will be handed out.", GameColors::UIWhite);
		else put_aligned_string(sX, sX + size_x, sY + 250, "Tutelary Angel (MAG) will be handed out.", GameColors::UIMenuHighlight);
		break;
	}
}

bool DialogBox_GuildHallMenu::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX, sY;
	sX = m_x;
	sY = m_y;

	switch (m_mode) {
	case mode::main_menu:
		if (mouse_in(link_1))
		{
			m_mode = mode::teleport;
			teleport_manager::get().set_map_count(-1);
			{
			hb::net::PacketRequestName20 req{};
			req.header.msg_id = ClientMsgId::RequestHeldenianTpList;
			req.header.msg_type = 0;
			std::snprintf(req.name, sizeof(req.name), "%s", "Gail");
			send_game_packet(req);
		}
			play_sound_effect('E', 14, 5);
			return true;
		}
		if (mouse_in(link_2))
		{
			m_mode = mode::hire_soldier;
			play_sound_effect('E', 14, 5);
			return true;
		}
		if (mouse_in(link_3))
		{
			m_mode = mode::take_flag;
			play_sound_effect('E', 14, 5);
			return true;
		}
		if (mouse_in(link_4))
		{
			m_mode = mode::tutelary_angel;
			play_sound_effect('E', 14, 5);
			return true;
		}
		break;

	case mode::teleport:
		if (teleport_manager::get().get_map_count() > 0)
		{
			for (int i = 0; i < teleport_manager::get().get_map_count(); i++)
			{
				if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + 130 + i * 15) && (mouse_y <= sY + 144 + i * 15))
				{
					{
				hb::net::PacketRequestTeleportId req{};
				req.header.msg_id = ClientMsgId::RequestHeldenianTp;
				req.header.msg_type = 0;
				req.teleport_id = teleport_manager::get().get_list()[i].index;
				send_game_packet(req);
			}
					disable_dialog_box(DialogBoxId::GuildHallMenu);
					return false;
				}
			}
		}
		break;

	case mode::hire_soldier:
		if (mouse_in(link_1)
			&& (player().m_construction_point >= 2000) && (m_game->on_game()->m_is_crusade_mode == false))
		{
			{
				hb::net::PacketRequestHeldenianScroll req{};
				req.header.msg_id = MsgId::RequestHeldenianScroll;
				req.header.msg_type = 0;
				std::snprintf(req.name, sizeof(req.name), "%s", "Gail");
				req.item_id = 875;
				send_game_packet(req);
			}
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(link_2)
			&& (player().m_construction_point >= 3000) && (m_game->on_game()->m_is_crusade_mode == false))
		{
			{
				hb::net::PacketRequestHeldenianScroll req{};
				req.header.msg_id = MsgId::RequestHeldenianScroll;
				req.header.msg_type = 0;
				std::snprintf(req.name, sizeof(req.name), "%s", "Gail");
				req.item_id = 876;
				send_game_packet(req);
			}
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(link_3)
			&& (player().m_construction_point >= 1500) && (m_game->on_game()->m_is_crusade_mode == false))
		{
			{
				hb::net::PacketRequestHeldenianScroll req{};
				req.header.msg_id = MsgId::RequestHeldenianScroll;
				req.header.msg_type = 0;
				std::snprintf(req.name, sizeof(req.name), "%s", "Gail");
				req.item_id = 877;
				send_game_packet(req);
			}
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(link_4)
			&& (player().m_construction_point >= 3000) && (m_game->on_game()->m_is_crusade_mode == false))
		{
			{
				hb::net::PacketRequestHeldenianScroll req{};
				req.header.msg_id = MsgId::RequestHeldenianScroll;
				req.header.msg_type = 0;
				std::snprintf(req.name, sizeof(req.name), "%s", "Gail");
				req.item_id = 878;
				send_game_packet(req);
			}
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(link_5)
			&& (player().m_construction_point >= 4000) && (m_game->on_game()->m_is_crusade_mode == false))
		{
			{
				hb::net::PacketRequestHeldenianScroll req{};
				req.header.msg_id = MsgId::RequestHeldenianScroll;
				req.header.msg_type = 0;
				std::snprintf(req.name, sizeof(req.name), "%s", "Gail");
				req.item_id = 879;
				send_game_packet(req);
			}
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(link_6)
			&& (player().m_construction_point >= 3000) && (m_game->on_game()->m_is_crusade_mode == false))
		{
			{
				hb::net::PacketRequestHeldenianScroll req{};
				req.header.msg_id = MsgId::RequestHeldenianScroll;
				req.header.msg_type = 0;
				std::snprintf(req.name, sizeof(req.name), "%s", "Gail");
				req.item_id = 880;
				send_game_packet(req);
			}
			play_sound_effect('E', 14, 5);
		}
		break;

	case mode::take_flag:
		if (mouse_in(link_take_flag)
			&& (player().m_enemy_kill_count >= 3))
		{
			send_game_packet(hb::net::make_common_command(CommonType::ReqGetOccupyFlag, player().m_player_x, player().m_player_y));
			play_sound_effect('E', 14, 5);
		}
		break;

	case mode::tutelary_angel:
		if (mouse_in(link_angel_str)
			&& (m_game->on_game()->m_gizon_item_upgrade_left >= 5))
		{
			{
				hb::net::PacketRequestAngel req{};
				req.header.msg_id = MsgId::RequestAngel;
				req.header.msg_type = 0;
				std::snprintf(req.name, sizeof(req.name), "%s", "Gail");
				req.angel_id = 1;
				send_game_packet(req);
			}
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(link_angel_dex)
			&& (m_game->on_game()->m_gizon_item_upgrade_left >= 5))
		{
			{
				hb::net::PacketRequestAngel req{};
				req.header.msg_id = MsgId::RequestAngel;
				req.header.msg_type = 0;
				std::snprintf(req.name, sizeof(req.name), "%s", "Gail");
				req.angel_id = 2;
				send_game_packet(req);
			}
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(link_angel_int)
			&& (m_game->on_game()->m_gizon_item_upgrade_left >= 5))
		{
			{
				hb::net::PacketRequestAngel req{};
				req.header.msg_id = MsgId::RequestAngel;
				req.header.msg_type = 0;
				std::snprintf(req.name, sizeof(req.name), "%s", "Gail");
				req.angel_id = 3;
				send_game_packet(req);
			}
			play_sound_effect('E', 14, 5);
		}
		if (mouse_in(link_angel_mag)
			&& (m_game->on_game()->m_gizon_item_upgrade_left >= 5))
		{
			{
				hb::net::PacketRequestAngel req{};
				req.header.msg_id = MsgId::RequestAngel;
				req.header.msg_type = 0;
				std::snprintf(req.name, sizeof(req.name), "%s", "Gail");
				req.angel_id = 4;
				send_game_packet(req);
			}
			play_sound_effect('E', 14, 5);
		}
		break;
	}
	return false;
}
