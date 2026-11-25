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
	// kit = this frame's kit dialog (on_draw's)
	void draw_scroll_bar(const hb::client::kit_dialog& kit);
	void draw_chat_messages(short sX, short sY);
	void handle_scroll_input(const hb::client::kit_dialog& kit);
	static hb::client::kit_rect kit_scrollbar();   // kit_track as a kit rect

	// The scrollbar groove is the track (the kit draws no arrows)
	static constexpr ui_rect kit_track{340, 28, 15, 121};
};
