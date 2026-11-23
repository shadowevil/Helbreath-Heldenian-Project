#include "DialogBox_HudPanel.h"
#include "Game.h"
#include "PacketSendHelpers.h"

#include "GlobalDef.h"
#include "SharedCalculations.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include "IInput.h"
#include "CombatSystem.h"
#include <algorithm>
#include <cstdlib>
#include <format>
#include <string>
#include "Screen_OnGame.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
// Static button data shared between draw and click handling
const DialogBox_HudPanel::ToggleButtonInfo DialogBox_HudPanel::TOGGLE_BUTTONS[] = {
	{ 410, 447, 412, 6, "Character",   DialogBoxId::CharacterInfo },
	{ 447, 484, 449, 7, "Inventory",   DialogBoxId::Inventory },
	{ 484, 521, 486, 8, "Magics",      DialogBoxId::Magic },
	{ 521, 558, 523, 9, "Skills",      DialogBoxId::Skill },
	{ 558, 595, 560, 10, "Chat Log",   DialogBoxId::ChatHistory },
	{ 595, 631, 597, 11, "System Menu", DialogBoxId::SystemMenu }
};

DialogBox_HudPanel::DialogBox_HudPanel(CGame* game)
	: IDialogBox(DialogBoxId::HudPanel, game)
{
	set_default_rect(0, LOGICAL_HEIGHT() - ICON_PANEL_HEIGHT(), ICON_PANEL_WIDTH(), ICON_PANEL_HEIGHT());
	m_can_close_on_right_click = false;
}

bool DialogBox_HudPanel::is_in_button(int x1, int x2) const
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	return (mouse_x > x1) && (mouse_x < x2) && (mouse_y > BTN_Y1()) && (mouse_y < BTN_Y2());
}

void DialogBox_HudPanel::toggle_dialog_with_sound(DialogBoxId::Type dialogId)
{
	if (m_game->get_dialog_box_manager().is_enabled(dialogId))
		m_game->get_dialog_box_manager().disable_dialog_box(dialogId);
	else
		enable_dialog_box(dialogId, 0, 0, 0);
	m_game->play_game_sound('E', 14, 5);
}

void DialogBox_HudPanel::draw_gauge_bars()
{
	int max_point, bar_width, display_value;
	uint32_t time = m_game->m_cur_time;
	auto sprite = m_game->m_sprite[InterfaceNdIconPanel];

	// HP bar
	max_point = hb::shared::calc::CalculateMaxHP(player().m_vit, player().m_level,
	                           player().m_str, player().m_angelic_str);
	if (max_point <= 0) max_point = 1;
	display_value = std::min(player().m_hp, max_point);
	bar_width = HP_MP_BAR_WIDTH - (display_value * HP_MP_BAR_WIDTH) / max_point;
	if (bar_width < 0) bar_width = 0;
	if (bar_width > HP_MP_BAR_WIDTH) bar_width = HP_MP_BAR_WIDTH;
	sprite->DrawWidth(HP_BAR_X(), HP_BAR_Y(), 12, bar_width);

	// HP number
	std::string statBuf;
	statBuf = std::format("{}", display_value);
	if (player().m_is_poisoned)
	{
		hb::shared::text::draw_text(GameFont::Numbers, 85 + hud_x_offset(), HP_NUM_Y(), statBuf.c_str(),
			hb::shared::text::TextStyle::from_color(GameColors::PoisonText));
		hb::shared::text::draw_text(GameFont::SprFont3_2, 35 + hud_x_offset(), HP_BAR_Y() + 2, "Poisoned",
			hb::shared::text::TextStyle::from_color(GameColors::PoisonLabel).with_alpha(0.7f));
	}
	else
	{
		hb::shared::text::draw_text(GameFont::Numbers, HP_NUM_X() + 1, HP_NUM_Y() + 1, statBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
		hb::shared::text::draw_text(GameFont::Numbers, HP_NUM_X(), HP_NUM_Y(), statBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite));
	}

	// MP bar
	max_point = hb::shared::calc::CalculateMaxMP(player().m_mag, player().m_angelic_mag,
	                           player().m_level, player().m_int, player().m_angelic_int);
	if (max_point <= 0) max_point = 1;
	display_value = std::min(player().m_mp, max_point);
	bar_width = HP_MP_BAR_WIDTH - (display_value * HP_MP_BAR_WIDTH) / max_point;
	if (bar_width < 0) bar_width = 0;
	if (bar_width > HP_MP_BAR_WIDTH) bar_width = HP_MP_BAR_WIDTH;
	sprite->DrawWidth(HP_BAR_X(), MP_BAR_Y(), 12, bar_width);

	// MP number
	statBuf = std::format("{}", display_value);
	hb::shared::text::draw_text(GameFont::Numbers, HP_NUM_X() + 1, MP_NUM_Y() + 1, statBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
	hb::shared::text::draw_text(GameFont::Numbers, HP_NUM_X(), MP_NUM_Y(), statBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite));

	// SP bar
	max_point = hb::shared::calc::CalculateMaxSP(player().m_str, player().m_angelic_str, player().m_level);
	if (max_point <= 0) max_point = 1;
	display_value = std::min(player().m_sp, max_point);
	bar_width = SP_BAR_WIDTH - (display_value * SP_BAR_WIDTH) / max_point;
	if (bar_width < 0) bar_width = 0;
	if (bar_width > SP_BAR_WIDTH) bar_width = SP_BAR_WIDTH;
	sprite->DrawWidth(SP_BAR_X(), SP_BAR_Y(), 13, bar_width);

	// SP number
	statBuf = std::format("{}", display_value);
	hb::shared::text::draw_text(GameFont::Numbers, SP_NUM_X() + 1, SP_NUM_Y() + 1, statBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
	hb::shared::text::draw_text(GameFont::Numbers, SP_NUM_X(), SP_NUM_Y(), statBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite));

}

