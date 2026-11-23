#pragma once
#include "dialog_box.h"

class DialogBox_ItemDrop : public IDialogBox
{
public:
	DialogBox_ItemDrop(CGame* game);
	~DialogBox_ItemDrop() override = default;

	void on_draw() override;
	bool on_click() override;
	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
	bool on_disable() override;

	char m_mode{};
	short m_item_index{};
	char m_name[32]{};

private:
	static constexpr ui_rect btn_yes{30, 55, 75, 21};
	static constexpr ui_rect btn_no{170, 55, 75, 21};
	static constexpr ui_rect link_toggle{35, 80, 206, 11};
};
