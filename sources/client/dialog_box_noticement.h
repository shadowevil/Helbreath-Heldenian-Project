#pragma once

#include "dialog_box.h"
#include <string>

class DialogBox_Noticement : public IDialogBox
{
public:
	explicit DialogBox_Noticement(CGame* game);

	void on_draw() override;
	bool on_click() override;

	void set_shutdown_info(uint16_t seconds, const char* message);

	bool on_enable(int type, int64_t v1, int v2, const char* string) override;

	char m_mode{};
	int m_countdown_seconds{};
	int m_param{};

private:
	std::string m_custom_message;
	uint16_t m_seconds_remaining = 0;

	static constexpr ui_rect btn_ok{210, 128, 75, 19};
};
