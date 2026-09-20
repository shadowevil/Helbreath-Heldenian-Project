// TESTER MENU — entire file is tester-only
#ifdef TESTER_ONLY
#include "DialogBox_ItemCreator.h"
#include "Game.h"
#include "GlobalDef.h"
#include "SpriteID.h"
#include "NetMessages.h"
#include "PacketSendHelpers.h"

#include "GameFonts.h"
#include "TextLibExt.h"
#include "TextInputManager.h"
#include "TextFieldRenderer.h"
#include "Item/ItemEnums.h"
#include <algorithm>
#include <format>
#include <cstring>
#include "IInput.h"
#include "AudioManager.h"

using namespace hb::shared::net;
using namespace hb::client::sprite_id;
using namespace hb::shared::item;
using render_color = hb::shared::render::Color;

// Layout constants
namespace layout
{
	// Shared
	constexpr int pad = 12;
	constexpr int content_x1 = 12;
	constexpr int content_x2 = 246;
	constexpr int content_w = content_x2 - content_x1;

	// Search page
	constexpr int search_bar_y = 30;
	constexpr int search_bar_h = 20;
	constexpr int list_y = 56;
	constexpr int row_h = 18;
	constexpr int list_rows = 12;
	constexpr int list_h = list_rows * row_h;
	constexpr int status_y = list_y + list_h + 2;

	// Configure page — two-column layout
	constexpr int item_info_y = 38;    // "Dagger (Weapon)" combined line

	// Column boundaries (inset from frame edges)
	constexpr int col_left_x1 = 22;
	constexpr int col_left_x2 = 122;
	constexpr int col_left_w = col_left_x2 - col_left_x1;
	constexpr int col_right_x1 = 136;
	constexpr int col_right_x2 = 236;
	constexpr int col_right_w = col_right_x2 - col_right_x1;

	// Row 1: First Stat / Second Stat type dropdowns
	constexpr int row1_label_y = 62;
	constexpr int row1_sel_y = 78;

	// Row 2: Value dropdowns
	constexpr int row2_label_y = 100;
	constexpr int row2_sel_y = 116;

	// Row 3: Upgrade / Count dropdowns
	constexpr int row3_label_y = 138;
	constexpr int row3_sel_y = 154;

	// Preview + buttons
	constexpr int preview_label_y = 180;
	constexpr int preview_text_y = 198;
	constexpr int btn_y = 234;
	constexpr int btn_w = 100;
}

// Dropdown visual style — warm tones to match parchment dialog background
namespace dd_style
{
	const auto bg           = render_color(40, 35, 28, 190);
	const auto border       = render_color(80, 70, 50);
	const auto border_hover = render_color(140, 125, 90);
	const auto border_open  = render_color(180, 160, 100);
	const auto list_bg      = render_color(30, 25, 18, 235);
	const auto list_border  = render_color(100, 90, 60);
	const auto item_hover   = render_color(90, 75, 45, 180);
	const auto scrollbar    = render_color(130, 115, 75, 160);
}

DialogBox_ItemCreator::DialogBox_ItemCreator(CGame* game)
	: IDialogBox(DialogBoxId::ItemCreator, game)
{
	set_default_rect(0, 0, 258, 339);
	m_can_close_on_right_click = true;
}

bool DialogBox_ItemCreator::on_disable()
{
	text_input_manager::get().end_input();
	m_initial_load = false;
	m_last_sent_search.clear();
	m_open_dropdown = dropdown_id::none;
	return true;
}

DialogBox_ItemCreator::item_category DialogBox_ItemCreator::classify_item(int16_t effect_type)
{
	auto et = static_cast<ItemEffectType>(effect_type);
	if (et == ItemEffectType::AttackManaSave)
		return item_category::magic_weapon;
	if (is_attack_effect_type(et))
		return item_category::weapon;
	if (et == ItemEffectType::Defense)
		return item_category::armor;
	return item_category::none;
}

const char* DialogBox_ItemCreator::category_name(item_category cat)
{
	switch (cat)
	{
	case item_category::weapon:       return "Weapon";
	case item_category::armor:        return "Armor";
	case item_category::magic_weapon: return "Magic Weapon";
	default:                          return "Other";
	}
}

