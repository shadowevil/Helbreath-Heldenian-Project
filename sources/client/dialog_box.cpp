#include "dialog_box.h"
#include "dialog_box_manager.h"
#include "player.h"
#include "game.h"
#include "game_fonts.h"
#include "text_lib_ext.h"
#include "input.h"
#include <algorithm>
#include <iterator>
#include <string_view>

IDialogBox::IDialogBox(DialogBoxId::Type id, CGame* game)
	: m_game(game)
	, m_id(id)
{
}

bool IDialogBox::mouse_in(const ui_rect& r) const
{
	int mx = hb::shared::input::get_mouse_x();
	int my = hb::shared::input::get_mouse_y();
	return mx >= m_x + r.x && mx < m_x + r.x + r.w
		&& my >= m_y + r.y && my < m_y + r.y + r.h;
}

void IDialogBox::draw_text_button(int sX, int sY, int frame, bool is_trans)
{
	m_game->draw_text_button(sX, sY, frame, is_trans);
}

void IDialogBox::draw_new_dialog_box(char type, int sX, int sY, int frame, bool is_no_color_key, bool is_trans)
{
	m_game->draw_new_dialog_box(type, sX, sY, frame, is_no_color_key, is_trans);
}

const hb::client::panel_style& kit_frame_style(kit_frame::type style)
{
	// Built once: the kit part names per style (matched to the original sprite frames, HH-0066)
	static const hb::client::panel_style gold_box{"stone_cloth", "wood_light", "gold"};
	static const hb::client::panel_style rivet_box{"stone_cloth", "wood_light", "rivet"};
	return style == kit_frame::rivet_box ? rivet_box : gold_box;
}

void draw_kit_notice_frame(const hb::client::panel_kit& kit, int x, int y, std::string_view title, int title_y)
{
	const hb::client::kit_dialog dialog(kit, kit_layout::notice_size, hb::client::kit_point{x, y}, 1.0f,
		kit_frame_style(kit_frame::rivet_box));
	dialog.inset(kit_layout::notice_panel);
	// After the panel: the notice headings sit on it, unlike the tall dialogs' titles above theirs
	if (!title.empty())
		dialog.title(title, title_y, kit_layout::title_fill);
}

void IDialogBox::draw_kit_background(kit_frame::type style, int x, int y, int w, int h, bool transparent)
{
	m_game->get_panel_kit().compose(x, y, w, h, kit_frame_style(style), hb::client::dialog_alpha(transparent));
}

namespace
{
	// The tall dialogs' kit titles, keyed by the frame number of the old Text sprite strip they replace
	struct tall_title
	{
		int text_frame;
		std::string_view text;
		int y;   // from the baked strip's cap rows, placed as the system menu title (HH-0065)
	};
	constexpr tall_title tall_titles[] = {
		{11, "Item for Sale", 21},
		{10, "Repair Item", 22},
		{21, "Items In Storage", 22},
		{1, "Skills", 20},
		{4, "Quest", 20},
		{3, "Party Menu", 20},
		{18, "Cityhall Menu", 21},
		{19, "Guild Menu", 21},
		{2, "Level up Setting", 19},
		{5, "Item Upgrade", 19},
		// The crusade map dialogs' strips (draw_crusade_frame)
		{15, "Commander Menu", 20},
		{16, "Constructor Menu", 20},
		{17, "Soldier Menu", 20},
	};

	const tall_title* find_title(int text_frame)
	{
		const auto title = std::find_if(std::begin(tall_titles), std::end(tall_titles),
			[text_frame](const tall_title& t) { return t.text_frame == text_frame; });
		return title != std::end(tall_titles) ? title : nullptr;
	}
}

void IDialogBox::draw_tall_frame(tall_frame::type style, int x, int y, int title_frame, tall_panel::type panel)
{
	// The default panel_style is the stone-cornered frame
	static const hb::client::panel_style stone{};
	const hb::client::kit_dialog dialog(m_game->get_panel_kit(), kit_layout::tall_size, hb::client::kit_point{x, y}, 1.0f,
		style == tall_frame::rivet ? kit_frame_style(kit_frame::rivet_box) : stone);
	// A title_frame missing from tall_titles draws no kit title: add new callers' titles to the table
	if (const tall_title* title = find_title(title_frame))
		dialog.title(title->text, title->y, kit_layout::title_fill, "divider_wave", 14);
	dialog.inset(panel == tall_panel::scrollbar ? kit_layout::tall_scroll_panel : kit_layout::tall_panel);
}

void IDialogBox::draw_crusade_frame(int title_frame, int x, int y, bool transparent)
{
	// The original frame (311x373, pivot y 39, drawn 5 px up) hangs under the 34 px title strip: 407 tall in all
	constexpr hb::client::kit_point crusade_size{311, 407};
	constexpr hb::client::kit_rect status_strip{15, 34, 281, 21};   // the sunken bar under the title
	const hb::client::kit_dialog dialog(m_game->get_panel_kit(), crusade_size, hb::client::kit_point{x, y},
		hb::client::dialog_alpha(transparent), kit_frame_style(kit_frame::gold_box));
	if (const tall_title* title = find_title(title_frame))
		dialog.title(title->text, title->y, kit_layout::title_fill, "divider_wave", 14);
	dialog.inset(status_strip);
}

void IDialogBox::draw_tall_scrollbar(int x, int y, float pos)
{
	const hb::client::kit_rect& t = kit_layout::tall_track;
	m_game->get_panel_kit().scrollbar({x + t.x, y + t.y, t.w, t.h}, std::clamp(pos, 0.0f, 1.0f));
}

void IDialogBox::put_string(int iX, int iY, const char* string, const hb::shared::render::Color& color)
{
	hb::shared::text::draw_text(GameFont::Default, iX, iY, string, hb::shared::text::TextStyle::from_color(color));
}

void IDialogBox::put_aligned_string(int x1, int x2, int iY, const char* string, const hb::shared::render::Color& color)
{
	hb::shared::text::draw_text_aligned(GameFont::Default, x1, iY, x2 - x1, 15, string,
	                         hb::shared::text::TextStyle::from_color(color), hb::shared::text::Align::TopCenter);
}

void IDialogBox::add_event_list(const char* txt, char color, bool dup_allow)
{
	m_game->add_event_list(txt, color, dup_allow);
}

bool IDialogBox::send_game_packet_impl(const hb::net::packet_base& pkt, size_t size, bool encrypt)
{
	return m_game->send_game_packet_impl(pkt, size, encrypt);
}

void IDialogBox::set_default_rect(short sX, short sY, short size_x, short size_y)
{
	m_x = sX;
	m_y = sY;
	m_size_x = size_x;
	m_size_y = size_y;
}

void IDialogBox::enable_dialog_box(DialogBoxId::Type id, int type, int64_t v1, int v2, const char* string)
{
	m_manager->enable_dialog_box(id, type, v1, v2, string);
}

void IDialogBox::disable_dialog_box(DialogBoxId::Type id)
{
	m_manager->disable_dialog_box(id);
}

void IDialogBox::disable_this_dialog()
{
	m_manager->disable_dialog_box(m_id);
}

IDialogBox* IDialogBox::get_dialog_box(DialogBoxId::Type id)
{
	return m_manager->get_dialog_box(id);
}

CPlayer& IDialogBox::player() const
{
	return m_manager->get_player();
}
