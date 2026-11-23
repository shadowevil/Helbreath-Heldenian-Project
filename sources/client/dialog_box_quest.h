#pragma once
#include "dialog_box.h"

class DialogBox_Quest : public IDialogBox
{
public:
	DialogBox_Quest(CGame* game);
	~DialogBox_Quest() override = default;

	void on_draw() override;
	bool on_click() override;
	bool on_enable(int type, int64_t v1, int v2, const char* string) override;

	enum class mode : uint8_t
	{
		details = 1,
		unavailable = 2,
	};
	mode m_mode{mode::details};

private:
	static constexpr ui_rect btn_ok{154, 293, 75, 19};
};