void DialogBox_HudPanel::draw_status_icons()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	uint32_t time = m_game->m_cur_time;
	auto sprite = m_game->m_sprite[InterfaceNdIconPanel];

	// Level up / Restart text (mutually exclusive: dead shows Restart, alive shows Level Up)
	if (player().m_hp > 0)
	{
		if ((player().m_lu_point > 0) && !m_game->get_dialog_box_manager().is_enabled(DialogBoxId::LevelUpSetting))
		{
			int flashColor = (GameClock::get_time_ms() / 3) % 255;
			hb::shared::text::draw_text(GameFont::Bitmap1, LEVELUP_TEXT_X(), LEVELUP_TEXT_Y(), "Level Up!", hb::shared::text::TextStyle::with_integrated_shadow(hb::shared::render::Color(flashColor, flashColor, 0)));
		}
	}
	else if (m_game->m_restart_count == -1)
	{
		int flashColor = (GameClock::get_time_ms() / 3) % 255;
		hb::shared::text::draw_text(GameFont::Bitmap1, LEVELUP_TEXT_X(), LEVELUP_TEXT_Y(), "Restart", hb::shared::text::TextStyle::with_integrated_shadow(hb::shared::render::Color(flashColor, flashColor, 0)));
	}

#ifdef TESTER_ONLY
	// TESTER MENU — overlay indicator (tester builds only)
	if (!m_game->get_dialog_box_manager().is_enabled(DialogBoxId::TesterMenu))
	{
		int flash = (GameClock::get_time_ms() / 3) % 255;
		hb::shared::text::draw_text(GameFont::Bitmap1, LEVELUP_TEXT_X(), LEVELUP_TEXT_Y() - 30,
			"Tester",
			hb::shared::text::TextStyle::with_integrated_shadow(hb::shared::render::Color(flash, flash, 0)));
	}
#endif // TESTER_ONLY

	// Combat mode / Safe attack icon (only shown while in attack mode)
	if (player().m_is_combat_mode)
	{
		if (player().m_is_safe_attack_mode)
			sprite->draw(COMBAT_ICON_X(), COMBAT_ICON_Y(), 4);
		else
			sprite->draw(COMBAT_ICON_X(), COMBAT_ICON_Y(), 5);
	}

	// Combat mode button hover
	if (is_in_button(BTN_COMBAT_X1(), BTN_COMBAT_X2()))
	{
		sprite->draw(BTN_COMBAT_X1(), BTN_Y1(), 16);
		const char* tooltip = player().m_is_combat_mode
			? (player().m_is_safe_attack_mode ? "Safe Attack" : "Attack")
			: "Peace";
		put_string(mouse_x - 10, mouse_y - 20, tooltip, GameColors::UITooltip);
	}

	// Crusade icon
	if (m_game->on_game()->m_is_crusade_mode && player().m_crusade_duty != 0)
	{
		bool hover = is_in_button(BTN_CRUSADE_X1(), BTN_CRUSADE_X2());
		if (player().m_aresden)
			sprite->draw(BTN_CRUSADE_X1() + (hover ? 1 : 0), BTN_Y1(), hover ? 1 : 2);
		else
			sprite->draw(BTN_CRUSADE_X1(), BTN_Y1(), hover ? 0 : 15);
	}

	// Map message / coordinates (or remaining EXP when Ctrl pressed)
	std::string infoBuf;
	if (hb::shared::input::is_ctrl_down())
	{
		uint32_t cur_exp = m_game->get_level_exp(player().m_level);
		uint32_t next_exp = m_game->get_level_exp(player().m_level + 1);
		if (player().m_exp < next_exp)
		{
			uint32_t exp_range = next_exp - cur_exp;
			uint32_t exp_progress = (player().m_exp > cur_exp) ? (player().m_exp - cur_exp) : 0;
			infoBuf = std::format("Rest Exp: {}", exp_range - exp_progress);
		}
		else
		{
			infoBuf.clear();
		}
	}
	else
	{
		infoBuf = std::format("{} ({},{})", m_game->m_map_message, player().m_player_x, player().m_player_y);
	}
	put_aligned_string(MAP_MSG_X1() + 1, MAP_MSG_X2() + 1, MAP_MSG_Y() + 1, infoBuf.c_str(), GameColors::UIBlack);
	put_aligned_string(MAP_MSG_X1(), MAP_MSG_X2(), MAP_MSG_Y(), infoBuf.c_str(), GameColors::UIPaleYellow);
}

