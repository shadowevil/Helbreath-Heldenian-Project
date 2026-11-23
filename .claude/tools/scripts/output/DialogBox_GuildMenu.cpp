#include "DialogBox_GuildMenu.h"
#include "Game.h"
#include "TextInputManager.h"
#include "TextFieldRenderer.h"
#include "lan_eng.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include <string>
#include "IInput.h"
#include "Packet/SharedPackets.h"
#include "PacketSendHelpers.h"
#include "Screen_OnGame.h"


using namespace hb::shared::net;
using namespace hb::client::sprite_id;
DialogBox_GuildMenu::DialogBox_GuildMenu(CGame* game)
	: IDialogBox(DialogBoxId::GuildMenu, game)
{
	set_default_rect(497 , 57 , 258, 339);
	m_can_close_on_right_click = true;
}

void DialogBox_GuildMenu::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;
	short size_x = m_size_x;

	m_game->draw_new_dialog_box(InterfaceNdGame2, sX, sY, 2);
	m_game->draw_new_dialog_box(InterfaceNdText, sX, sY, 19);

	switch (m_mode) {
	case mode::main_menu:
		DrawMode0_MainMenu(sX, sY, size_x);
		break;
	case mode::create_guild:
		DrawMode1_CreateGuild(sX, sY, size_x);
		break;
	case mode::creating:
		put_aligned_string(sX, sX + size_x, sY + 140, DRAW_DIALOGBOX_GUILDMENU19, GameColors::UILabel);
		break;
	case mode::guild_created:
		put_aligned_string(sX, sX + size_x, sY + 125, DRAW_DIALOGBOX_GUILDMENU20, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + 140, player().m_guild_name.c_str(), GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + 150, "____________________", GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + 165, DRAW_DIALOGBOX_GUILDMENU21, GameColors::UILabel);
		if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;
	case mode::create_failed:
		put_aligned_string(sX, sX + size_x, sY + 135, DRAW_DIALOGBOX_GUILDMENU22, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + 150, DRAW_DIALOGBOX_GUILDMENU23, GameColors::UILabel);
		if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;
	case mode::disband_confirm:
		DrawMode5_DisbandConfirm(sX, sY, size_x);
		break;
	case mode::disbanding:
		put_aligned_string(sX, sX + size_x, sY + 140, DRAW_DIALOGBOX_GUILDMENU29, GameColors::UILabel);
		break;
	case mode::disband_success:
		put_aligned_string(sX, sX + size_x, sY + 140, DRAW_DIALOGBOX_GUILDMENU30, GameColors::UILabel);
		if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;
	case mode::disband_failed:
		put_aligned_string(sX, sX + size_x, sY + 140, DRAW_DIALOGBOX_GUILDMENU31, GameColors::UILabel);
		if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;
	case mode::admission_ticket:
		DrawMode9_AdmissionTicket(sX, sY, size_x);
		break;
	case mode::admission_result:
		put_aligned_string(sX, sX + size_x, sY + 140, DRAW_DIALOGBOX_GUILDMENU37, GameColors::UILabel);
		if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;
	case mode::secession_ticket:
		DrawMode11_SecessionTicket(sX, sY, size_x);
		break;
	case mode::secession_result:
		put_aligned_string(sX, sX + size_x, sY + 140, DRAW_DIALOGBOX_GUILDMENU43, GameColors::UILabel);
		if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;
	case mode::fightzone_select:
		DrawMode13_FightzoneSelect(sX, sY, size_x);
		break;
	case mode::fightzone_reserved:
		put_aligned_string(sX, sX + size_x, sY + 130, DRAW_DIALOGBOX_GUILDMENU66, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + 145, DRAW_DIALOGBOX_GUILDMENU67, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + 160, DRAW_DIALOGBOX_GUILDMENU68, GameColors::UILabel);
		if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;
	case mode::fightzone_won:
		put_aligned_string(sX, sX + size_x, sY + 135, DRAW_DIALOGBOX_GUILDMENU69, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + 150, DRAW_DIALOGBOX_GUILDMENU70, GameColors::UILabel);
		if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;
	case mode::fightzone_lost:
		put_aligned_string(sX, sX + size_x, sY + 135, DRAW_DIALOGBOX_GUILDMENU71, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + 150, DRAW_DIALOGBOX_GUILDMENU72, GameColors::UILabel);
		if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;
	case mode::fightzone_draw:
		put_aligned_string(sX, sX + size_x, sY + 140, DRAW_DIALOGBOX_GUILDMENU73, GameColors::UILabel);
		if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;
	case mode::fightzone_waiting:
		put_aligned_string(sX, sX + size_x, sY + 140, DRAW_DIALOGBOX_GUILDMENU74, GameColors::UILabel);
		break;
	case mode::get_ticket_redirect:
		// send_command was here but on_draw is called every frame — move to one-shot transition
		m_mode = mode::main_menu;
		break;
	case mode::confirm_cancel:
		DrawMode20_ConfirmCancel(sX, sY, size_x);
		break;
	case mode::fightzone_denied:
		put_aligned_string(sX, sX + size_x, sY + ADJY + 95, DRAW_DIALOGBOX_GUILDMENU76, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + ADJY + 110, DRAW_DIALOGBOX_GUILDMENU77, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + ADJY + 135, DRAW_DIALOGBOX_GUILDMENU78, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + ADJY + 150, DRAW_DIALOGBOX_GUILDMENU79, GameColors::UILabel);
		put_aligned_string(sX, sX + size_x, sY + ADJY + 165, DRAW_DIALOGBOX_GUILDMENU80, GameColors::UILabel);
		if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;
	case mode::fightzone_canceled:
		put_aligned_string(sX, sX + size_x, sY + 140, DRAW_DIALOGBOX_GUILDMENU81, GameColors::UILabel);
		if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
		else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
		break;
	}
}

