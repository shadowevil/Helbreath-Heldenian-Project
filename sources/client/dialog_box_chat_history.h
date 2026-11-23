#pragma once
#include "dialog_box.h"

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
	void draw_scroll_bar(short sX, short sY);
	void draw_chat_messages(short sX, short sY);
	void handle_scroll_input(short sX, short sY);

	static constexpr ui_rect scroll_track{336, 28, 26, 113};
	static constexpr ui_rect scroll_up{336, 19, 26, 9};
	static constexpr ui_rect scroll_down{336, 141, 26, 22};
	static constexpr ui_rect scroll_area{336, 18, 26, 146};
};
