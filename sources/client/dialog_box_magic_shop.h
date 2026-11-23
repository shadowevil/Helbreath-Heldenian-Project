#pragma once

#include "dialog_box.h"

class DialogBox_MagicShop : public IDialogBox
{
public:
	DialogBox_MagicShop(CGame* game);
	~DialogBox_MagicShop() override = default;

	void on_draw() override;
	bool on_click() override;

	bool on_enable(int type, int64_t v1, int v2, const char* string) override;

	char m_mode{};
	short m_page{};

private:
	void draw_spell_list(short sX, short sY);
	void draw_page_indicator(short sX, short sY);
	bool handle_spell_click(short sX, short sY);
	void handle_page_click(short sX, short sY);
};