void DialogBox_HudPanel::draw_icon_buttons()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());

	if (mouse_y <= BTN_Y1() || mouse_y >= BTN_Y2()) return;

	uint32_t time = m_game->m_cur_time;
	auto sprite = m_game->m_sprite[InterfaceNdIconPanel];
	int xOffset = hud_x_offset();

	for (int i = 0; i < TOGGLE_BUTTON_COUNT; i++)
	{
		const auto& btn = TOGGLE_BUTTONS[i];
		// Add X offset for wider resolutions
		if (mouse_x > btn.x1 + xOffset && mouse_x < btn.x2 + xOffset)
		{
			sprite->draw(btn.spriteX + xOffset, BTN_Y1(), btn.spriteFrame);
			int tooltipOffset = (btn.dialogId == DialogBoxId::SystemMenu) ? -20 : -10;
			put_string(mouse_x + tooltipOffset, mouse_y - 20, btn.tooltip, GameColors::UITooltip);
			break;
		}
	}
}

void DialogBox_HudPanel::draw_super_attack_overlay()
{
	if (player().m_super_attack_left <= 0) return;

	int iconX = COMBAT_ICON_X();
	int iconY = COMBAT_ICON_Y();
	int btnX = BTN_COMBAT_X1();
	int btnY = BTN_Y1();
	int btnW = 42;
	int btnH = 41;

	bool mastered = (player().m_skill_mastery[combat_system::get().get_weapon_skill_type()] == 100);

	// draw additive overlay sprite at combat icon position only when ALT is held
	if (hb::shared::input::is_alt_down() && mastered)
		m_game->m_sprite[InterfaceNdIconPanel]->draw(iconX, iconY, 3, hb::shared::sprite::DrawParams::additive(0.7f));

	// draw super attack count text at bottom-right of combat button area
	std::string txt = std::format("{}", player().m_super_attack_left);
	if (mastered)
		hb::shared::text::draw_text_aligned(GameFont::Bitmap1, btnX, btnY, btnW, btnH, txt.c_str(), hb::shared::text::TextStyle::with_integrated_shadow(hb::shared::render::Color(255, 255, 255)), hb::shared::text::Align::BottomRight);
	else
		hb::shared::text::draw_text_aligned(GameFont::Bitmap1, btnX, btnY, btnW, btnH, txt.c_str(), hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnActive), hb::shared::text::Align::BottomRight);
}

void DialogBox_HudPanel::on_draw()
{
	// draw main HUD background centered (at xOffset, which is 0 for 640x480, 80 for 800x600)
	m_game->m_sprite[InterfaceNdIconPanel]->draw(0, m_y, 14);

	draw_gauge_bars();
	draw_status_icons();
	draw_icon_buttons();
	draw_super_attack_overlay();
}

bool DialogBox_HudPanel::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());

	// Crusade button
	if (is_in_button(BTN_CRUSADE_X1(), BTN_CRUSADE_X2()))
	{
		if (!m_game->on_game()->m_is_crusade_mode) return false;
		switch (player().m_crusade_duty)
		{
		case 1: enable_dialog_box(DialogBoxId::CrusadeSoldier, 0, 0, 0); break;
		case 2: enable_dialog_box(DialogBoxId::CrusadeConstructor, 0, 0, 0); break;
		case 3: enable_dialog_box(DialogBoxId::CrusadeCommander, 0, 0, 0); break;
		default: return false;
		}
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// Combat mode toggle
	if (is_in_button(BTN_COMBAT_X1(), BTN_COMBAT_X2()))
	{
		m_game->send_game_packet(hb::net::make_common_command(CommonType::ToggleCombatMode, m_game->m_player->m_player_x, m_game->m_player->m_player_y));
		m_game->play_game_sound('E', 14, 5);
		return true;
	}

	// Dialog toggle buttons (Character, Inventory, Magic, Skill, Chat, System)
	int xOffset = hud_x_offset();
	for (int i = 0; i < TOGGLE_BUTTON_COUNT; i++)
	{
		const auto& btn = TOGGLE_BUTTONS[i];
		// Add X offset for wider resolutions
		if (is_in_button(btn.x1 + xOffset, btn.x2 + xOffset))
		{
			toggle_dialog_with_sound(btn.dialogId);
			return true;
		}
	}

	return false;
}

bool DialogBox_HudPanel::on_item_drop()
{
	// Placeholder — no item drop behavior on HUD panel at this time
	return false;
}
