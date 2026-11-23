#pragma once
#include "dialog_box.h"

class DialogBox_Skill : public IDialogBox
{
public:
	DialogBox_Skill(CGame* game);
	~DialogBox_Skill() override = default;

	void on_draw() override;
	bool on_click() override;
	PressResult on_press() override;

	char m_mode{};
	short m_scroll_position{};
	bool m_is_down_skill_pending{};

private:
	static constexpr ui_rect area_scroll{240, 30, 21, 291};
};