void DialogBox_GuildMenu::DrawMode0_MainMenu(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// Create new guild option
	if ((player().m_guild_rank == -1) && (player().m_charisma >= 20) && (player().m_level >= 20)) {
		if ((mouse_x > sX + ADJX + 80) && (mouse_x < sX + ADJX + 210) && (mouse_y > sY + ADJY + 63) && (mouse_y < sY + ADJY + 78))
			put_aligned_string(sX, sX + size_x, sY + ADJY + 65, DRAW_DIALOGBOX_GUILDMENU1, GameColors::UIWhite);
		else put_aligned_string(sX, sX + size_x, sY + ADJY + 65, DRAW_DIALOGBOX_GUILDMENU1, GameColors::UIMagicBlue);
	}
	else put_aligned_string(sX, sX + size_x, sY + ADJY + 65, DRAW_DIALOGBOX_GUILDMENU1, GameColors::UIDisabled);

	// Disband guild option
	if (player().m_guild_rank == 0) {
		if ((mouse_x > sX + ADJX + 72) && (mouse_x < sX + ADJX + 222) && (mouse_y > sY + ADJY + 82) && (mouse_y < sY + ADJY + 99))
			put_aligned_string(sX, sX + size_x, sY + ADJY + 85, DRAW_DIALOGBOX_GUILDMENU4, GameColors::UIWhite);
		else put_aligned_string(sX, sX + size_x, sY + ADJY + 85, DRAW_DIALOGBOX_GUILDMENU4, GameColors::UIMagicBlue);
	}
	else put_aligned_string(sX, sX + size_x, sY + ADJY + 85, DRAW_DIALOGBOX_GUILDMENU4, GameColors::UIDisabled);

	// Admission ticket option
	if ((mouse_x > sX + ADJX + 61) && (mouse_x < sX + ADJX + 226) && (mouse_y > sY + ADJY + 103) && (mouse_y < sY + ADJY + 120))
		put_aligned_string(sX, sX + size_x, sY + ADJY + 105, DRAW_DIALOGBOX_GUILDMENU7, GameColors::UIWhite);
	else put_aligned_string(sX, sX + size_x, sY + ADJY + 105, DRAW_DIALOGBOX_GUILDMENU7, GameColors::UIMagicBlue);

	// Secession ticket option
	if ((mouse_x > sX + ADJX + 60) && (mouse_x < sX + ADJX + 227) && (mouse_y > sY + ADJY + 123) && (mouse_y < sY + ADJY + 139))
		put_aligned_string(sX, sX + size_x, sY + ADJY + 125, DRAW_DIALOGBOX_GUILDMENU9, GameColors::UIWhite);
	else put_aligned_string(sX, sX + size_x, sY + ADJY + 125, DRAW_DIALOGBOX_GUILDMENU9, GameColors::UIMagicBlue);

	// Fightzone options
	if (player().m_guild_rank == 0 && m_game->on_game()->m_fightzone_number == 0) {
		if ((mouse_x > sX + ADJX + 72) && (mouse_x < sX + ADJX + 228) && (mouse_y > sY + ADJY + 143) && (mouse_y < sY + ADJY + 169))
			put_aligned_string(sX, sX + size_x, sY + ADJY + 145, DRAW_DIALOGBOX_GUILDMENU11, GameColors::UIWhite);
		else put_aligned_string(sX, sX + size_x, sY + ADJY + 145, DRAW_DIALOGBOX_GUILDMENU11, GameColors::UIMagicBlue);
	}
	else if (player().m_guild_rank == 0 && m_game->on_game()->m_fightzone_number > 0) {
		if ((mouse_x > sX + ADJX + 72) && (mouse_x < sX + ADJX + 216) && (mouse_y > sY + ADJY + 143) && (mouse_y < sY + ADJY + 169))
			put_aligned_string(sX, sX + size_x, sY + ADJY + 145, DRAW_DIALOGBOX_GUILDMENU13, GameColors::UIWhite);
		else put_aligned_string(sX, sX + size_x, sY + ADJY + 145, DRAW_DIALOGBOX_GUILDMENU13, GameColors::UIMagicBlue);
	}
	else if (m_game->on_game()->m_fightzone_number < 0) {
		put_aligned_string(sX, sX + size_x, sY + ADJY + 145, DRAW_DIALOGBOX_GUILDMENU13, GameColors::UIDisabled);
	}
	else put_aligned_string(sX, sX + size_x, sY + ADJY + 145, DRAW_DIALOGBOX_GUILDMENU11, GameColors::UIDisabled);

	put_aligned_string(sX, sX + size_x, sY + ADJY + 205, DRAW_DIALOGBOX_GUILDMENU17);
}

