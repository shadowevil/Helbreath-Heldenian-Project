#include "dialog_box_hud_panel.h"
#include "game.h"
#include "packet_send_helpers.h"

#include "global_def.h"
#include "shared_calculations.h"
#include "game_fonts.h"
#include "text_lib_ext.h"
#include "input.h"
#include "input_state_helper.h"
#include "combat_system.h"
#include <algorithm>
#include <cstdlib>
#include <format>
#include <string>
#include "screen_on_game.h"
#include "audio_manager.h"

using namespace hb::shared::net;
// Static button data shared between draw and click handling
const DialogBox_HudPanel::ToggleButtonInfo DialogBox_HudPanel::TOGGLE_BUTTONS[] = {
	{ 410, 447, "Character",   DialogBoxId::CharacterInfo },
	{ 447, 484, "Inventory",   DialogBoxId::Inventory },
	{ 484, 521, "Magics",      DialogBoxId::Magic },
	{ 521, 558, "Skills",      DialogBoxId::Skill },
	{ 558, 595, "Chat Log",    DialogBoxId::ChatHistory },
	{ 595, 631, "System Menu", DialogBoxId::SystemMenu }
};

DialogBox_HudPanel::DialogBox_HudPanel(CGame* game)
	: IDialogBox(DialogBoxId::HudPanel, game)
{
	set_default_rect(0, LOGICAL_HEIGHT() - ICON_PANEL_HEIGHT(), ICON_PANEL_WIDTH(), ICON_PANEL_HEIGHT());
	m_can_close_on_right_click = false;
}

hb::shared::geometry::GamePoint DialogBox_HudPanel::hud_mouse()
{
	// The HUD draws with 800x600 mapped onto the world rect, so its units are world-target units.
	// Not clamped: a mouse outside the world rect simply misses every HUD rect.
	return hb::client::mouse_in_world_units();
}

short DialogBox_HudPanel::hud_mouse_x()
{
	return static_cast<short>(hud_mouse().x);
}

short DialogBox_HudPanel::hud_mouse_y()
{
	return static_cast<short>(hud_mouse().y);
}

bool DialogBox_HudPanel::is_in_button(int x1, int x2) const
{
	short mouse_x = hud_mouse_x();
	short mouse_y = hud_mouse_y();
	return (mouse_x > x1) && (mouse_x < x2) && (mouse_y > BTN_Y1()) && (mouse_y < BTN_Y2());
}

void DialogBox_HudPanel::toggle_dialog_with_sound(DialogBoxId::Type dialogId)
{
	if (m_game->get_dialog_box_manager().is_enabled(dialogId))
		m_game->get_dialog_box_manager().disable_dialog_box(dialogId);
	else
		enable_dialog_box(dialogId, 0, 0, 0);
	audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
}

