#pragma once
#include "dialog_box.h"

class DialogBox_ItemDropAmount : public IDialogBox
{
public:
	DialogBox_ItemDropAmount(CGame* game);
	~DialogBox_ItemDropAmount() override = default;

	void on_draw() override;
	bool on_click() override;
	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
	bool on_disable() override;

	enum class mode : uint8_t
	{
		input = 1,
		selected = 20,
	};

	mode m_mode{mode::input};
	short m_item_index{};
	char m_label[32]{};
	int m_max_amount{};

	int m_drop_x{};
	int m_drop_y{};
	int m_drop_target_type{};
	int m_drop_target_id{};
	char m_target_name[32]{};
};