void DialogBox_GuildMenu::DrawMode1_CreateGuild(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	put_aligned_string(sX, sX + size_x, sY + 125, DRAW_DIALOGBOX_GUILDMENU18, GameColors::UILabel);
	put_string(sX + 75, sY + 150, "____________________", GameColors::UILabel);

	if (m_game->get_dialog_box_manager().get_top_id() != DialogBoxId::GuildMenu) {
		std::string masked(player().m_guild_name.size(), '*');
		hb::shared::text::draw_text(GameFont::Default, sX + 75, sY + 140, masked.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite));
	}

	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y)) {
		if ((player().m_guild_name == "NONE") || player().m_guild_name.empty()) {
			m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 24);
		}
		else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 25);
	}
	else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 24);

	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 17);
	else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 16);
}

void DialogBox_GuildMenu::DrawMode5_DisbandConfirm(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	put_aligned_string(sX, sX + size_x, sY + 90, DRAW_DIALOGBOX_GUILDMENU24);
	put_aligned_string(sX, sX + size_x, sY + 105, player().m_guild_name.c_str(), GameColors::UILabel);
	put_aligned_string(sX, sX + size_x, sY + 118, "____________________", GameColors::UIBlack);
	put_aligned_string(sX, sX + size_x, sY + 130, DRAW_DIALOGBOX_GUILDMENU25);
	put_aligned_string(sX, sX + size_x, sY + 145, DRAW_DIALOGBOX_GUILDMENU26);
	put_aligned_string(sX, sX + size_x, sY + 160, DRAW_DIALOGBOX_GUILDMENU27);
	put_aligned_string(sX, sX + size_x, sY + 175, DRAW_DIALOGBOX_GUILDMENU28, GameColors::UILabel);

	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 19);
	else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 18);

	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 3);
	else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 2);
}

