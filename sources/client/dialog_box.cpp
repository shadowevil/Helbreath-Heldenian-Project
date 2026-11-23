#include "dialog_box.h"
#include "dialog_box_manager.h"
#include "player.h"
#include "game.h"
#include "game_fonts.h"
#include "text_lib_ext.h"
#include "input.h"

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
