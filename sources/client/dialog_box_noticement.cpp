#include "dialog_box_noticement.h"
#include "game.h"
#include "game_fonts.h"
#include "lan_eng.h"
#include "text_lib.h"
#include "sprite_collection.h"
#include "sprite.h"
#include <format>
#include <string>
#include "input.h"
#include "audio_manager.h"
using namespace hb::client::sprite_id;

DialogBox_Noticement::DialogBox_Noticement(CGame* game)
	: IDialogBox(DialogBoxId::Noticement, game)
{
	set_default_rect(0, 0, 315, 171);
}

void DialogBox_Noticement::set_shutdown_info(uint16_t seconds, const char* message)
{
	m_seconds_remaining = seconds;
	if (message != nullptr && message[0] != '\0')
		m_custom_message = message;
}

void DialogBox_Noticement::on_draw()
{
	// Center the dialog on screen (same method as Connecting overlay)
	auto rect = m_game->m_sprite[InterfaceNdGame4]->GetFrameRect(2);
	short sX = static_cast<short>((LOGICAL_WIDTH() - rect.width) / 2);
	short sY = static_cast<short>((LOGICAL_HEIGHT() - rect.height) / 2);
	short size_x = static_cast<short>(rect.width);

	// Update stored position for click handling
	m_x = sX;
	m_y = sY;
	m_size_x = size_x;
	m_size_y = static_cast<short>(rect.height);

	draw_new_dialog_box(InterfaceNdGame4, sX, sY, 2);

	using namespace hb::shared::text;
	const auto title_style = TextStyle::with_highlight(GameColors::UIDarkRed);
	constexpr int pad_left = 18;
	constexpr int pad_right = 18;
	int text_width = size_x - pad_left - pad_right;

	// Title line: countdown text (same font/color as Connecting overlay)
	std::string title;
	if (m_seconds_remaining == 0)
	{
		title = "Server shutting down now!";
	}
	else if (m_seconds_remaining >= 120)
	{
		int minutes = m_seconds_remaining / 60;
		title = std::format("Server shutting down in {} minute{}!", minutes, minutes != 1 ? "s" : "");
	}
	else
	{
		title = std::format("Server shutting down in {} second{}!", m_seconds_remaining, m_seconds_remaining != 1 ? "s" : "");
	}

	draw_text_aligned(GameFont::Bitmap1, sX + pad_left, sY + 31, text_width, 15,
		title.c_str(), title_style, Align::TopCenter);

	// Custom message or default warning (word-wrapped)
	int msg_y = sY + 52;
	int msg_height = m_size_y - 52 - 40; // Leave room for OK button

	if (!m_custom_message.empty())
	{
		draw_text_wrapped(GameFont::Default, sX + pad_left, msg_y, text_width, msg_height,
			m_custom_message.c_str(), TextStyle::from_color(GameColors::UIBlack), Align::TopCenter);
	}
	else
	{
		draw_text_wrapped(GameFont::Default, sX + pad_left, msg_y, text_width, msg_height,
			DRAW_DIALOGBOX_NOTICEMSG1, TextStyle::from_color(GameColors::UIBlack), Align::TopCenter);
	}

	// OK button
	if (mouse_in(btn_ok))
		draw_new_dialog_box(InterfaceNdButton, sX + 210, sY + 127, 1);
	else
		draw_new_dialog_box(InterfaceNdButton, sX + 210, sY + 127, 0);
}

bool DialogBox_Noticement::on_click()
{
	// OK button
	if (mouse_in(btn_ok))
	{
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		disable_this_dialog();
		return true;
	}

	return false;
}

bool DialogBox_Noticement::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	m_mode = type;
	m_countdown_seconds = static_cast<int>(v1);
	m_param = v2;
	return true;
}
