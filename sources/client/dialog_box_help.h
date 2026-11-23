#pragma once
#include "dialog_box.h"

class DialogBox_Help : public IDialogBox
{
public:
	DialogBox_Help(CGame* game);
	~DialogBox_Help() override = default;

	void on_draw() override;
	bool on_click() override;

private:
	bool is_mouse_over_item(short mouse_x, short mouse_y, short sX, short sY, int item);
	void draw_help_item(short sX, short size_x, short sY, int item, const char* text, bool highlight);
};
