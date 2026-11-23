#pragma once
#include "dialog_box.h"

class DialogBox_LevelUpSetting : public IDialogBox
{
public:
	DialogBox_LevelUpSetting(CGame* game);
	~DialogBox_LevelUpSetting() override = default;

	void on_draw() override;
	bool on_click() override;

	bool on_enable(int type, int64_t v1, int v2, const char* string) override;

	int m_initial_lu_points{};

private:
	void draw_stat_row(short sX, short sY, int y_offset, const char* label,
	                 int current_stat, int pending_change, short mouse_x, short mouse_y,
	                 int arrow_y_offset, bool can_increase, bool can_decrease);

	bool handle_stat_click(short mouse_x, short mouse_y, short sX, short sY,
	                     int y_offset, int& current_stat, int16_t& pending_change);
};
