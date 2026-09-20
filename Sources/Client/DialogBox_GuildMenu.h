#pragma once
#include "IDialogBox.h"

class DialogBox_GuildMenu : public IDialogBox
{
public:
	DialogBox_GuildMenu(CGame* game);
	~DialogBox_GuildMenu() override = default;

	void on_draw() override;
	bool on_click() override;

	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
	bool on_disable() override;
};