void DialogBox_GuildMenu::DrawMode9_AdmissionTicket(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	CItem* cfg = m_game->get_item_config(hb::shared::item::ItemId::GuildAdmissionTicket);
	int price = cfg ? static_cast<int>(cfg->m_price) : 0;
	put_aligned_string(sX, sX + size_x, sY + ADJY + 60, DRAW_DIALOGBOX_GUILDMENU32);
	put_aligned_string(sX, sX + size_x, sY + ADJY + 75, std::format(DRAW_DIALOGBOX_GUILDMENU33, price).c_str());
	put_aligned_string(sX, sX + size_x, sY + ADJY + 90, DRAW_DIALOGBOX_GUILDMENU34);
	put_aligned_string(sX, sX + size_x, sY + ADJY + 105, DRAW_DIALOGBOX_GUILDMENU35);
	put_aligned_string(sX, sX + size_x, sY + ADJY + 130, DRAW_DIALOGBOX_GUILDMENU36);
	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 31);
	else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 30);
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 17);
	else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 16);
}

void DialogBox_GuildMenu::DrawMode11_SecessionTicket(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	CItem* cfg = m_game->get_item_config(hb::shared::item::ItemId::GuildSecessionTicket);
	int price = cfg ? static_cast<int>(cfg->m_price) : 0;
	put_aligned_string(sX, sX + size_x, sY + ADJY + 60, DRAW_DIALOGBOX_GUILDMENU38);
	put_aligned_string(sX, sX + size_x, sY + ADJY + 75, std::format(DRAW_DIALOGBOX_GUILDMENU39, price).c_str());
	put_aligned_string(sX, sX + size_x, sY + ADJY + 90, DRAW_DIALOGBOX_GUILDMENU40);
	put_aligned_string(sX, sX + size_x, sY + ADJY + 105, DRAW_DIALOGBOX_GUILDMENU41);
	put_aligned_string(sX, sX + size_x, sY + ADJY + 130, DRAW_DIALOGBOX_GUILDMENU42);
	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 31);
	else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 30);
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 17);
	else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 16);
}

