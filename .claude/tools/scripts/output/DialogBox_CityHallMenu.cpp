#include "DialogBox_CityHallMenu.h"
#include "Game.h"
#include "TeleportManager.h"
#include "lan_eng.h"
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
DialogBox_CityHallMenu::DialogBox_CityHallMenu(CGame* game)
	: IDialogBox(DialogBoxId::CityHallMenu, game)
{
	set_default_rect(497 , 57 , 258, 339);
}

void DialogBox_CityHallMenu::on_draw()
{
	short sX = m_x;
	short sY = m_y;
	short size_x = m_size_x;

	m_game->draw_new_dialog_box(InterfaceNdGame2, sX, sY, 2);
	m_game->draw_new_dialog_box(InterfaceNdText, sX, sY, 18);

	switch (m_mode)
	{
	case mode::main_menu:              DrawMode0_MainMenu(sX, sY, size_x); break;
	case mode::citizenship_warning:    DrawMode1_CitizenshipWarning(sX, sY, size_x); break;
	case mode::offering_citizenship:   DrawMode2_OfferingCitizenship(sX, sY, size_x); break;
	case mode::citizenship_success:    DrawMode3_CitizenshipSuccess(sX, sY, size_x); break;
	case mode::citizenship_failed:     DrawMode4_CitizenshipFailed(sX, sY, size_x); break;
	case mode::reward_gold:            DrawMode5_RewardGold(sX, sY, size_x); break;
	case mode::hero_items:             DrawMode7_HeroItems(sX, sY, size_x); break;
	case mode::cancel_quest:           DrawMode8_CancelQuest(sX, sY, size_x); break;
	case mode::change_play_mode:       DrawMode9_ChangePlayMode(sX, sY, size_x); break;
	case mode::teleport_menu:          DrawMode10_TeleportMenu(sX, sY, size_x); break;
	case mode::hero_item_confirm:      DrawMode11_HeroItemConfirm(sX, sY, size_x); break;
	}
}

void DialogBox_CityHallMenu::DrawMode0_MainMenu(short sX, short sY, short size_x)
{
	// Citizenship request
	if (player().m_citizen == false)
	{
		if (mouse_in(link_citizenship))
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 70, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU1, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
		else
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 70, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU1, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
	}
	else
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 70, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU1, hb::shared::text::TextStyle::from_color(GameColors::UIDisabled), hb::shared::text::Align::TopCenter);

	// Reward gold
	if (player().m_reward_gold > 0)
	{
		if (mouse_in(link_reward_gold))
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 95, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU4, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
		else
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 95, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU4, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
	}
	else
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 95, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU4, hb::shared::text::TextStyle::from_color(GameColors::UIDisabled), hb::shared::text::Align::TopCenter);

	// Hero's Items
	if ((player().m_enemy_kill_count >= 100) && (player().m_contribution >= 10))
	{
		if (mouse_in(link_hero_items))
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 120, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU8, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
		else
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 120, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU8, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
	}
	else
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 120, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU8, hb::shared::text::TextStyle::from_color(GameColors::UIDisabled), hb::shared::text::Align::TopCenter);

	// Cancel quest
	if (m_game->on_game()->m_quest.quest_type != 0)
	{
		if (mouse_in(link_cancel_quest))
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 145, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU11, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
		else
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 145, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU11, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
	}
	else
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 145, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU11, hb::shared::text::TextStyle::from_color(GameColors::UIDisabled), hb::shared::text::Align::TopCenter);

	// Change playmode
	if ((m_game->on_game()->m_is_crusade_mode == false) && player().m_citizen && (player().m_pk_count == 0))
	{
		if (player().m_hunter == true)
		{
			if (mouse_in(link_change_playmode))
				hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 170, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU56, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
			else
				hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 170, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU56, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
		}
		else if (player().m_level < 100)
		{
			if (mouse_in(link_change_playmode))
				hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 170, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU56, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
			else
				hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 170, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU56, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
		}
		else
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 170, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU56, hb::shared::text::TextStyle::from_color(GameColors::UIDisabled), hb::shared::text::Align::TopCenter);
	}
	else
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 170, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU56, hb::shared::text::TextStyle::from_color(GameColors::UIDisabled), hb::shared::text::Align::TopCenter);

	// Teleport menu
	if ((m_game->on_game()->m_is_crusade_mode == false) && player().m_citizen && (player().m_pk_count == 0))
	{
		if (mouse_in(link_teleport))
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 195, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU69, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
		else
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 195, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU69, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
	}
	else
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 195, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU69, hb::shared::text::TextStyle::from_color(GameColors::UIDisabled), hb::shared::text::Align::TopCenter);

	// Change crusade role
	if (m_game->on_game()->m_is_crusade_mode && player().m_citizen)
	{
		if (mouse_in(link_crusade_role))
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 220, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU14, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
		else
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 220, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU14, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
	}
	else
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 220, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU14, hb::shared::text::TextStyle::from_color(GameColors::UIDisabled), hb::shared::text::Align::TopCenter);

	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 270, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU17, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
}

