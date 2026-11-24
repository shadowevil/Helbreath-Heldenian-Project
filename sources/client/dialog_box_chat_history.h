#pragma once
#include "dialog_box.h"

namespace hb::client
{
	class kit_dialog;
	struct kit_rect;
}

class DialogBox_ChatHistory : public IDialogBox
{
public:
	DialogBox_ChatHistory(CGame* game);
	~DialogBox_ChatHistory() override = default;

	void on_draw() override;
	bool on_click() override;
	PressResult on_press() override;

	bool cancels_text_input_on_enable() const override { return false; }

	short m_scroll_position{};

private:
	// kit = the kit frame when the panel kit draws this dialog, else nullptr (the sprite thumb)
	void draw_scroll_bar(const hb::client::kit_dialog* kit, short sX, short sY);
	void draw_chat_messages(short sX, short sY);
	void handle_scroll_input(const hb::client::kit_dialog* kit, short sX, short sY);
	static hb::client::kit_rect kit_scrollbar();   // kit_track as a kit rect

	// Sprite frame: the 105 px track and the arrow zones
	static constexpr ui_rect scroll_track{336, 28, 26, 113};
	static constexpr ui_rect scroll_up{336, 19, 26, 9};
	static constexpr ui_rect scroll_down{336, 141, 26, 22};
	static constexpr ui_rect scroll_area{336, 18, 26, 146};
	// Panel kit: the scrollbar groove is the track (the kit draws no arrows)
	static constexpr ui_rect kit_track{340, 28, 15, 121};
};
