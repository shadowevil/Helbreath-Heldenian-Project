#pragma once
#include "dialog_box.h"

class DialogBox_ChangeStatsMajestic : public IDialogBox
{
public:
	DialogBox_ChangeStatsMajestic(CGame* game);
	~DialogBox_ChangeStatsMajestic() override = default;

	void on_draw() override;
	bool on_click() override;

	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
	bool on_disable() override;

	char m_mode{};
	short m_view{};

private:
	void draw_stat_row(short sX, short sY, int y_offset, const char* label,
		int current_stat, int16_t pending_change, short mouse_x, short mouse_y,
		int arrow_y_offset, bool can_undo, bool can_reduce);

	static constexpr ui_rect btn_cancel{30, 293, 75, 19};
	static constexpr ui_rect btn_confirm{154, 293, 75, 19};
};