void DialogBox_CityHallMenu::DrawMode1_CitizenshipWarning(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 80, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU18, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 95, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU19, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 110, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU20, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 125, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU21, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 140, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU22, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 155, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU23, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 170, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU24, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 200, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU25, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 215, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU26, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 230, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU27, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);

	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 19);
	else
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 18);

	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 3);
	else
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 2);
}

void DialogBox_CityHallMenu::DrawMode2_OfferingCitizenship(short sX, short sY, short size_x)
{
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 140, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU28, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
}

void DialogBox_CityHallMenu::DrawMode3_CitizenshipSuccess(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 140, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU29, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);

	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
	else
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
}

void DialogBox_CityHallMenu::DrawMode4_CitizenshipFailed(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 80, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU30, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 100, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU31, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 115, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU32, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);

	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
	else
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
}

void DialogBox_CityHallMenu::DrawMode5_RewardGold(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	std::string txt;

	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 125, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU33, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	txt = std::format(DRAW_DIALOGBOX_CITYHALL_MENU34, player().m_reward_gold);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 140, (sX + size_x) - (sX), 15, txt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 155, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU35, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);

	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 19);
	else
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 18);

	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 3);
	else
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 2);
}

void DialogBox_CityHallMenu::DrawMode7_HeroItems(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 60, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU46, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);

	// Hero's Cape (EK 300)
	if (player().m_enemy_kill_count >= 300)
	{
		if ((mouse_x > sX + 35) && (mouse_x < sX + 220) && (mouse_y > sY + 95) && (mouse_y < sY + 110))
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 95, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU47, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
		else
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 95, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU47, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
	}
	else
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 95, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU47, hb::shared::text::TextStyle::from_color(GameColors::UIDisabled), hb::shared::text::Align::TopCenter);

	// Hero's Helm (EK 150 - Contrib 20)
	if ((player().m_enemy_kill_count >= 150) && (player().m_contribution >= 20))
	{
		if ((mouse_x > sX + 35) && (mouse_x < sX + 220) && (mouse_y > sY + 125) && (mouse_y < sY + 140))
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 125, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU48, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
		else
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 125, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU48, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
	}
	else
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 125, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU48, hb::shared::text::TextStyle::from_color(GameColors::UIDisabled), hb::shared::text::Align::TopCenter);

	// Hero's Cap (EK 100 - Contrib 20)
	if ((player().m_enemy_kill_count >= 100) && (player().m_contribution >= 20))
	{
		if ((mouse_x > sX + 35) && (mouse_x < sX + 220) && (mouse_y > sY + 155) && (mouse_y < sY + 170))
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 155, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU49, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
		else
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 155, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU49, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
	}
	else
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 155, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU49, hb::shared::text::TextStyle::from_color(GameColors::UIDisabled), hb::shared::text::Align::TopCenter);

	// Hero's Armor (EK 300 - Contrib 30)
	if ((player().m_enemy_kill_count >= 300) && (player().m_contribution >= 30))
	{
		if ((mouse_x > sX + 35) && (mouse_x < sX + 220) && (mouse_y > sY + 185) && (mouse_y < sY + 200))
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 185, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU50, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
		else
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 185, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU50, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
	}
	else
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 185, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU50, hb::shared::text::TextStyle::from_color(GameColors::UIDisabled), hb::shared::text::Align::TopCenter);

	// Hero's Robe (EK 200 - Contrib 20)
	if ((player().m_enemy_kill_count >= 200) && (player().m_contribution >= 20))
	{
		if ((mouse_x > sX + 35) && (mouse_x < sX + 220) && (mouse_y > sY + 215) && (mouse_y < sY + 230))
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 215, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU51, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
		else
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 215, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU51, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
	}
	else
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 215, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU51, hb::shared::text::TextStyle::from_color(GameColors::UIDisabled), hb::shared::text::Align::TopCenter);

	// Hero's Hauberk (EK 100 - Contrib 10)
	if ((player().m_enemy_kill_count >= 100) && (player().m_contribution >= 10))
	{
		if ((mouse_x > sX + 35) && (mouse_x < sX + 220) && (mouse_y > sY + 245) && (mouse_y < sY + 260))
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 245, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU52, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
		else
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 245, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU52, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
	}
	else
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 245, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU52, hb::shared::text::TextStyle::from_color(GameColors::UIDisabled), hb::shared::text::Align::TopCenter);

	// Hero's Leggings (EK 150 - Contrib 15)
	if ((player().m_enemy_kill_count >= 150) && (player().m_contribution >= 15))
	{
		if ((mouse_x > sX + 35) && (mouse_x < sX + 220) && (mouse_y > sY + 275) && (mouse_y < sY + 290))
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 275, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU53, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
		else
			hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 275, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU53, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
	}
	else
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 275, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU53, hb::shared::text::TextStyle::from_color(GameColors::UIDisabled), hb::shared::text::Align::TopCenter);
}

