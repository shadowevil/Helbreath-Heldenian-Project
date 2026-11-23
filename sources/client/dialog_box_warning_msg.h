#pragma once

#include "dialog_box.h"

class DialogBox_WarningMsg : public IDialogBox
{
public:
	explicit DialogBox_WarningMsg(CGame* game);

	void on_draw() override;
	bool on_click() override;
	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
	bool on_disable() override;

	short m_item_index{};

private:
	static constexpr ui_rect btn_ok{122, 127, 78, 21};
};
