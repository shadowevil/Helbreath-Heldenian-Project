#pragma once

#include "dialog_box.h"

class DialogBox_Resurrect : public IDialogBox
{
public:
	explicit DialogBox_Resurrect(CGame* game);

	void on_draw() override;
	bool on_click() override;
	bool on_enable(int type, int64_t v1, int v2, const char* string) override;

	char m_mode{};
	short m_view{};

private:
	static constexpr ui_rect btn_yes{30, 55, 75, 21};
	static constexpr ui_rect btn_no{170, 55, 75, 21};
};
