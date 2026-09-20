#pragma once
#include "IDialogBox.h"

class DialogBox_GuildOperation : public IDialogBox
{
public:
	DialogBox_GuildOperation(CGame* game);
	~DialogBox_GuildOperation() override = default;

	void on_draw() override;
	bool on_click() override;

	void put(const char* name, char op_mode);
	void shift();
	void reset();
};
