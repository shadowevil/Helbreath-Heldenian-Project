#include "DialogBox_Party.h"
#include "Game.h"
#include "PacketSendHelpers.h"

#include "lan_eng.h"
#include <format>
#include <string>
#include "IInput.h"
#include "Screen_OnGame.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
DialogBox_Party::DialogBox_Party(CGame* game)
	: IDialogBox(DialogBoxId::Party, game)
{
	set_default_rect(0 , 0 , 258, 339);
	m_can_close_on_right_click = false;
}

void DialogBox_Party::on_draw()
{
	short sX = m_x;
	short sY = m_y;
	short size_x = m_size_x;

	m_game->draw_new_dialog_box(InterfaceNdGame2, sX, sY, 0);
	m_game->draw_new_dialog_box(InterfaceNdText, sX, sY, 3);

	switch (m_mode) {
	case mode::main_menu:
		if (m_game->on_game()->m_party_status == 0) {
			if (mouse_in(link_create))
				put_aligned_string(sX, sX + size_x, sY + 85, DRAW_DIALOGBOX_PARTY1, GameColors::UIWhite);
			else
				put_aligned_string(sX, sX + size_x, sY + 85, DRAW_DIALOGBOX_PARTY1, GameColors::UIMagicBlue);
		}
		else {
			put_aligned_string(sX, sX + size_x, sY + 85, DRAW_DIALOGBOX_PARTY1, GameColors::UIDisabled);
		}

		if (m_game->on_game()->m_party_status != 0) {
			if (mouse_in(link_leave))
				put_aligned_string(sX, sX + size_x, sY + 105, DRAW_DIALOGBOX_PARTY4, GameColors::UIWhite);
			else
				put_aligned_string(sX, sX + size_x, sY + 105, DRAW_DIALOGBOX_PARTY4, GameColors::UIMagicBlue);
		}
		else {
			put_aligned_string(sX, sX + size_x, sY + 105, DRAW_DIALOGBOX_PARTY4, GameColors::UIDisabled);
		}

		if (m_game->on_game()->m_party_status != 0) {
			if (mouse_in(link_members))
				put_aligned_string(sX, sX + size_x, sY + 125, DRAW_DIALOGBOX_PARTY7, GameColors::UIWhite);
			else
				put_aligned_string(sX, sX + size_x, sY + 125, DRAW_DIALOGBOX_PARTY7, GameColors::UIMagicBlue);
		}
		else {
			put_aligned_string(sX, sX + size_x, sY + 125, DRAW_DIALOGBOX_PARTY7, GameColors::UIDisabled);
		}

		switch (m_game->on_game()->m_party_status) {
		case 0:
			put_aligned_string(sX, sX + size_x, sY + 155, DRAW_DIALOGBOX_PARTY10);
			put_aligned_string(sX, sX + size_x, sY + 170, DRAW_DIALOGBOX_PARTY11);
			put_aligned_string(sX, sX + size_x, sY + 185, DRAW_DIALOGBOX_PARTY12);
			break;
		case 1:
		case 2:
			put_aligned_string(sX, sX + size_x, sY + 155, DRAW_DIALOGBOX_PARTY13);
			put_aligned_string(sX, sX + size_x, sY + 170, DRAW_DIALOGBOX_PARTY14);
			put_aligned_string(sX, sX + size_x, sY + 185, DRAW_DIALOGBOX_PARTY15);
			break;
		}

		if (mouse_in(btn_right))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;

	case mode::invited: {
		std::string partyBuf;
		partyBuf = std::format(DRAW_DIALOGBOX_PARTY16, m_leader_name);
		put_aligned_string(sX, sX + size_x, sY + 95, partyBuf.c_str());
		put_aligned_string(sX, sX + size_x, sY + 110, DRAW_DIALOGBOX_PARTY17);
		put_aligned_string(sX, sX + size_x, sY + 125, DRAW_DIALOGBOX_PARTY18);
		put_aligned_string(sX, sX + size_x, sY + 140, DRAW_DIALOGBOX_PARTY19);
		put_aligned_string(sX, sX + size_x, sY + 155, DRAW_DIALOGBOX_PARTY20);
		put_aligned_string(sX, sX + size_x, sY + 175, DRAW_DIALOGBOX_PARTY21);

		if (mouse_in(btn_left))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 19);
		else
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 18);

		if (mouse_in(btn_right))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 3);
		else
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 2);
		break;
	}

	case mode::pointing:
		put_aligned_string(sX, sX + size_x, sY + 95, DRAW_DIALOGBOX_PARTY22);
		put_aligned_string(sX, sX + size_x, sY + 110, DRAW_DIALOGBOX_PARTY23);
		put_aligned_string(sX, sX + size_x, sY + 125, DRAW_DIALOGBOX_PARTY24);
		put_aligned_string(sX, sX + size_x, sY + 140, DRAW_DIALOGBOX_PARTY25);

		if (mouse_in(btn_right))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 17);
		else
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 16);
		break;

	case mode::join_requested: {
		std::string partyBuf;
		partyBuf = std::format(DRAW_DIALOGBOX_PARTY26, m_leader_name);
		put_aligned_string(sX, sX + size_x, sY + 95, partyBuf.c_str());
		put_aligned_string(sX, sX + size_x, sY + 110, DRAW_DIALOGBOX_PARTY27);
		put_aligned_string(sX, sX + size_x, sY + 125, DRAW_DIALOGBOX_PARTY28);
		put_aligned_string(sX, sX + size_x, sY + 140, DRAW_DIALOGBOX_PARTY29);
		put_aligned_string(sX, sX + size_x, sY + 155, DRAW_DIALOGBOX_PARTY30);

		if (mouse_in(btn_right))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 17);
		else
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 16);
		break;
	}

	case mode::member_list: {
		put_aligned_string(sX, sX + size_x, sY + 95, DRAW_DIALOGBOX_PARTY31);
		put_aligned_string(sX, sX + size_x, sY + 110, DRAW_DIALOGBOX_PARTY32);

		int nth = 0;
		for (int i = 0; i <= hb::shared::limits::MaxPartyMembers; i++) {
			if (m_name_list[i].name.size() != 0) {
				put_aligned_string(sX + 17, sX + 270, sY + 140 + 15 * nth, m_name_list[i].name.c_str(), GameColors::UILabel);
				nth++;
			}
		}

		if (mouse_in(btn_right))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;
	}

	case mode::leaving:
		put_aligned_string(sX, sX + size_x, sY + 95, DRAW_DIALOGBOX_PARTY33);
		put_aligned_string(sX, sX + size_x, sY + 110, DRAW_DIALOGBOX_PARTY34);
		break;

	case mode::withdrawn:
		put_aligned_string(sX, sX + size_x, sY + 95, DRAW_DIALOGBOX_PARTY35);
		if (mouse_in(btn_right))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;

	case mode::party_full:
		put_aligned_string(sX, sX + size_x, sY + 95, DRAW_DIALOGBOX_PARTY36);
		put_aligned_string(sX, sX + size_x, sY + 110, DRAW_DIALOGBOX_PARTY37);
		put_aligned_string(sX, sX + size_x, sY + 125, DRAW_DIALOGBOX_PARTY38);
		put_aligned_string(sX, sX + size_x, sY + 140, DRAW_DIALOGBOX_PARTY39);
		if (mouse_in(btn_right))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;

	case mode::failed:
		put_aligned_string(sX, sX + size_x, sY + 95, DRAW_DIALOGBOX_PARTY40);
		put_aligned_string(sX, sX + size_x, sY + 110, DRAW_DIALOGBOX_PARTY41);
		put_aligned_string(sX, sX + size_x, sY + 125, DRAW_DIALOGBOX_PARTY42);
		put_aligned_string(sX, sX + size_x, sY + 140, DRAW_DIALOGBOX_PARTY43);
		put_aligned_string(sX, sX + size_x, sY + 155, DRAW_DIALOGBOX_PARTY44);
		put_aligned_string(sX, sX + size_x, sY + 170, DRAW_DIALOGBOX_PARTY45);

		if (mouse_in(btn_right))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;

	case mode::already_in_party:
		put_aligned_string(sX, sX + size_x, sY + 95, DRAW_DIALOGBOX_PARTY46);
		put_aligned_string(sX, sX + size_x, sY + 110, DRAW_DIALOGBOX_PARTY47);
		put_aligned_string(sX, sX + size_x, sY + 125, DRAW_DIALOGBOX_PARTY48);
		put_aligned_string(sX, sX + size_x, sY + 140, DRAW_DIALOGBOX_PARTY49);
		put_aligned_string(sX, sX + size_x, sY + 155, DRAW_DIALOGBOX_PARTY50);
		put_aligned_string(sX, sX + size_x, sY + 170, DRAW_DIALOGBOX_PARTY51);

		if (mouse_in(btn_right))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;

	case mode::disbanded:
		put_aligned_string(sX, sX + size_x, sY + 95, DRAW_DIALOGBOX_PARTY52);
		put_aligned_string(sX, sX + size_x, sY + 110, DRAW_DIALOGBOX_PARTY53);
		put_aligned_string(sX, sX + size_x, sY + 125, DRAW_DIALOGBOX_PARTY54);
		if (mouse_in(btn_right))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;

	case mode::confirm_leave:
		put_aligned_string(sX, sX + size_x, sY + 95, DRAW_DIALOGBOX_PARTY55);
		if (mouse_in(btn_left))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 19);
		else
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 18);

		if (mouse_in(btn_right))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 3);
		else
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 2);
		break;
	}
}

