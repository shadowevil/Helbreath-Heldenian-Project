#pragma once
#include "dialog_box.h"

class DialogBox_Shop : public IDialogBox
{
public:
    DialogBox_Shop(CGame* game);
    ~DialogBox_Shop() override = default;

    void on_draw() override;
    bool on_click() override;
    PressResult on_press() override;

	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
	bool on_disable() override;

	// m_mode: 0 = item list, >0 = item detail (m_mode - 1 = item index)
	char m_mode{};
	short m_scroll_offset{};
	bool m_items_loaded{};
	int m_npc_id{};
	int m_quantity{};

private:
    void draw_item_list(short sX, short sY);
    void draw_item_details(short sX, short sY, short mouse_x, short mouse_y, short z);
    void draw_weapon_stats(short sX, short sY, int item_index, bool& flag_red_shown);
    void draw_shield_stats(short sX, short sY, int item_index, bool& flag_red_shown);
    void draw_armor_stats(short sX, short sY, int item_index, bool& flag_stat_low, bool& flag_red_shown);
    void draw_level_requirement(short sX, short sY, int item_index, bool& flag_red_shown);
    void draw_quantity_selector(short sX, short sY, short mouse_x, short mouse_y, short z);
    int calculate_discounted_price(int item_index);

    bool on_click_item_list(short sX, short sY);
    bool on_click_item_details(short sX, short sY);

	static constexpr ui_rect btn_buy{30, 292, 75, 21};
	static constexpr ui_rect btn_cancel{154, 292, 75, 21};
	static constexpr ui_rect btn_qty_up_1000{117, 209, 18, 22};
	static constexpr ui_rect btn_qty_down_1000{117, 234, 18, 18};
	static constexpr ui_rect btn_qty_up_100{131, 209, 18, 22};
	static constexpr ui_rect btn_qty_down_100{131, 234, 18, 18};
	static constexpr ui_rect btn_qty_up_10{145, 209, 18, 22};
	static constexpr ui_rect btn_qty_down_10{145, 234, 18, 18};
	static constexpr ui_rect btn_qty_up_1{159, 209, 18, 22};
	static constexpr ui_rect btn_qty_down_1{159, 234, 18, 18};
	static constexpr ui_rect area_scroll{240, 20, 21, 311};

	int get_max_quantity() const;
};
