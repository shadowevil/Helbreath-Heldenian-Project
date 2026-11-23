#pragma once
#include "dialog_box.h"

class DialogBox_ConfirmExchange : public IDialogBox
{
public:
	DialogBox_ConfirmExchange(CGame* game);
	~DialogBox_ConfirmExchange() override = default;

	void on_draw() override;
	bool on_click() override;

	enum class mode : uint8_t
	{
		question = 1,
		waiting = 2,
	};
	mode m_mode{mode::question};

private:
	static constexpr ui_rect btn_yes{30, 55, 75, 21};
	static constexpr ui_rect btn_no{170, 55, 75, 21};
};