bool DialogBox_Party::on_click()
{
	switch (m_mode) {
	case mode::main_menu:
		if (m_game->on_game()->m_party_status == 0) {
			if (mouse_in(link_create)) {
				m_mode = mode::pointing;
				m_game->on_game()->m_is_get_pointing_mode = true;
				m_game->on_game()->m_point_command_type = 200;
				play_sound_effect('E', 14, 5);
				return true;
			}
		}

		if (m_game->on_game()->m_party_status != 0) {
			if (mouse_in(link_leave)) {
				m_mode = mode::confirm_leave;
				play_sound_effect('E', 14, 5);
				return true;
			}
		}

		if (m_game->on_game()->m_party_status != 0) {
			if (mouse_in(link_members)) {
				{
					auto pkt = hb::net::make_common_command_str(CommonType::RequestJoinParty, player().m_player_x, player().m_player_y);
					pkt.v1 = 2;
					std::snprintf(pkt.text, sizeof(pkt.text), "%s", m_game->m_mc_name.c_str());
					send_game_packet(pkt);
				}
				m_mode = mode::member_list;
				play_sound_effect('E', 14, 5);
				return true;
			}
		}

		if (mouse_in(btn_right)) {
			m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Party);
			return true;
		}
		break;

	case mode::invited:
		if (mouse_in(btn_left)) {
			{
				auto pkt = hb::net::make_common_command_str(CommonType::RequestAcceptJoinParty, player().m_player_x, player().m_player_y);
				pkt.v1 = 1;
				std::snprintf(pkt.text, sizeof(pkt.text), "%s", m_leader_name);
				send_game_packet(pkt);
			}
			m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Party);
			play_sound_effect('E', 14, 5);
			return true;
		}

		if (mouse_in(btn_right)) {
			{
				auto pkt = hb::net::make_common_command_str(CommonType::RequestAcceptJoinParty, player().m_player_x, player().m_player_y);
				std::snprintf(pkt.text, sizeof(pkt.text), "%s", m_leader_name);
				send_game_packet(pkt);
			}
			m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Party);
			play_sound_effect('E', 14, 5);
			return true;
		}
		break;

	case mode::pointing:
		if (mouse_in(btn_right)) {
			m_mode = mode::main_menu;
			play_sound_effect('E', 14, 5);
			return true;
		}
		break;

	case mode::join_requested:
		if (mouse_in(btn_right)) {
			m_mode = mode::main_menu;
			{
				auto pkt = hb::net::make_common_command_str(CommonType::RequestAcceptJoinParty, player().m_player_x, player().m_player_y);
				pkt.v1 = 2;
				std::snprintf(pkt.text, sizeof(pkt.text), "%s", m_leader_name);
				send_game_packet(pkt);
			}
			m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Party);
			play_sound_effect('E', 14, 5);
			return true;
		}
		break;

	case mode::member_list:
	case mode::withdrawn:
	case mode::party_full:
	case mode::failed:
	case mode::already_in_party:
	case mode::disbanded:
		if (mouse_in(btn_right)) {
			m_mode = mode::main_menu;
			play_sound_effect('E', 14, 5);
			return true;
		}
		break;

	case mode::confirm_leave:
		if (mouse_in(btn_left)) {
			{
				auto pkt = hb::net::make_common_command_str(CommonType::RequestJoinParty, player().m_player_x, player().m_player_y);
				std::snprintf(pkt.text, sizeof(pkt.text), "%s", m_game->m_mc_name.c_str());
				send_game_packet(pkt);
			}
			m_mode = mode::leaving;
			play_sound_effect('E', 14, 5);
			return true;
		}

		if (mouse_in(btn_right)) {
			m_mode = mode::main_menu;
			play_sound_effect('E', 14, 5);
			return true;
		}
		break;

	default:
		break;
	}

	return false;
}

