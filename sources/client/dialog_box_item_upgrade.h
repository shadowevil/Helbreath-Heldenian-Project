#pragma once
#include "dialog_box.h"

class DialogBox_ItemUpgrade : public IDialogBox
{
public:
    DialogBox_ItemUpgrade(CGame* game);
    ~DialogBox_ItemUpgrade() override = default;

    void on_draw() override;
    bool on_click() override;
    bool on_item_drop() override;

	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
	bool on_disable() override;

	enum class mode : uint8_t
	{
		gizon_upgrade = 1,
		in_progress = 2,
		success = 3,
		failed = 4,
		select_upgrade_type = 5,
		stone_upgrade = 6,
		item_lost = 7,
		max_upgrade = 8,
		cannot_upgrade = 9,
		no_points = 10,
		need_stone = 12,
	};
	mode m_mode{mode::gizon_upgrade};
	int m_selected_item_index{-1};
	int m_stone_xelima_count{};
	int m_stone_merien_count{};
	uint32_t m_upgrade_start_time{};

private:
    // draw helpers for each mode
    void DrawMode1_GizonUpgrade(int sX, int sY);
    void DrawMode2_InProgress(int sX, int sY);
    void DrawMode3_Success(int sX, int sY);
    void DrawMode4_Failed(int sX, int sY);
    void DrawMode5_SelectUpgradeType(int sX, int sY);
    void DrawMode6_StoneUpgrade(int sX, int sY);
    void DrawMode7_ItemLost(int sX, int sY);
    void DrawMode8_MaxUpgrade(int sX, int sY);
    void DrawMode9_CannotUpgrade(int sX, int sY);
    void DrawMode10_NoPoints(int sX, int sY);

    // Shared drawing helper
    void draw_item_preview(int sX, int sY, int item_index);
    int calculate_upgrade_cost(int item_index);

    // Mode 5 menu link rects (dialog-relative)
    static constexpr ui_rect link_normal_upgrade{25, 101, 223, 14};
    static constexpr ui_rect link_majestic_upgrade{25, 121, 223, 14};
};