void DialogBox_ItemCreator::build_valid_options(int16_t effect_type)
{
	m_category = classify_item(effect_type);
	m_valid_prefixes.clear();
	m_valid_secondaries.clear();

	m_valid_prefixes.push_back({0, "None", 0});
	m_valid_secondaries.push_back({0, "None", 0});

	switch (m_category)
	{
	case item_category::weapon:
		m_valid_prefixes.push_back({1, "Critical", m_game->m_prefix_multiplier[1]});
		m_valid_prefixes.push_back({2, "Poisoning", m_game->m_prefix_multiplier[2]});
		m_valid_prefixes.push_back({3, "Righteous", m_game->m_prefix_multiplier[3]});
		m_valid_prefixes.push_back({5, "Agile", m_game->m_prefix_multiplier[5]});
		m_valid_prefixes.push_back({6, "Light", m_game->m_prefix_multiplier[6]});
		m_valid_prefixes.push_back({7, "Sharp", m_game->m_prefix_multiplier[7]});
		m_valid_prefixes.push_back({8, "Strong", m_game->m_prefix_multiplier[8]});
		m_valid_prefixes.push_back({9, "Ancient", m_game->m_prefix_multiplier[9]});
		m_valid_secondaries.push_back({2, "HitProb", m_game->m_secondary_multiplier[2]});
		m_valid_secondaries.push_back({10, "ConsecAtk", m_game->m_secondary_multiplier[10]});
		m_valid_secondaries.push_back({11, "ExpBonus", m_game->m_secondary_multiplier[11]});
		m_valid_secondaries.push_back({12, "GoldBonus", m_game->m_secondary_multiplier[12]});
		break;

	case item_category::armor:
		m_valid_prefixes.push_back({8, "Strong", m_game->m_prefix_multiplier[8]});
		m_valid_prefixes.push_back({6, "Light", m_game->m_prefix_multiplier[6]});
		m_valid_prefixes.push_back({11, "ManaConvert", m_game->m_prefix_multiplier[11]});
		m_valid_prefixes.push_back({12, "CritChance", m_game->m_prefix_multiplier[12]});
		m_valid_secondaries.push_back({3, "DefRatio", m_game->m_secondary_multiplier[3]});
		m_valid_secondaries.push_back({1, "PoisonRes", m_game->m_secondary_multiplier[1]});
		m_valid_secondaries.push_back({5, "SPRecov", m_game->m_secondary_multiplier[5]});
		m_valid_secondaries.push_back({4, "HPRecov", m_game->m_secondary_multiplier[4]});
		m_valid_secondaries.push_back({6, "MPRecov", m_game->m_secondary_multiplier[6]});
		m_valid_secondaries.push_back({7, "MagicRes", m_game->m_secondary_multiplier[7]});
		m_valid_secondaries.push_back({8, "PhysAbsorb", m_game->m_secondary_multiplier[8]});
		m_valid_secondaries.push_back({9, "MagicAbsorb", m_game->m_secondary_multiplier[9]});
		break;

	case item_category::magic_weapon:
		m_valid_prefixes.push_back({10, "Special", m_game->m_prefix_multiplier[10]});
		m_valid_secondaries.push_back({2, "HitProb", m_game->m_secondary_multiplier[2]});
		m_valid_secondaries.push_back({10, "ConsecAtk", m_game->m_secondary_multiplier[10]});
		m_valid_secondaries.push_back({11, "ExpBonus", m_game->m_secondary_multiplier[11]});
		m_valid_secondaries.push_back({12, "GoldBonus", m_game->m_secondary_multiplier[12]});
		break;

	default:
		break;
	}
}

std::string DialogBox_ItemCreator::build_preview_string() const
{
	if (m_selected_index < 0 || m_selected_index >= m_result_count)
		return "";

	std::string result;

	if (m_prefix_index > 0 && m_prefix_index < static_cast<int>(m_valid_prefixes.size()))
	{
		result += m_valid_prefixes[m_prefix_index].name;
		result += " ";
	}

	result += m_results[m_selected_index].name;

	if (m_prefix_index > 0 && m_prefix_index < static_cast<int>(m_valid_prefixes.size()))
	{
		int real_val = m_prefix_value * m_valid_prefixes[m_prefix_index].multiplier;
		result += std::format(" {}", real_val);
	}

	if (m_secondary_index > 0 && m_secondary_index < static_cast<int>(m_valid_secondaries.size()))
	{
		int real_val = m_secondary_value * m_valid_secondaries[m_secondary_index].multiplier;
		result += std::format(" {} {}", m_valid_secondaries[m_secondary_index].name, real_val);
	}

	if (m_enchant_value > 0)
		result += std::format(" +{}", m_enchant_value);

	return result;
}

void DialogBox_ItemCreator::on_enter_pressed()
{
	// Live search handles everything — Enter is a no-op
}

void DialogBox_ItemCreator::receive_search_results(const hb::net::PacketNotifyTesterItemSearchResult* pkt)
{
	m_result_count = std::clamp(static_cast<int>(pkt->count), 0, 50);
	std::memcpy(m_results, pkt->entries, sizeof(m_results));
	m_selected_index = -1;
	m_scroll_offset = 0;
}

// ---------------------------------------------------------------------------
// UI HELPERS
// ---------------------------------------------------------------------------