void DialogBox_CityHallMenu::DrawMode8_CancelQuest(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 125, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU54, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 140, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU55, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);

	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 19);
	else
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 18);

	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 3);
	else
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 2);
}

void DialogBox_CityHallMenu::DrawMode9_ChangePlayMode(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	if (player().m_hunter)
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 53, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU57, hb::shared::text::TextStyle::from_color(GameColors::UIYellow), hb::shared::text::Align::TopCenter);
	else
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 53, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU58, hb::shared::text::TextStyle::from_color(GameColors::UIYellow), hb::shared::text::Align::TopCenter);

	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 78, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU59, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text(GameFont::Default, sX + 35, sY + 108, DRAW_DIALOGBOX_CITYHALL_MENU60, hb::shared::text::TextStyle::from_color(GameColors::UIOrange));
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 125, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU61, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 140, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU62, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 155, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU63, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text(GameFont::Default, sX + 35, sY + 177, DRAW_DIALOGBOX_CITYHALL_MENU64, hb::shared::text::TextStyle::from_color(GameColors::UIOrange));
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 194, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU65, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 209, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU66, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 224, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU67, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 252, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU68, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);

	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 19);
	else
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 18);

	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 3);
	else
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 2);
}

void DialogBox_CityHallMenu::DrawMode10_TeleportMenu(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	char mapNameBuf[120];
	std::string teleportBuf;

	if (teleport_manager::get().get_map_count() > 0)
	{
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 50, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU69, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 80, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU70, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 95, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU71, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 110, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU72, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
		hb::shared::text::draw_text(GameFont::Default, sX + 35, sY + 250, DRAW_DIALOGBOX_CITYHALL_MENU72_1, hb::shared::text::TextStyle::with_shadow(GameColors::UILabel));

		for (int i = 0; i < teleport_manager::get().get_map_count(); i++)
		{
			std::memset(mapNameBuf, 0, sizeof(mapNameBuf));
			m_game->get_official_map_name(teleport_manager::get().get_list()[i].mapname.c_str(), mapNameBuf);
			teleportBuf = std::format(DRAW_DIALOGBOX_CITYHALL_MENU77, mapNameBuf, teleport_manager::get().get_list()[i].cost);

			if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + 130 + i * 15) && (mouse_y <= sY + 144 + i * 15))
				hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 130 + i * 15, (sX + size_x) - (sX), 15, teleportBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
			else
				hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 130 + i * 15, (sX + size_x) - (sX), 15, teleportBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIMenuHighlight), hb::shared::text::Align::TopCenter);
		}
	}
	else if (teleport_manager::get().get_map_count() == -1)
	{
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 125, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU73, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 150, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU74, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 175, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU75, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	}
	else
	{
		hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 175, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU76, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	}
}

