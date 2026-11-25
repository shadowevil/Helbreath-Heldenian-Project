#pragma once
#include "dialog_box.h"

class DialogBox_Manufacture : public IDialogBox
{
public:
	DialogBox_Manufacture(CGame* game);
	~DialogBox_Manufacture() override = default;

	void on_draw() override;
	bool on_click() override;
	PressResult on_press() override;
	bool on_item_drop() override;

	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
	bool on_disable() override;
private:
	void draw_frame(short sX, short sY);
	void draw_alchemy_waiting(short sX, short sY);
	void draw_alchemy_creating(short sX, short sY);
	void draw_manufacture_list(short sX, short sY);
	void draw_manufacture_waiting(short sX, short sY);
	void draw_manufacture_in_progress(short sX, short sY);
	void draw_manufacture_done(short sX, short sY);
	void draw_crafting_waiting(short sX, short sY);
	void draw_crafting_in_progress(short sX, short sY);
	void reset_item_slots();

	// Press helpers
	bool check_slot_item_click(int slotIndex, int itemIdx, int drawX, int drawY);

	// Item drop helper: Tries to add item to first empty slot, returns true if successful
	bool try_add_item_to_slot(int item_id, bool updateBuildStatus);

	// Hit-test rects (dialog-relative, adj offsets baked in)
	static constexpr ui_rect btn_try_now{65, 183, 94, 21};          // alchemy + crafting "Try Now!"
	static constexpr ui_rect btn_back_mfg{31, 346, 64, 20};         // manufacture back
	static constexpr ui_rect btn_manufacture{159, 346, 96, 20};     // manufacture confirm

public:
	enum class mode : uint8_t
	{
		alchemy_waiting = 1,
		alchemy_creating = 2,
		manufacture_list = 3,
		manufacture_waiting = 4,
		manufacture_in_progress = 5,
		manufacture_done = 6,
		crafting_waiting = 7,
		crafting_in_progress = 8,
	};
	mode m_mode{mode::alchemy_waiting};
	short m_scroll_view{};
	int m_slot_1{-1};
	int m_slot_2{-1};
	int m_slot_3{-1};
	int m_slot_4{-1};
	int m_slot_5{-1};
	int m_slot_6{-1};
	uint32_t m_anim_timer{};
	char m_progress{};
	char m_anim_frame{};
	char m_result_flag{};
	char m_result_value{};
	char m_recipe_valid{};
};