void DialogBox_ItemCreator::draw_dropdown_field(int x, int y, int w,
	const char* text, bool is_open, bool is_hover)
{
	// Background box
	m_game->m_Renderer->draw_rect_filled(x, y, w, dropdown_h, dd_style::bg);

	// Border — color depends on state
	auto border = is_open ? dd_style::border_open : (is_hover ? dd_style::border_hover : dd_style::border);
	m_game->m_Renderer->draw_rect_outline(x, y, w, dropdown_h, border);

	// Selected value text (left-aligned with padding) — always gold
	put_string(x + 4, y + 2, text, GameColors::UIPaleYellow);
}

int DialogBox_ItemCreator::get_open_dropdown_count() const
{
	switch (m_open_dropdown)
	{
	case dropdown_id::prefix_type:  return static_cast<int>(m_valid_prefixes.size());
	case dropdown_id::effect_type:  return static_cast<int>(m_valid_secondaries.size());
	case dropdown_id::prefix_value: return max_value;
	case dropdown_id::effect_value: return max_value;
	case dropdown_id::upgrade:      return 16;
	case dropdown_id::count:        return 10;
	default:                        return 0;
	}
}

// ---------------------------------------------------------------------------
// SEARCH PAGE
// ---------------------------------------------------------------------------

void DialogBox_ItemCreator::draw_search_page(short sX, short sY, short size_x, short mouse_x, short mouse_y, short z)
{
	// Title
	hb::shared::text::draw_text_aligned(GameFont::Bitmap1,
		sX, sY + 8, size_x, 15,
		"Create Item",
		hb::shared::text::TextStyle::with_integrated_shadow(GameColors::UIWarningRed),
		hb::shared::text::Align::TopCenter);

	// Text input
	if (!text_input_manager::get().is_active())
	{
		text_input_manager::get().start_input(sX + 70, sY + layout::search_bar_y + 5, 20, m_search_text);
		m_last_sx = sX;
		m_last_sy = sY;
	}
	else if (sX != m_last_sx || sY != m_last_sy)
	{
		text_input_manager::get().end_input();
		text_input_manager::get().start_input(sX + 70, sY + layout::search_bar_y + 5, 20, m_search_text);
		m_last_sx = sX;
		m_last_sy = sY;
	}

	put_string(sX + 16, sY + layout::search_bar_y + 5, "Search:", GameColors::UIWhite);

	// Live search: auto-send whenever text changes (including initial empty load)
	if (!m_initial_load || m_search_text != m_last_sent_search)
	{
		m_initial_load = true;
		m_last_sent_search = m_search_text;
		m_scroll_offset = 0;
		{
			auto pkt = hb::net::make_common_command_str(CommonType::TesterItemSearch, player().m_player_x, player().m_player_y);
			std::snprintf(pkt.text, sizeof(pkt.text), "%s", m_search_text.empty() ? "" : m_search_text.c_str());
			send_game_packet(pkt);
		}
	}

	// Mouse wheel
	if (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::ItemCreator && z != 0)
	{
		m_scroll_offset -= z / 60;
		int max_scroll = std::max(0, m_result_count - layout::list_rows);
		m_scroll_offset = std::clamp(m_scroll_offset, 0, max_scroll);
	}

	// Results list
	for (int i = 0; i < layout::list_rows && (i + m_scroll_offset) < m_result_count; i++)
	{
		int idx = i + m_scroll_offset;
		auto& entry = m_results[idx];
		int ry = sY + layout::list_y + i * layout::row_h;

		bool hover = (mouse_x >= sX + layout::content_x1 && mouse_x <= sX + layout::content_x2
			&& mouse_y >= ry && mouse_y <= ry + layout::row_h - 2);

		auto color = hover ? GameColors::UIWhite : GameColors::UIMagicBlue;
		hb::shared::text::draw_text_aligned(GameFont::Default,
			sX + layout::content_x1 + 6, ry, layout::content_w - 12, 15,
			entry.name,
			hb::shared::text::TextStyle::from_color(color),
			hb::shared::text::Align::TopLeft);
	}

	// Status line
	if (m_result_count > 0)
	{
		int sy = sY + layout::status_y;
		auto count_str = std::format("{} found", m_result_count);
		put_string(sX + layout::content_x1 + 4, sy, count_str.c_str(), GameColors::UIBlack);

		if (m_result_count > layout::list_rows)
		{
			int max_scroll = m_result_count - layout::list_rows;
			auto scroll_str = std::format("[{}/{}]", m_scroll_offset + 1, max_scroll + 1);
			put_string(sX + 100, sy, scroll_str.c_str(), GameColors::UIBlack);
		}
	}

	// Close button (sprite)
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) &&
		(mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
	else
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
}

