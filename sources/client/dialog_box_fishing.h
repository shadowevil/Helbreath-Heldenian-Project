#pragma once
#include "dialog_box.h"

class DialogBox_Fishing : public IDialogBox
{
public:
	DialogBox_Fishing(CGame* game);
	~DialogBox_Fishing() override = default;

	void on_draw() override;
	bool on_click() override;
	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
	bool on_disable() override;

	char m_mode{};
	char m_fish_name[32]{};
	int m_catch_chance{};
	int m_fish_count{};

private:
	static constexpr ui_rect btn_try_now{160, 70, 94, 21};
};