void DialogBox_GuildMenu::DrawMode13_FightzoneSelect(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	put_aligned_string(sX, sX + size_x, sY + ADJY + 40, DRAW_DIALOGBOX_GUILDMENU44);
	put_aligned_string(sX, sX + size_x, sY + ADJY + 55, DRAW_DIALOGBOX_GUILDMENU45);
	put_aligned_string(sX, sX + size_x, sY + ADJY + 70, DRAW_DIALOGBOX_GUILDMENU46);
	put_aligned_string(sX, sX + size_x, sY + ADJY + 85, DRAW_DIALOGBOX_GUILDMENU47);
	put_aligned_string(sX, sX + size_x, sY + ADJY + 100, DRAW_DIALOGBOX_GUILDMENU48);
	put_aligned_string(sX, sX + size_x, sY + ADJY + 115, DRAW_DIALOGBOX_GUILDMENU49);
	put_aligned_string(sX, sX + size_x, sY + ADJY + 130, DRAW_DIALOGBOX_GUILDMENU50);

	// Fightzone buttons
	if ((mouse_x > sX + ADJX + 65) && (mouse_x < sX + ADJX + 137) && (mouse_y > sY + ADJY + 168) && (mouse_y < sY + ADJY + 185))
		put_string(sX + ADJX + 65 + 25 - 23, sY + ADJY + 170, DRAW_DIALOGBOX_GUILDMENU51, GameColors::UIWhite);
	else put_string(sX + ADJX + 65 + 25 - 23, sY + ADJY + 170, DRAW_DIALOGBOX_GUILDMENU51, GameColors::UIMagicBlue);

	if ((mouse_x > sX + ADJX + 150) && (mouse_x < sX + ADJX + 222) && (mouse_y > sY + ADJY + 168) && (mouse_y < sY + ADJY + 185))
		put_string(sX + ADJX + 150 + 25 - 23, sY + ADJY + 170, DRAW_DIALOGBOX_GUILDMENU53, GameColors::UIWhite);
	else put_string(sX + ADJX + 150 + 25 - 23, sY + ADJY + 170, DRAW_DIALOGBOX_GUILDMENU53, GameColors::UIMagicBlue);

	if ((mouse_x > sX + ADJX + 65) && (mouse_x < sX + ADJX + 137) && (mouse_y > sY + ADJY + 188) && (mouse_y < sY + ADJY + 205))
		put_string(sX + ADJX + 65 + 25 - 23, sY + ADJY + 190, DRAW_DIALOGBOX_GUILDMENU55, GameColors::UIWhite);
	else put_string(sX + ADJX + 65 + 25 - 23, sY + ADJY + 190, DRAW_DIALOGBOX_GUILDMENU55, GameColors::UIMagicBlue);

	if ((mouse_x > sX + ADJX + 150) && (mouse_x < sX + ADJX + 222) && (mouse_y > sY + ADJY + 188) && (mouse_y < sY + ADJY + 205))
		put_string(sX + ADJX + 150 + 25 - 23, sY + ADJY + 190, DRAW_DIALOGBOX_GUILDMENU57, GameColors::UIWhite);
	else put_string(sX + ADJX + 150 + 25 - 23, sY + ADJY + 190, DRAW_DIALOGBOX_GUILDMENU57, GameColors::UIMagicBlue);

	if ((mouse_x > sX + ADJX + 65) && (mouse_x < sX + ADJX + 137) && (mouse_y > sY + ADJY + 208) && (mouse_y < sY + ADJY + 225))
		put_string(sX + ADJX + 65 + 25 - 23, sY + ADJY + 210, DRAW_DIALOGBOX_GUILDMENU59, GameColors::UIWhite);
	else put_string(sX + ADJX + 65 + 25 - 23, sY + ADJY + 210, DRAW_DIALOGBOX_GUILDMENU59, GameColors::UIMagicBlue);

	if ((mouse_x > sX + ADJX + 150) && (mouse_x < sX + ADJX + 222) && (mouse_y > sY + ADJY + 208) && (mouse_y < sY + ADJY + 225))
		put_string(sX + ADJX + 150 + 25 - 23, sY + ADJY + 210, DRAW_DIALOGBOX_GUILDMENU61, GameColors::UIWhite);
	else put_string(sX + ADJX + 150 + 25 - 23, sY + ADJY + 210, DRAW_DIALOGBOX_GUILDMENU61, GameColors::UIMagicBlue);

	if ((mouse_x > sX + ADJX + 65) && (mouse_x < sX + ADJX + 137) && (mouse_y > sY + ADJY + 228) && (mouse_y < sY + ADJY + 245))
		put_string(sX + ADJX + 65 + 25 - 23, sY + ADJY + 230, DRAW_DIALOGBOX_GUILDMENU63, GameColors::UIWhite);
	else put_string(sX + ADJX + 65 + 25 - 23, sY + ADJY + 230, DRAW_DIALOGBOX_GUILDMENU63, GameColors::UIMagicBlue);

	if ((mouse_x > sX + ADJX + 150) && (mouse_x < sX + ADJX + 222) && (mouse_y > sY + ADJY + 228) && (mouse_y < sY + ADJY + 245))
		put_string(sX + ADJX + 150 + 25 - 23, sY + ADJY + 230, DRAW_DIALOGBOX_GUILDMENU65, GameColors::UIWhite);
	else put_string(sX + ADJX + 150 + 25 - 23, sY + ADJY + 230, DRAW_DIALOGBOX_GUILDMENU65, GameColors::UIMagicBlue);

	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 17);
	else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 16);
}

void DialogBox_GuildMenu::DrawMode20_ConfirmCancel(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	put_aligned_string(sX, sX + size_x, sY + 125, DRAW_DIALOGBOX_GUILDMENU75, GameColors::UILabel);
	put_string(sX + 75, sY + 150, "____________________", GameColors::UILabel);
	hb::shared::text::draw_text(GameFont::Default, sX + 75, sY + 140, player().m_guild_name.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite));
	if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 25);
	else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 24);
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 17);
	else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 16);
}

bool DialogBox_GuildMenu::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;

	switch (m_mode) {
	case mode::main_menu:
		return on_click_mode0(sX, sY);
	case mode::create_guild:
		return on_click_mode1(sX, sY);
	case mode::guild_created:
	case mode::create_failed:
	case mode::disband_success:
	case mode::disband_failed:
	case mode::admission_result:
	case mode::secession_result:
		return on_click_mode_ok_only(sX, sY);
	case mode::disband_confirm:
		return on_click_mode5(sX, sY);
	case mode::admission_ticket:
		return on_click_mode9(sX, sY);
	case mode::secession_ticket:
		return on_click_mode11(sX, sY);
	case mode::fightzone_select:
		return on_click_mode13(sX, sY);
	case mode::fightzone_reserved:
	case mode::fightzone_won:
	case mode::fightzone_lost:
	case mode::fightzone_draw:
	case mode::fightzone_denied:
	case mode::fightzone_canceled:
		return on_click_mode_ok_only(sX, sY);
	}
	return false;
}

