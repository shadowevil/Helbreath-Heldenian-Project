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

void IDialogBox::draw_new_dialog_box(char type, int sX, int sY, int frame, bool is_no_color_key, bool is_trans)
{
	m_game->draw_new_dialog_box(type, sX, sY, frame, is_no_color_key, is_trans);
}

bool IDialogBox::draw_kit_background(kit_frame::type style, int x, int y, int w, int h, bool transparent)
{
	hb::client::panel_kit* kit = m_game->get_panel_kit();
	if (!kit)
		return false;
	// Built once: the kit part names per style (matched to the original sprite frames, HH-0066)
	static const hb::client::panel_style gold_box{"stone_cloth", "wood_light", "gold"};
	switch (style)
	{
	case kit_frame::gold_box:
	default:
		kit->compose(x, y, w, h, gold_box, hb::client::dialog_alpha(transparent));
		break;
	}
	return true;
}

namespace
{
	// The shop family frame (Game2 frame 2), dialog-relative, matched to the original (HH-0066)
	constexpr hb::client::kit_point shop_size{258, 339};
	constexpr hb::shared::render::Color shop_title_fill{148, 24, 16};
	// The list panel; 220 wide (the original ran to x 241 under its track overlay) so the groove sits beside it
	constexpr hb::client::kit_rect shop_panel{16, 31, 220, 288};
	// The thumb's top at 30 + 274 * pos, where the sprite thumb drew (frame 7, pivot -6,-5, at x 242, y 35 + loc)
	constexpr hb::client::kit_rect shop_track{236, 30, 15, 274 + 15};

	struct shop_title
	{
		int text_frame;
		std::string_view text;
		int y;   // the baked strip's cap rows, placed as the system menu title (HH-0065)
	};
	constexpr shop_title shop_titles[] = {
		{11, "Item for Sale", 21},
		{10, "Repair Item", 22},
		{21, "Items In Storage", 22},
	};
}

void IDialogBox::draw_shop_frame(int title_frame, int x, int y)
{
	using namespace hb::client::sprite_id;
	hb::client::panel_kit* kit = m_game->get_panel_kit();
	const auto title = std::find_if(std::begin(shop_titles), std::end(shop_titles),
		[title_frame](const shop_title& t) { return t.text_frame == title_frame; });
	if (kit && title != std::end(shop_titles))
	{
		static const hb::client::panel_style shop_style{"stone_cloth", "wood_light", "rivet"};
		const hb::client::kit_dialog dialog(*kit, shop_size, hb::client::kit_point{x, y}, 1.0f, shop_style);
		dialog.title(title->text, title->y, shop_title_fill, "divider_wave", 14);
		dialog.inset(shop_panel);
	}
	else
	{
		draw_new_dialog_box(InterfaceNdGame2, x, y, 2);
		draw_new_dialog_box(InterfaceNdText, x, y, title_frame);
	}
}

void IDialogBox::draw_shop_scrollbar(int x, int y, float pos, int pointer_loc, int track_frame)
{
	using namespace hb::client::sprite_id;
	if (hb::client::panel_kit* kit = m_game->get_panel_kit())
	{
		kit->scrollbar({x + shop_track.x, y + shop_track.y, shop_track.w, shop_track.h}, std::clamp(pos, 0.0f, 1.0f));
	}
	else
	{
		draw_new_dialog_box(InterfaceNdGame2, x, y, track_frame);
		draw_new_dialog_box(InterfaceNdGame2, x + 242, y + pointer_loc + 35, 7);
	}
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