void DialogBox_CityHallMenu::DrawMode11_HeroItemConfirm(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 125, (sX + size_x - 1) - (sX), 15, m_cTakeHeroItemName.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX + 1, sY + 125, (sX + size_x) - (sX + 1), 15, m_cTakeHeroItemName.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
	hb::shared::text::draw_text_aligned(GameFont::Default, sX, sY + 260, (sX + size_x) - (sX), 15, DRAW_DIALOGBOX_CITYHALL_MENU46A, hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);

	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 19);
	else
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 18);

	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 3);
	else
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 2);
}

bool DialogBox_CityHallMenu::on_click()
{
	short sX = m_x;
	short sY = m_y;

	switch (m_mode)
	{
	case mode::main_menu:            return on_click_mode0(sX, sY);
	case mode::citizenship_warning:  return on_click_mode1(sX, sY);
	case mode::citizenship_success:
	case mode::citizenship_failed:   return OnClickMode3_4(sX, sY);
	case mode::reward_gold:          return on_click_mode5(sX, sY);
	case mode::hero_items:           return on_click_mode7(sX, sY);
	case mode::cancel_quest:         return on_click_mode8(sX, sY);
	case mode::change_play_mode:     return on_click_mode9(sX, sY);
	case mode::teleport_menu:        return on_click_mode10(sX, sY);
	case mode::hero_item_confirm:    return on_click_mode11(sX, sY);
	}
	return false;
}