bool DialogBox_Party::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	m_mode = static_cast<mode>(type);
	auto* charDlg = get_dialog_box(DialogBoxId::CharacterInfo);
	if (charDlg) { m_x = charDlg->m_x + 20; m_y = charDlg->m_y + 20; }
	return true;
}

void DialogBox_Party::reset_members()
{
	for (int i = 0; i < hb::shared::limits::MaxPartyMembers; i++)
		m_members[i].status = 0;
}

bool DialogBox_Party::add_member_name(const char* name)
{
	for (int i = 0; i < hb::shared::limits::MaxPartyMembers; i++)
	{
		if (m_name_list[i].name.empty())
		{
			m_name_list[i].name.assign(name, strnlen(name, hb::shared::limits::CharNameLen));
			return true;
		}
	}
	return false;
}

void DialogBox_Party::set_name_list(int count, const char* data, int name_len)
{
	clear_name_list();
	for (int i = 0; i < count && i < hb::shared::limits::MaxPartyMembers; i++)
	{
		m_name_list[i].name.assign(data, strnlen(data, name_len));
		data += name_len;
	}
}

bool DialogBox_Party::remove_member_name(const char* name)
{
	for (int i = 0; i < hb::shared::limits::MaxPartyMembers; i++)
	{
		if (m_name_list[i].name == name)
		{
			m_name_list[i].name.clear();
			return true;
		}
	}
	return false;
}

void DialogBox_Party::clear_name_list()
{
	for (int i = 0; i <= hb::shared::limits::MaxPartyMembers; i++)
		m_name_list[i].name.clear();
}

bool DialogBox_Party::is_party_member(const std::string& name) const
{
	for (int i = 0; i < hb::shared::limits::MaxPartyMembers; i++)
	{
		if (m_name_list[i].name == name)
			return true;
	}
	return false;
}
