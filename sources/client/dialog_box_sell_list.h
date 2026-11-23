#pragma once
#include "dialog_box.h"
#include "game_constants.h"

class DialogBox_SellList : public IDialogBox
{
public:
	DialogBox_SellList(CGame* game);
	~DialogBox_SellList() override = default;

	void on_draw() override;
	bool on_click() override;
	bool on_item_drop() override;

	bool on_disable() override;

	struct SellEntry { int index = -1; int amount = 0; };

	void reset();
	bool add_item(int index, int amount);
	const SellEntry& get_entry(int i) const;

private:
	SellEntry m_items[game_limits::max_sell_list]{};

	void draw_item_list(short sX, short sY, short size_x, short mouse_x, short mouse_y, int& empty_count);
	void draw_empty_list_message(short sX, short sY, short size_x);
	void draw_buttons(short sX, short sY, short mouse_x, short mouse_y, bool has_items);
};