void DialogBox_HudPanel::draw_gauge_bars()
{
	int max_point, display_value;
	// The HUD kit draws each gauge as trough -> fill clipped to the value -> caps; EXP and the message frame go
	// first, under the rest
	const hb::client::hud_kit& kit = m_game->get_hud_kit();
	kit.exp(m_y, player().m_exp, m_game->get_level_exp(player().m_level + 1));
	kit.message_frame(m_y);

	// HP bar
	max_point = hb::shared::calc::max_hp(m_game->m_formula_engine,
		hb::shared::calc::vit{(double)player().m_vit}, hb::shared::calc::level{(double)player().m_level},
		hb::shared::calc::str{(double)player().m_str}, hb::shared::calc::angelic_str{(double)player().m_angelic_str});
	if (max_point <= 0) max_point = 1;
	display_value = std::min(player().m_hp, max_point);
	kit.tube(hb::client::hud_tube::hp, m_y, display_value, max_point);

	// HP number
	std::string statBuf;
	statBuf = std::format("{}", display_value);
	if (player().m_is_poisoned)
	{
		hb::shared::text::draw_text(GameFont::Numbers, 85 + hud_x_offset() + left_group_dx, HP_NUM_Y(), statBuf.c_str(),
			hb::shared::text::TextStyle::from_color(GameColors::PoisonText));
		hb::shared::text::draw_text(GameFont::SprFont3_2, 35 + hud_x_offset() + left_group_dx, HP_BAR_Y() + 2, "Poisoned",
			hb::shared::text::TextStyle::from_color(GameColors::PoisonLabel).with_alpha(0.7f));
	}
	else
	{
		hb::shared::text::draw_text(GameFont::Numbers, HP_NUM_X() + 1, HP_NUM_Y() + 1, statBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
		hb::shared::text::draw_text(GameFont::Numbers, HP_NUM_X(), HP_NUM_Y(), statBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite));
	}

	// MP bar
	max_point = hb::shared::calc::max_mp(m_game->m_formula_engine,
		hb::shared::calc::mag{(double)player().m_mag}, hb::shared::calc::angelic_mag{(double)player().m_angelic_mag},
		hb::shared::calc::level{(double)player().m_level}, hb::shared::calc::intel{(double)player().m_int},
		hb::shared::calc::angelic_int{(double)player().m_angelic_int});
	if (max_point <= 0) max_point = 1;
	display_value = std::min(player().m_mp, max_point);
	kit.tube(hb::client::hud_tube::mp, m_y, display_value, max_point);

	// MP number
	statBuf = std::format("{}", display_value);
	hb::shared::text::draw_text(GameFont::Numbers, HP_NUM_X() + 1, MP_NUM_Y() + 1, statBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
	hb::shared::text::draw_text(GameFont::Numbers, HP_NUM_X(), MP_NUM_Y(), statBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite));

	// SP bar
	max_point = hb::shared::calc::max_sp(m_game->m_formula_engine,
		hb::shared::calc::str{(double)player().m_str}, hb::shared::calc::angelic_str{(double)player().m_angelic_str},
		hb::shared::calc::level{(double)player().m_level});
	if (max_point <= 0) max_point = 1;
	display_value = std::min(player().m_sp, max_point);
	kit.stamina(m_y, display_value, max_point);

	// SP number
	statBuf = std::format("{}", display_value);
	hb::shared::text::draw_text(GameFont::Numbers, SP_NUM_X() + 1, SP_NUM_Y() + 1, statBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack));
	hb::shared::text::draw_text(GameFont::Numbers, SP_NUM_X(), SP_NUM_Y(), statBuf.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite));

	// Hunger bar (the kit drew EXP first, under the rest)
	kit.hunger(m_y, std::clamp(player().m_hunger_status, 0, MAX_HUNGER), MAX_HUNGER);
}

