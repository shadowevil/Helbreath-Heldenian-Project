#pragma once
#include "dialog_box.h"

class DialogBox_GuideMap : public IDialogBox
{
public:
	DialogBox_GuideMap(CGame* game);
	~DialogBox_GuideMap() override = default;

	void on_update() override;
	void on_draw() override;
	bool on_click() override;
	bool on_double_click() override;

private:
	void draw_border(short sX, short sY);
	void draw_zoomed_map(short sX, short sY);
	void draw_full_map(short sX, short sY);
	void draw_location_tooltip(short mouse_x, short mouse_y, short sX, short sY);
};