bool DialogBox_CityHallMenu::on_click_mode0(short sX, short sY)
{
	// Citizenship request
	if (mouse_in(link_citizenship))
	{
		if (player().m_citizen == true) return false;
		m_mode = mode::citizenship_warning;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// Reward gold
	if (mouse_in(link_reward_gold))
	{
		if (player().m_reward_gold <= 0) return false;
		m_mode = mode::reward_gold;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// Hero items
	if (mouse_in(link_hero_items))
	{
		if (player().m_enemy_kill_count < 100) return false;
		m_mode = mode::hero_items;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// Cancel quest
	if (mouse_in(link_cancel_quest))
	{
		if (m_game->on_game()->m_quest.quest_type == 0) return false;
		m_mode = mode::cancel_quest;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// Change playmode
	if (mouse_in(link_change_playmode))
	{
		if (m_game->on_game()->m_is_crusade_mode) return false;
		if (player().m_pk_count != 0) return false;
		if (player().m_citizen == false) return false;
		if ((player().m_level > 100) && (player().m_hunter == false)) return false;
		m_mode = mode::change_play_mode;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// Teleport menu
	if (mouse_in(link_teleport))
	{
		m_mode = mode::teleport_menu;
		teleport_manager::get().set_map_count(-1);
		{
			hb::net::PacketRequestName20 req{};
			req.header.msg_id = ClientMsgId::RequestTeleportList;
			req.header.msg_type = 0;
			std::snprintf(req.name, sizeof(req.name), "%s", "William");
			m_game->send_game_packet(req);
		}
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// Crusade job
	if (mouse_in(link_crusade_role))
	{
		if (m_game->on_game()->m_is_crusade_mode == false) return false;
		m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::CrusadeJob, 1, 0, 0);
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_CityHallMenu::on_click_mode1(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// Yes button
	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		{
			hb::net::PacketRequestHeaderOnly req{};
			req.header.msg_id = MsgId::RequestCivilRight;
			req.header.msg_type = MsgType::Confirm;
			m_game->send_game_packet(req);
		}
		m_mode = mode::offering_citizenship;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// No button
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		m_mode = mode::main_menu;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_CityHallMenu::OnClickMode3_4(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// OK button
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		m_mode = mode::main_menu;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}
	return false;
}

bool DialogBox_CityHallMenu::on_click_mode5(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// Yes button
	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		m_game->send_game_packet(hb::net::make_common_command(CommonType::ReqGetRewardMoney, m_game->m_player->m_player_x, m_game->m_player->m_player_y));
		m_mode = mode::main_menu;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// No button
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		m_mode = mode::main_menu;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_CityHallMenu::on_click_mode7(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	int req_hero_item_id = 0;

	if (m_game->m_cur_focus < 1 || m_game->m_char_list[m_game->m_cur_focus - 1] == nullptr)
		return false;

	// Hero's Cape
	if ((mouse_x >= sX + 35) && (mouse_x <= sX + 220) && (mouse_y >= sY + 95) && (mouse_y <= sY + 110))
	{
		if (player().m_aresden == true) req_hero_item_id = 400;
		else req_hero_item_id = 401;
		m_cTakeHeroItemName = DRAW_DIALOGBOX_CITYHALL_MENU47;
		m_mode = mode::hero_item_confirm;
		m_hero_item_id = req_hero_item_id;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// Hero's Helm
	if ((mouse_x >= sX + 35) && (mouse_x <= sX + 220) && (mouse_y >= sY + 125) && (mouse_y <= sY + 140))
	{
		if ((player().m_aresden == true) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 1)) req_hero_item_id = 403;
		if ((player().m_aresden == true) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 2)) req_hero_item_id = 404;
		if ((player().m_aresden == false) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 1)) req_hero_item_id = 405;
		if ((player().m_aresden == false) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 2)) req_hero_item_id = 406;
		m_cTakeHeroItemName = DRAW_DIALOGBOX_CITYHALL_MENU48;
		m_mode = mode::hero_item_confirm;
		m_hero_item_id = req_hero_item_id;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// Hero's Cap
	if ((mouse_x >= sX + 35) && (mouse_x <= sX + 220) && (mouse_y >= sY + 155) && (mouse_y <= sY + 170))
	{
		if ((player().m_aresden == true) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 1)) req_hero_item_id = 407;
		if ((player().m_aresden == true) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 2)) req_hero_item_id = 408;
		if ((player().m_aresden == false) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 1)) req_hero_item_id = 409;
		if ((player().m_aresden == false) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 2)) req_hero_item_id = 410;
		m_cTakeHeroItemName = DRAW_DIALOGBOX_CITYHALL_MENU49;
		m_mode = mode::hero_item_confirm;
		m_hero_item_id = req_hero_item_id;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// Hero's Armor
	if ((mouse_x >= sX + 35) && (mouse_x <= sX + 220) && (mouse_y >= sY + 185) && (mouse_y <= sY + 200))
	{
		if ((player().m_aresden == true) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 1)) req_hero_item_id = 411;
		if ((player().m_aresden == true) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 2)) req_hero_item_id = 412;
		if ((player().m_aresden == false) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 1)) req_hero_item_id = 413;
		if ((player().m_aresden == false) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 2)) req_hero_item_id = 414;
		m_cTakeHeroItemName = DRAW_DIALOGBOX_CITYHALL_MENU50;
		m_mode = mode::hero_item_confirm;
		m_hero_item_id = req_hero_item_id;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// Hero's Robe
	if ((mouse_x >= sX + 35) && (mouse_x <= sX + 220) && (mouse_y >= sY + 215) && (mouse_y <= sY + 230))
	{
		if ((player().m_aresden == true) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 1)) req_hero_item_id = 415;
		if ((player().m_aresden == true) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 2)) req_hero_item_id = 416;
		if ((player().m_aresden == false) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 1)) req_hero_item_id = 417;
		if ((player().m_aresden == false) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 2)) req_hero_item_id = 418;
		m_cTakeHeroItemName = DRAW_DIALOGBOX_CITYHALL_MENU51;
		m_mode = mode::hero_item_confirm;
		m_hero_item_id = req_hero_item_id;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// Hero's Hauberk
	if ((mouse_x >= sX + 35) && (mouse_x <= sX + 220) && (mouse_y >= sY + 245) && (mouse_y <= sY + 260))
	{
		if ((player().m_aresden == true) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 1)) req_hero_item_id = 419;
		if ((player().m_aresden == true) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 2)) req_hero_item_id = 420;
		if ((player().m_aresden == false) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 1)) req_hero_item_id = 421;
		if ((player().m_aresden == false) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 2)) req_hero_item_id = 422;
		m_cTakeHeroItemName = DRAW_DIALOGBOX_CITYHALL_MENU52;
		m_mode = mode::hero_item_confirm;
		m_hero_item_id = req_hero_item_id;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// Hero's Leggings
	if ((mouse_x >= sX + 35) && (mouse_x <= sX + 220) && (mouse_y >= sY + 275) && (mouse_y <= sY + 290))
	{
		if ((player().m_aresden == true) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 1)) req_hero_item_id = 423;
		if ((player().m_aresden == true) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 2)) req_hero_item_id = 424;
		if ((player().m_aresden == false) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 1)) req_hero_item_id = 425;
		if ((player().m_aresden == false) && (m_game->m_char_list[m_game->m_cur_focus - 1]->m_sex == 2)) req_hero_item_id = 426;
		m_cTakeHeroItemName = DRAW_DIALOGBOX_CITYHALL_MENU53;
		m_mode = mode::hero_item_confirm;
		m_hero_item_id = req_hero_item_id;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_CityHallMenu::on_click_mode8(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// Yes button
	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		m_game->send_game_packet(hb::net::make_common_command(CommonType::RequestCancelQuest, m_game->m_player->m_player_x, m_game->m_player->m_player_y));
		m_mode = mode::main_menu;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// No button
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		m_mode = mode::main_menu;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_CityHallMenu::on_click_mode9(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// Yes button
	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		m_game->send_game_packet(hb::net::make_common_command(CommonType::RequestHuntMode, m_game->m_player->m_player_x, m_game->m_player->m_player_y));
		m_mode = mode::main_menu;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// No button
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		m_mode = mode::main_menu;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_CityHallMenu::on_click_mode10(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	if (teleport_manager::get().get_map_count() > 0)
	{
		for (int i = 0; i < teleport_manager::get().get_map_count(); i++)
		{
			if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + 130 + i * 15) && (mouse_y <= sY + 144 + i * 15))
			{
				{
				hb::net::PacketRequestTeleportId req{};
				req.header.msg_id = ClientMsgId::RequestChargedTeleport;
				req.header.msg_type = 0;
				req.teleport_id = teleport_manager::get().get_list()[i].index;
				m_game->send_game_packet(req);
			}
				m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::CityHallMenu);
				return true;
			}
		}
	}
	return false;
}

bool DialogBox_CityHallMenu::on_click_mode11(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// Yes button
	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		{
			auto pkt = hb::net::make_common_command(CommonType::ReqGetHeroMantle, m_game->m_player->m_player_x, m_game->m_player->m_player_y);
			pkt.v1 = m_hero_item_id;
			m_game->send_game_packet(pkt);
		}
		m_mode = mode::main_menu;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// No button
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		m_mode = mode::hero_items;
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	return false;
}
