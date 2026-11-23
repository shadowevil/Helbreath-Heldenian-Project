#pragma once
#include "dialog_box.h"

class DialogBox_Map : public IDialogBox
{
public:
	DialogBox_Map(CGame* game);
	~DialogBox_Map() override = default;

	void on_draw() override;
	bool on_click() override;

	bool on_enable(int type, int64_t v1, int v2, const char* string) override;

	int m_map_zone{};
	int m_map_id{};
};