void DialogBox_HudPanel::draw_status_icons()
{
	short mouse_x = hud_mouse_x();
	short mouse_y = hud_mouse_y();

	// Level Up / Restart / Tester text moved to DialogBox_StatusOverlay

	const hb::client::hud_kit& kit = m_game->get_hud_kit();
	const bool combat_hover = is_in_button(BTN_COMBAT_X1(), BTN_COMBAT_X2());
	// The kit bar is plain: socket and orb in every mode (peace has its own hover orb), ring on hover
	const hb::client::hud_combat::type mode = !player().m_is_combat_mode ? hb::client::hud_combat::peace
		: (player().m_is_safe_attack_mode ? hb::client::hud_combat::safe_attack : hb::client::hud_combat::attack);
	kit.combat(m_y, mode, combat_hover);

	// Combat mode button hover
	if (combat_hover)
	{
		const char* tooltip = player().m_is_combat_mode
			? (player().m_is_safe_attack_mode ? "Safe Attack" : "Attack")
			: "Peace";
		put_string(mouse_x - 10, mouse_y - 20, tooltip, GameColors::UITooltip);
	}

	// Crusade icon
	if (m_game->on_game()->m_is_crusade_mode && player().m_crusade_duty != 0)
	{
		kit.crusade(m_y, player().m_aresden, is_in_button(BTN_CRUSADE_X1(), BTN_CRUSADE_X2()));
	}

	// Map message / coordinates (or remaining EXP when Ctrl pressed)
	std::string infoBuf;
	if (hb::shared::input::is_ctrl_down())
	{
		uint32_t next_exp = m_game->get_level_exp(player().m_level + 1);
		if (next_exp > player().m_exp)
			infoBuf = std::format("Rest Exp: {}", next_exp - player().m_exp);
		else
			infoBuf.clear();
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
	short mouse_x = hud_mouse_x();
	short mouse_y = hud_mouse_y();
	const int xOffset = menu_x_offset();

	// The hovered button (same rects as the clicks), -1 = none
	int hovered = -1;
	if (mouse_y > BTN_Y1() && mouse_y < BTN_Y2())
	{
		for (int i = 0; i < TOGGLE_BUTTON_COUNT; i++)
		{
			// Add X offset for wider resolutions
			if (mouse_x > TOGGLE_BUTTONS[i].x1 + xOffset && mouse_x < TOGGLE_BUTTONS[i].x2 + xOffset)
			{
				hovered = i;
				break;
			}
		}
	}

	// The kit bar is plain, so it draws all six (normal or hover)
	m_game->get_hud_kit().menu(m_y, hovered);

	if (hovered >= 0)
	{
		const auto& btn = TOGGLE_BUTTONS[hovered];
		int tooltipOffset = (btn.dialogId == DialogBoxId::SystemMenu) ? -20 : -10;
		put_string(mouse_x + tooltipOffset, mouse_y - 20, btn.tooltip, GameColors::UITooltip);
	}
}

void DialogBox_HudPanel::draw_super_attack_overlay()
{
	if (player().m_super_attack_left <= 0) return;

	int btnX = BTN_COMBAT_X1();
	int btnY = BTN_Y1();
	int btnW = 42;
	int btnH = 41;

	bool mastered = (player().m_skill_mastery[combat_system::get().get_weapon_skill_type()] == 100);

	// draw additive overlay sprite at combat icon position only when ALT is held
	if (hb::shared::input::is_alt_down() && mastered)
		m_game->get_hud_kit().super_orb(m_y);

	// draw super attack count text at bottom-right of combat button area
	std::string txt = std::format("{}", player().m_super_attack_left);
	if (mastered)
		hb::shared::text::draw_text_aligned(GameFont::Bitmap1, btnX, btnY, btnW, btnH, txt.c_str(), hb::shared::text::TextStyle::with_integrated_shadow(hb::shared::render::Color(255, 255, 255)), hb::shared::text::Align::BottomRight);
	else
		hb::shared::text::draw_text_aligned(GameFont::Bitmap1, btnX, btnY, btnW, btnH, txt.c_str(), hb::shared::text::TextStyle::with_highlight(GameColors::BmpBtnActive), hb::shared::text::Align::BottomRight);
}

void DialogBox_HudPanel::on_draw()
{
	// The action bar draws in the world rect: it keeps the share of the window it has at 800x600
	m_game->m_Renderer->push_world_rect_view();

	// draw main HUD background: the HUD kit's plain bar
	m_game->get_hud_kit().bar(m_y);

	draw_gauge_bars();
	draw_status_icons();
	draw_icon_buttons();
	draw_super_attack_overlay();

	m_game->m_Renderer->pop_world_rect_view();
}

bool DialogBox_HudPanel::on_click()
{
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
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	// Combat mode toggle
	if (is_in_button(BTN_COMBAT_X1(), BTN_COMBAT_X2()))
	{
		m_game->send_game_packet(hb::net::make_common_command(CommonType::ToggleCombatMode, m_game->m_player->m_player_x, m_game->m_player->m_player_y));
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	// Dialog toggle buttons (Character, Inventory, Magic, Skill, Chat, System)
	const int xOffset = menu_x_offset();
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