// ---------------------------------------------------------------------------
// CONFIGURE PAGE
// ---------------------------------------------------------------------------

void DialogBox_ItemCreator::draw_configure_page(short sX, short sY, short size_x, short mouse_x, short mouse_y, short z)
{
	// Title
	hb::shared::text::draw_text_aligned(GameFont::Bitmap1,
		sX, sY + 8, size_x, 15,
		"Configure Item",
		hb::shared::text::TextStyle::with_integrated_shadow(GameColors::UIWarningRed),
		hb::shared::text::Align::TopCenter);

	// Item name + category (single line)
	if (m_selected_index >= 0 && m_selected_index < m_result_count)
	{
		auto info_str = std::format("{} ({})", m_results[m_selected_index].name, category_name(m_category));
		hb::shared::text::draw_text_aligned(GameFont::Default,
			sX, sY + layout::item_info_y, size_x, 15,
			info_str.c_str(),
			hb::shared::text::TextStyle::from_color(GameColors::UIPaleYellow),
			hb::shared::text::Align::TopCenter);
	}

	// Absolute column positions
	int lx = sX + layout::col_left_x1;
	int rx = sX + layout::col_right_x1;

	if (m_category == item_category::none)
	{
		put_aligned_string(sX + layout::col_left_x1, sX + layout::col_right_x2, sY + layout::row1_label_y + 10, "No attributes for this type.", GameColors::UIWhite);
		put_aligned_string(sX + layout::col_left_x1, sX + layout::col_right_x2, sY + layout::row1_label_y + 30, "Item will be created plain.", GameColors::UIWhite);

		// Count dropdown (still available for plain items)
		put_string(rx + 4, sY + layout::row3_label_y, "Count:", GameColors::UIWhite);
		auto count_str = std::to_string(m_item_count);
		bool cnt_open = (m_open_dropdown == dropdown_id::count);
		bool cnt_hover = !cnt_open && (mouse_x >= rx && mouse_x <= rx + layout::col_right_w
			&& mouse_y >= sY + layout::row3_sel_y && mouse_y < sY + layout::row3_sel_y + dropdown_h);
		draw_dropdown_field(rx, sY + layout::row3_sel_y, layout::col_right_w, count_str.c_str(), cnt_open, cnt_hover);
	}
	else
	{
		// --- ROW 1: Prefix type (left) / Effect type (right) ---
		put_string(lx + 4, sY + layout::row1_label_y, "First Stat", GameColors::UIWhite);
		put_string(rx + 4, sY + layout::row1_label_y, "Second Stat", GameColors::UIWhite);

		const char* prefix_name = (m_prefix_index < static_cast<int>(m_valid_prefixes.size()))
			? m_valid_prefixes[m_prefix_index].name : "None";
		bool pn_open = (m_open_dropdown == dropdown_id::prefix_type);
		bool pn_hover = !pn_open && (mouse_x >= lx && mouse_x <= lx + layout::col_left_w
			&& mouse_y >= sY + layout::row1_sel_y && mouse_y < sY + layout::row1_sel_y + dropdown_h);
		draw_dropdown_field(lx, sY + layout::row1_sel_y, layout::col_left_w, prefix_name, pn_open, pn_hover);

		const char* sec_name = (m_secondary_index < static_cast<int>(m_valid_secondaries.size()))
			? m_valid_secondaries[m_secondary_index].name : "None";
		bool sn_open = (m_open_dropdown == dropdown_id::effect_type);
		bool sn_hover = !sn_open && (mouse_x >= rx && mouse_x <= rx + layout::col_right_w
			&& mouse_y >= sY + layout::row1_sel_y && mouse_y < sY + layout::row1_sel_y + dropdown_h);
		draw_dropdown_field(rx, sY + layout::row1_sel_y, layout::col_right_w, sec_name, sn_open, sn_hover);

		// --- ROW 2: Prefix value (left) / Effect value (right) ---
		if (m_prefix_index > 0 && m_prefix_index < static_cast<int>(m_valid_prefixes.size()))
		{
			put_string(lx + 4, sY + layout::row2_label_y, "Value:", GameColors::UIWhite);
			int real_val = m_prefix_value * m_valid_prefixes[m_prefix_index].multiplier;
			auto val_str = std::to_string(real_val);
			bool pv_open = (m_open_dropdown == dropdown_id::prefix_value);
			bool pv_hover = !pv_open && (mouse_x >= lx && mouse_x <= lx + layout::col_left_w
				&& mouse_y >= sY + layout::row2_sel_y && mouse_y < sY + layout::row2_sel_y + dropdown_h);
			draw_dropdown_field(lx, sY + layout::row2_sel_y, layout::col_left_w, val_str.c_str(), pv_open, pv_hover);
		}

		if (m_secondary_index > 0 && m_secondary_index < static_cast<int>(m_valid_secondaries.size()))
		{
			put_string(rx + 4, sY + layout::row2_label_y, "Value:", GameColors::UIWhite);
			int real_val = m_secondary_value * m_valid_secondaries[m_secondary_index].multiplier;
			auto val_str = std::to_string(real_val);
			bool sv_open = (m_open_dropdown == dropdown_id::effect_value);
			bool sv_hover = !sv_open && (mouse_x >= rx && mouse_x <= rx + layout::col_right_w
				&& mouse_y >= sY + layout::row2_sel_y && mouse_y < sY + layout::row2_sel_y + dropdown_h);
			draw_dropdown_field(rx, sY + layout::row2_sel_y, layout::col_right_w, val_str.c_str(), sv_open, sv_hover);
		}

		// --- ROW 3: Upgrade (left) / Count (right) ---
		put_string(lx + 4, sY + layout::row3_label_y, "Upgrade:", GameColors::UIWhite);
		auto enchant_str = std::format("+{}", m_enchant_value);
		bool enc_open = (m_open_dropdown == dropdown_id::upgrade);
		bool enc_hover = !enc_open && (mouse_x >= lx && mouse_x <= lx + layout::col_left_w
			&& mouse_y >= sY + layout::row3_sel_y && mouse_y < sY + layout::row3_sel_y + dropdown_h);
		draw_dropdown_field(lx, sY + layout::row3_sel_y, layout::col_left_w, enchant_str.c_str(), enc_open, enc_hover);

		put_string(rx + 4, sY + layout::row3_label_y, "Count:", GameColors::UIWhite);
		auto count_str = std::to_string(m_item_count);
		bool cnt_open = (m_open_dropdown == dropdown_id::count);
		bool cnt_hover = !cnt_open && (mouse_x >= rx && mouse_x <= rx + layout::col_right_w
			&& mouse_y >= sY + layout::row3_sel_y && mouse_y < sY + layout::row3_sel_y + dropdown_h);
		draw_dropdown_field(rx, sY + layout::row3_sel_y, layout::col_right_w, count_str.c_str(), cnt_open, cnt_hover);

		// --- PREVIEW ---
		auto preview = build_preview_string();
		if (!preview.empty())
		{
			hb::shared::text::draw_text_aligned(GameFont::Default,
				sX, sY + layout::preview_label_y, size_x, 15,
				"Preview:",
				hb::shared::text::TextStyle::from_color(GameColors::UIWhite),
				hb::shared::text::Align::TopCenter);
			hb::shared::text::draw_text_aligned(GameFont::Default,
				sX, sY + layout::preview_text_y, size_x, 15,
				preview.c_str(),
				hb::shared::text::TextStyle::from_color(GameColors::UIPaleYellow),
				hb::shared::text::Align::TopCenter);
		}
	}

	// --- BUTTONS ---
	int left_btn_x = sX + layout::col_left_x1;
	int right_btn_x = sX + layout::col_right_x2 - layout::btn_w;

	auto create_label = (m_item_count > 1) ? std::format("[Create x{}]", m_item_count) : std::string("[Create]");
	bool create_hover = (mouse_x >= left_btn_x && mouse_x <= left_btn_x + layout::btn_w
		&& mouse_y >= sY + layout::btn_y && mouse_y <= sY + layout::btn_y + 18);
	hb::shared::text::draw_text_aligned(GameFont::Default,
		left_btn_x, sY + layout::btn_y, layout::btn_w, 15,
		create_label.c_str(),
		hb::shared::text::TextStyle::from_color(create_hover ? GameColors::UIWhite : GameColors::UIMagicBlue),
		hb::shared::text::Align::TopCenter);

	bool back_hover = (mouse_x >= right_btn_x && mouse_x <= right_btn_x + layout::btn_w
		&& mouse_y >= sY + layout::btn_y && mouse_y <= sY + layout::btn_y + 18);
	hb::shared::text::draw_text_aligned(GameFont::Default,
		right_btn_x, sY + layout::btn_y, layout::btn_w, 15,
		"[<< Back]",
		hb::shared::text::TextStyle::from_color(back_hover ? GameColors::UIWhite : GameColors::UIMagicBlue),
		hb::shared::text::Align::TopCenter);

	// Close button (sprite)
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) &&
		(mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
	else
		draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);

	// --- DROPDOWN OVERLAY (drawn last, on top of everything) ---
	if (m_open_dropdown != dropdown_id::none)
	{
		// Mouse wheel scrolls the open dropdown list
		if (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::ItemCreator && z != 0)
		{
			int total = get_open_dropdown_count();
			if (total > dropdown_max_vis)
			{
				m_dropdown_scroll -= z / 60;
				int max_scroll = total - dropdown_max_vis;
				m_dropdown_scroll = std::clamp(m_dropdown_scroll, 0, max_scroll);
			}
		}

		// Determine dropdown anchor position and build option list
		int dd_x = 0, dd_y = 0, dd_w = 0;
		int dd_count = 0;
		int dd_selected = -1;

		// Temporary option text buffer (tester-only, allocation is fine)
		std::vector<std::string> options;

		switch (m_open_dropdown)
		{
		case dropdown_id::prefix_type:
			dd_x = lx; dd_y = sY + layout::row1_sel_y; dd_w = layout::col_left_w;
			dd_selected = m_prefix_index;
			for (auto& p : m_valid_prefixes) options.push_back(p.name);
			break;
		case dropdown_id::effect_type:
			dd_x = rx; dd_y = sY + layout::row1_sel_y; dd_w = layout::col_right_w;
			dd_selected = m_secondary_index;
			for (auto& s : m_valid_secondaries) options.push_back(s.name);
			break;
		case dropdown_id::prefix_value:
			if (m_prefix_index > 0 && m_prefix_index < static_cast<int>(m_valid_prefixes.size()))
			{
				dd_x = lx; dd_y = sY + layout::row2_sel_y; dd_w = layout::col_left_w;
				dd_selected = m_prefix_value - 1;
				int mult = m_valid_prefixes[m_prefix_index].multiplier;
				for (int i = 1; i <= max_value; i++) options.push_back(std::to_string(i * mult));
			}
			break;
		case dropdown_id::effect_value:
			if (m_secondary_index > 0 && m_secondary_index < static_cast<int>(m_valid_secondaries.size()))
			{
				dd_x = rx; dd_y = sY + layout::row2_sel_y; dd_w = layout::col_right_w;
				dd_selected = m_secondary_value - 1;
				int mult = m_valid_secondaries[m_secondary_index].multiplier;
				for (int i = 1; i <= max_value; i++) options.push_back(std::to_string(i * mult));
			}
			break;
		case dropdown_id::upgrade:
			dd_x = lx; dd_y = sY + layout::row3_sel_y; dd_w = layout::col_left_w;
			dd_selected = m_enchant_value;
			for (int i = 0; i <= 15; i++) options.push_back(std::format("+{}", i));
			break;
		case dropdown_id::count:
			dd_x = rx; dd_y = sY + layout::row3_sel_y; dd_w = layout::col_right_w;
			dd_selected = m_item_count - 1;
			for (int i = 1; i <= 10; i++) options.push_back(std::to_string(i));
			break;
		default:
			break;
		}

		dd_count = static_cast<int>(options.size());
		if (dd_count > 0)
		{
			int list_y = dd_y + dropdown_h;
			int vis = std::min(dd_count, static_cast<int>(dropdown_max_vis));
			int list_h = vis * dropdown_h;

			// Clamp scroll
			int max_scroll = std::max(0, dd_count - dropdown_max_vis);
			m_dropdown_scroll = std::clamp(m_dropdown_scroll, 0, max_scroll);

			// List background + border
			m_game->m_Renderer->draw_rect_filled(dd_x, list_y, dd_w, list_h, dd_style::list_bg);
			m_game->m_Renderer->draw_rect_outline(dd_x, list_y, dd_w, list_h, dd_style::list_border);

			// Draw each visible option
			for (int i = 0; i < vis; i++)
			{
				int idx = i + m_dropdown_scroll;
				if (idx >= dd_count) break;

				int iy = list_y + i * dropdown_h;
				bool item_hover = (mouse_x >= dd_x && mouse_x <= dd_x + dd_w
					&& mouse_y >= iy && mouse_y < iy + dropdown_h);

				if (item_hover)
					m_game->m_Renderer->draw_rect_filled(dd_x + 1, iy, dd_w - 2, dropdown_h, dd_style::item_hover);

				auto color = (idx == dd_selected) ? GameColors::UIPaleYellow
					: (item_hover ? GameColors::UIWhite : GameColors::UINearWhite);
				put_string(dd_x + 4, iy + 1, options[idx].c_str(), color);
			}

			// Scrollbar indicator if list is scrollable
			if (dd_count > dropdown_max_vis)
			{
				int bar_h = std::max(8, list_h * vis / dd_count);
				int bar_y = list_y + (list_h - bar_h) * m_dropdown_scroll / max_scroll;
				m_game->m_Renderer->draw_rect_filled(dd_x + dd_w - 4, bar_y, 3, bar_h, dd_style::scrollbar);
			}
		}
	}
}