bool DialogBox_GuildMenu::on_click_mode0(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// Create new guild
	if ((mouse_x > sX + ADJX + 80) && (mouse_x < sX + ADJX + 210) && (mouse_y > sY + ADJY + 63) && (mouse_y < sY + ADJY + 78)) {
		if (player().m_guild_rank != -1) return false;
		if (player().m_charisma < 20) return false;
		if (player().m_level < 20) return false;
		if (m_game->on_game()->m_is_crusade_mode) return false;
		text_input_manager::get().end_input();
		text_input_manager::get().start_input(sX + 75, sY + 140, 21, player().m_guild_name, false, hb::client::guild_name_allowed_chars);
		m_mode = mode::create_guild;
		play_sound_effect('E', 14, 5);
		return true;
	}

	// Disband guild
	if ((mouse_x > sX + ADJX + 72) && (mouse_x < sX + ADJX + 222) && (mouse_y > sY + ADJY + 82) && (mouse_y < sY + ADJY + 99)) {
		if (player().m_guild_rank != 0) return false;
		if (m_game->on_game()->m_is_crusade_mode) return false;
		m_mode = mode::disband_confirm;
		play_sound_effect('E', 14, 5);
		return true;
	}

	// Admission ticket
	if ((mouse_x > sX + ADJX + 61) && (mouse_x < sX + ADJX + 226) && (mouse_y > sY + ADJY + 103) && (mouse_y < sY + ADJY + 120)) {
		m_mode = mode::admission_ticket;
		play_sound_effect('E', 14, 5);
		return true;
	}

	// Secession ticket
	if ((mouse_x > sX + ADJX + 60) && (mouse_x < sX + ADJX + 227) && (mouse_y > sY + ADJY + 123) && (mouse_y < sY + ADJY + 139)) {
		m_mode = mode::secession_ticket;
		play_sound_effect('E', 14, 5);
		return true;
	}

	// Fightzone
	if (m_game->on_game()->m_fightzone_number < 0) return false;
	if ((mouse_x > sX + ADJX + 72) && (mouse_x < sX + ADJX + 228) && (mouse_y > sY + ADJY + 143) && (mouse_y < sY + ADJY + 169)) {
		if (player().m_guild_rank != 0) return false;
		if (m_game->on_game()->m_fightzone_number == 0)
			m_mode = mode::fightzone_select;
		else {
			m_mode = mode::get_ticket_redirect;
			send_game_packet(hb::net::make_common_command(CommonType::ReqGetOccupyFightZoneTicket, player().m_player_x, player().m_player_y));
		}
		play_sound_effect('E', 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_GuildMenu::on_click_mode1(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// Submit button
	if ((mouse_x >= sX + 30) && (mouse_x <= sX + 30 + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y)) {
		if (player().m_guild_name == "NONE") return false;
		if (player().m_guild_name.empty()) return false;
		{
			hb::net::PacketRequestGuildAction req{};
			req.header.msg_id = MsgId::request_create_new_guild;
			req.header.msg_type = MsgType::Confirm;
			std::snprintf(req.player, sizeof(req.player), "%s", player().m_player_name.c_str());
			std::snprintf(req.account, sizeof(req.account), "%s", m_game->m_account_name.c_str());
			std::snprintf(req.password, sizeof(req.password), "%s", m_game->m_account_password.c_str());
			std::snprintf(req.guild, sizeof(req.guild), "%s", player().m_guild_name.c_str());
			CMisc::replace_string(req.guild, ' ', '_');
			send_game_packet(req);
		}
		m_mode = mode::creating;
		text_input_manager::get().end_input();
		play_sound_effect('E', 14, 5);
		return true;
	}

	// Cancel button
	if ((mouse_x >= sX + 154) && (mouse_x <= sX + 154 + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y)) {
		m_mode = mode::main_menu;
		text_input_manager::get().end_input();
		play_sound_effect('E', 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_GuildMenu::on_click_mode5(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// Confirm disband
	if ((mouse_x >= sX + 30) && (mouse_x <= sX + 30 + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y)) {
		{
			hb::net::PacketRequestGuildAction req{};
			req.header.msg_id = MsgId::request_disband_guild;
			req.header.msg_type = MsgType::Confirm;
			std::snprintf(req.player, sizeof(req.player), "%s", player().m_player_name.c_str());
			std::snprintf(req.account, sizeof(req.account), "%s", m_game->m_account_name.c_str());
			std::snprintf(req.password, sizeof(req.password), "%s", m_game->m_account_password.c_str());
			std::snprintf(req.guild, sizeof(req.guild), "%s", player().m_guild_name.c_str());
			CMisc::replace_string(req.guild, ' ', '_');
			send_game_packet(req);
		}
		m_mode = mode::disbanding;
		play_sound_effect('E', 14, 5);
		return true;
	}

	// Cancel
	if ((mouse_x >= sX + 154) && (mouse_x <= sX + 154 + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y)) {
		m_mode = mode::main_menu;
		play_sound_effect('E', 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_GuildMenu::on_click_mode9(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	std::string temp;

	// Purchase admission ticket
	if ((mouse_x >= sX + 30) && (mouse_x <= sX + 30 + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y)) {
		temp = "GuildAdmissionTicket";
		{
			auto pkt = hb::net::make_common_command_str(CommonType::ReqPurchaseItem, player().m_player_x, player().m_player_y);
			pkt.v1 = 1;
			pkt.v2 = hb::shared::item::ItemId::GuildAdmissionTicket;
			std::snprintf(pkt.text, sizeof(pkt.text), "%s", temp.c_str());
			send_game_packet(pkt);
		}
		m_mode = mode::main_menu;
		play_sound_effect('E', 14, 5);
		return true;
	}

	// Cancel
	if ((mouse_x >= sX + 154) && (mouse_x <= sX + 154 + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y)) {
		m_mode = mode::main_menu;
		play_sound_effect('E', 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_GuildMenu::on_click_mode11(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	std::string temp;

	// Purchase secession ticket
	if ((mouse_x >= sX + 30) && (mouse_x <= sX + 30 + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y)) {
		temp = "GuildSecessionTicket";
		{
			auto pkt = hb::net::make_common_command_str(CommonType::ReqPurchaseItem, player().m_player_x, player().m_player_y);
			pkt.v1 = 1;
			pkt.v2 = hb::shared::item::ItemId::GuildSecessionTicket;
			std::snprintf(pkt.text, sizeof(pkt.text), "%s", temp.c_str());
			send_game_packet(pkt);
		}
		m_mode = mode::main_menu;
		play_sound_effect('E', 14, 5);
		return true;
	}

	// Cancel
	if ((mouse_x >= sX + 154) && (mouse_x <= sX + 154 + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y)) {
		m_mode = mode::main_menu;
		play_sound_effect('E', 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_GuildMenu::on_click_mode13(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// Fightzone 1
	if ((mouse_x > sX + ADJX + 65) && (mouse_x < sX + ADJX + 137) && (mouse_y > sY + ADJY + 168) && (mouse_y < sY + ADJY + 185)) {
		{
				hb::net::PacketRequestFightzoneReserve req{};
				req.header.msg_id = MsgId::RequestFightZoneReserve;
				req.header.msg_type = 0;
				req.fightzone = 1;
				send_game_packet(req);
			}
		m_mode = mode::fightzone_waiting;
		m_game->on_game()->m_fightzone_number_temp = 1;
		play_sound_effect('E', 14, 5);
		return true;
	}
	// Fightzone 2
	if ((mouse_x > sX + ADJX + 150) && (mouse_x < sX + ADJX + 222) && (mouse_y > sY + ADJY + 168) && (mouse_y < sY + ADJY + 185)) {
		{
				hb::net::PacketRequestFightzoneReserve req{};
				req.header.msg_id = MsgId::RequestFightZoneReserve;
				req.header.msg_type = 0;
				req.fightzone = 2;
				send_game_packet(req);
			}
		m_mode = mode::fightzone_waiting;
		m_game->on_game()->m_fightzone_number_temp = 2;
		play_sound_effect('E', 14, 5);
		return true;
	}
	// Fightzone 3
	if ((mouse_x > sX + ADJX + 65) && (mouse_x < sX + ADJX + 137) && (mouse_y > sY + ADJY + 188) && (mouse_y < sY + ADJY + 205)) {
		{
				hb::net::PacketRequestFightzoneReserve req{};
				req.header.msg_id = MsgId::RequestFightZoneReserve;
				req.header.msg_type = 0;
				req.fightzone = 3;
				send_game_packet(req);
			}
		m_mode = mode::fightzone_waiting;
		m_game->on_game()->m_fightzone_number_temp = 3;
		play_sound_effect('E', 14, 5);
		return true;
	}
	// Fightzone 4
	if ((mouse_x > sX + ADJX + 150) && (mouse_x < sX + ADJX + 222) && (mouse_y > sY + ADJY + 188) && (mouse_y < sY + ADJY + 205)) {
		{
				hb::net::PacketRequestFightzoneReserve req{};
				req.header.msg_id = MsgId::RequestFightZoneReserve;
				req.header.msg_type = 0;
				req.fightzone = 4;
				send_game_packet(req);
			}
		m_mode = mode::fightzone_waiting;
		m_game->on_game()->m_fightzone_number_temp = 4;
		play_sound_effect('E', 14, 5);
		return true;
	}
	// Fightzone 5
	if ((mouse_x > sX + ADJX + 65) && (mouse_x < sX + ADJX + 137) && (mouse_y > sY + ADJY + 208) && (mouse_y < sY + ADJY + 225)) {
		{
				hb::net::PacketRequestFightzoneReserve req{};
				req.header.msg_id = MsgId::RequestFightZoneReserve;
				req.header.msg_type = 0;
				req.fightzone = 5;
				send_game_packet(req);
			}
		m_mode = mode::fightzone_waiting;
		m_game->on_game()->m_fightzone_number_temp = 5;
		play_sound_effect('E', 14, 5);
		return true;
	}
	// Fightzone 6
	if ((mouse_x > sX + ADJX + 150) && (mouse_x < sX + ADJX + 222) && (mouse_y > sY + ADJY + 208) && (mouse_y < sY + ADJY + 225)) {
		{
				hb::net::PacketRequestFightzoneReserve req{};
				req.header.msg_id = MsgId::RequestFightZoneReserve;
				req.header.msg_type = 0;
				req.fightzone = 6;
				send_game_packet(req);
			}
		m_mode = mode::fightzone_waiting;
		m_game->on_game()->m_fightzone_number_temp = 6;
		play_sound_effect('E', 14, 5);
		return true;
	}
	// Fightzone 7
	if ((mouse_x > sX + ADJX + 65) && (mouse_x < sX + ADJX + 137) && (mouse_y > sY + ADJY + 228) && (mouse_y < sY + ADJY + 245)) {
		{
				hb::net::PacketRequestFightzoneReserve req{};
				req.header.msg_id = MsgId::RequestFightZoneReserve;
				req.header.msg_type = 0;
				req.fightzone = 7;
				send_game_packet(req);
			}
		m_mode = mode::fightzone_waiting;
		m_game->on_game()->m_fightzone_number_temp = 7;
		play_sound_effect('E', 14, 5);
		return true;
	}
	// Fightzone 8
	if ((mouse_x > sX + ADJX + 150) && (mouse_x < sX + ADJX + 222) && (mouse_y > sY + ADJY + 228) && (mouse_y < sY + ADJY + 245)) {
		{
				hb::net::PacketRequestFightzoneReserve req{};
				req.header.msg_id = MsgId::RequestFightZoneReserve;
				req.header.msg_type = 0;
				req.fightzone = 8;
				send_game_packet(req);
			}
		m_mode = mode::fightzone_waiting;
		m_game->on_game()->m_fightzone_number_temp = 8;
		play_sound_effect('E', 14, 5);
		return true;
	}

	// Cancel
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y)) {
		m_mode = mode::main_menu;
		play_sound_effect('E', 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_GuildMenu::on_click_mode_ok_only(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y)) {
		m_mode = mode::main_menu;
		play_sound_effect('E', 14, 5);
		return true;
	}
	return false;
}

bool DialogBox_GuildMenu::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (m_mode == mode::create_guild) {
		text_input_manager::get().end_input();
		text_input_manager::get().start_input(m_x + 75, m_y + 140, 21, player().m_guild_name, false, hb::client::guild_name_allowed_chars);
	}
	return true;
}

bool DialogBox_GuildMenu::on_disable()
{
	if (m_mode == mode::create_guild)
		text_input_manager::get().end_input();
	m_mode = mode::main_menu;
	return true;
}