void DialogBox_ItemCreator::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
	short sX = m_x;
	short sY = m_y;
	short size_x = m_size_x;

	draw_new_dialog_box(InterfaceNdGame2, sX, sY, 0);

	if (m_page == 0)
		draw_search_page(sX, sY, size_x, mouse_x, mouse_y, z);
	else
		draw_configure_page(sX, sY, size_x, mouse_x, mouse_y, z);
}

// ---------------------------------------------------------------------------
// CLICK HANDLERS
// ---------------------------------------------------------------------------

bool DialogBox_ItemCreator::on_click_search(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// Results list — clicking goes directly to configure
	for (int i = 0; i < layout::list_rows && (i + m_scroll_offset) < m_result_count; i++)
	{
		int idx = i + m_scroll_offset;
		int ry = sY + layout::list_y + i * layout::row_h;
		if (mouse_x >= sX + layout::content_x1 && mouse_x <= sX + layout::content_x2
			&& mouse_y >= ry && mouse_y <= ry + layout::row_h - 2)
		{
			m_selected_index = idx;
			text_input_manager::get().end_input();
			build_valid_options(m_results[idx].effect_type);
			m_prefix_index = 0;
			m_prefix_value = 1;
			m_secondary_index = 0;
			m_secondary_value = 1;
			m_enchant_value = 0;
			m_item_count = 1;
			m_open_dropdown = dropdown_id::none;
			m_page = 1;
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
	}

	// Close button
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) &&
		(mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		disable_this_dialog();
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_ItemCreator::on_click_configure(short sX, short sY, short size_x)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	int lx = sX + layout::col_left_x1;
	int rx = sX + layout::col_right_x1;

	// --- STEP 1: Handle clicks when a dropdown list is open ---
	if (m_open_dropdown != dropdown_id::none)
	{
		// Determine the open dropdown's anchor and option count
		int dd_x = 0, dd_y = 0, dd_w = 0, dd_count = 0;

		switch (m_open_dropdown)
		{
		case dropdown_id::prefix_type:
			dd_x = lx; dd_y = sY + layout::row1_sel_y; dd_w = layout::col_left_w;
			dd_count = static_cast<int>(m_valid_prefixes.size());
			break;
		case dropdown_id::effect_type:
			dd_x = rx; dd_y = sY + layout::row1_sel_y; dd_w = layout::col_right_w;
			dd_count = static_cast<int>(m_valid_secondaries.size());
			break;
		case dropdown_id::prefix_value:
			dd_x = lx; dd_y = sY + layout::row2_sel_y; dd_w = layout::col_left_w;
			dd_count = max_value;
			break;
		case dropdown_id::effect_value:
			dd_x = rx; dd_y = sY + layout::row2_sel_y; dd_w = layout::col_right_w;
			dd_count = max_value;
			break;
		case dropdown_id::upgrade:
			dd_x = lx; dd_y = sY + layout::row3_sel_y; dd_w = layout::col_left_w;
			dd_count = 16;
			break;
		case dropdown_id::count:
			dd_x = rx; dd_y = sY + layout::row3_sel_y; dd_w = layout::col_right_w;
			dd_count = 10;
			break;
		default: break;
		}

		int list_y = dd_y + dropdown_h;
		int vis = std::min(dd_count, static_cast<int>(dropdown_max_vis));
		int list_h = vis * dropdown_h;

		// Click on the dropdown field itself → toggle closed
		if (mouse_x >= dd_x && mouse_x <= dd_x + dd_w
			&& mouse_y >= dd_y && mouse_y < dd_y + dropdown_h)
		{
			m_open_dropdown = dropdown_id::none;
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}

		// Click inside the expanded list → select item
		if (mouse_x >= dd_x && mouse_x <= dd_x + dd_w
			&& mouse_y >= list_y && mouse_y < list_y + list_h)
		{
			int clicked_idx = (mouse_y - list_y) / dropdown_h + m_dropdown_scroll;
			if (clicked_idx >= 0 && clicked_idx < dd_count)
			{
				switch (m_open_dropdown)
				{
				case dropdown_id::prefix_type:
					m_prefix_index = clicked_idx;
					m_prefix_value = 1;
					break;
				case dropdown_id::effect_type:
					m_secondary_index = clicked_idx;
					m_secondary_value = 1;
					break;
				case dropdown_id::prefix_value:
					m_prefix_value = clicked_idx + 1;
					break;
				case dropdown_id::effect_value:
					m_secondary_value = clicked_idx + 1;
					break;
				case dropdown_id::upgrade:
					m_enchant_value = clicked_idx;
					break;
				case dropdown_id::count:
					m_item_count = clicked_idx + 1;
					break;
				default: break;
				}
			}
			m_open_dropdown = dropdown_id::none;
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}

		// Click outside → close dropdown and fall through to normal handling
		m_open_dropdown = dropdown_id::none;
	}

	// --- STEP 2: Check clicks on dropdown fields (to open them) ---
	auto try_open_dropdown = [&](dropdown_id id, int x, int y, int w) -> bool
	{
		if (mouse_x >= x && mouse_x <= x + w
			&& mouse_y >= y && mouse_y < y + dropdown_h)
		{
			m_open_dropdown = id;
			m_dropdown_scroll = 0;
			audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
			return true;
		}
		return false;
	};

	if (m_category == item_category::none)
	{
		// Count dropdown
		if (try_open_dropdown(dropdown_id::count, rx, sY + layout::row3_sel_y, layout::col_right_w))
			return true;
	}
	else
	{
		// Row 1: Prefix type / Effect type
		if (try_open_dropdown(dropdown_id::prefix_type, lx, sY + layout::row1_sel_y, layout::col_left_w))
			return true;
		if (try_open_dropdown(dropdown_id::effect_type, rx, sY + layout::row1_sel_y, layout::col_right_w))
			return true;

		// Row 2: Prefix value / Effect value (only if type is selected)
		if (m_prefix_index > 0)
		{
			if (try_open_dropdown(dropdown_id::prefix_value, lx, sY + layout::row2_sel_y, layout::col_left_w))
				return true;
		}
		if (m_secondary_index > 0)
		{
			if (try_open_dropdown(dropdown_id::effect_value, rx, sY + layout::row2_sel_y, layout::col_right_w))
				return true;
		}

		// Row 3: Upgrade / Count
		if (try_open_dropdown(dropdown_id::upgrade, lx, sY + layout::row3_sel_y, layout::col_left_w))
			return true;
		if (try_open_dropdown(dropdown_id::count, rx, sY + layout::row3_sel_y, layout::col_right_w))
			return true;
	}

	// --- STEP 3: Create / Back / Close buttons ---
	int left_btn_x = sX + layout::col_left_x1;
	if (mouse_x >= left_btn_x && mouse_x <= left_btn_x + layout::btn_w
		&& mouse_y >= sY + layout::btn_y && mouse_y <= sY + layout::btn_y + 18)
	{
		if (m_selected_index >= 0 && m_selected_index < m_result_count)
		{
			int item_id = m_results[m_selected_index].item_id;

			int prefix_type = (m_prefix_index < static_cast<int>(m_valid_prefixes.size()))
				? m_valid_prefixes[m_prefix_index].type : 0;
			int secondary_type = (m_secondary_index < static_cast<int>(m_valid_secondaries.size()))
				? m_valid_secondaries[m_secondary_index].type : 0;
			int pval = (prefix_type != 0) ? m_prefix_value : 0;
			int sval = (secondary_type != 0) ? m_secondary_value : 0;

			// Pack attributes into legacy uint32_t format for TesterCreateItem command
			uint32_t attr = 0;
			attr |= (static_cast<uint32_t>(sval) & 0x0F) << 8;
			attr |= (static_cast<uint32_t>(secondary_type) & 0x0F) << 12;
			attr |= (static_cast<uint32_t>(pval) & 0x0F) << 16;
			attr |= (static_cast<uint32_t>(prefix_type) & 0x0F) << 20;
			attr |= (static_cast<uint32_t>(m_enchant_value) & 0x0F) << 28;
			{
				auto pkt = hb::net::make_common_command(CommonType::TesterCreateItem, player().m_player_x, player().m_player_y);
				pkt.v1 = item_id;
				pkt.v2 = static_cast<int>(attr);
				pkt.v3 = m_item_count;
				send_game_packet(pkt);
			}
		}
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	// Back button
	int right_btn_x = sX + layout::col_right_x2 - layout::btn_w;
	if (mouse_x >= right_btn_x && mouse_x <= right_btn_x + layout::btn_w
		&& mouse_y >= sY + layout::btn_y && mouse_y <= sY + layout::btn_y + 18)
	{
		m_page = 0;
		m_open_dropdown = dropdown_id::none;
		text_input_manager::get().start_input(sX + 70, sY + layout::search_bar_y + 5, 20, m_search_text);
		m_last_sx = sX;
		m_last_sy = sY;
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	// Close button
	if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) &&
		(mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
	{
		disable_this_dialog();
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_ItemCreator::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;
	short size_x = m_size_x;

	if (m_page == 0)
		return on_click_search(sX, sY, size_x);
	else
		return on_click_configure(sX, sY, size_x);
}

bool DialogBox_ItemCreator::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	int ic_x = LOGICAL_WIDTH() - 258 * 2 - 20;
	int ic_y = LOGICAL_HEIGHT() - 339 - ICON_PANEL_HEIGHT() - 10;
	m_x = ic_x;
	m_y = ic_y;
	return true;
}
#endif // TESTER_ONLY
