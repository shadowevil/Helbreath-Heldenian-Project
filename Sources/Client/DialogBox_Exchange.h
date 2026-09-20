#pragma once
#include "IDialogBox.h"
#include "Item/ItemInstanceData.h"
#include <string>
#include <cstdint>

class DialogBox_Exchange : public IDialogBox
{
public:
	DialogBox_Exchange(CGame* game);
	~DialogBox_Exchange() override = default;

	void on_draw() override;
	bool on_click() override;
	bool on_item_drop() override;

	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
	bool on_disable() override;

	enum class mode : uint8_t
	{
		pending = 1,
		confirmed = 2,
	};
	mode m_mode{mode::pending};

	struct ExchangeSlot {
		int v1 = -1, v2 = -1, v3 = -1, v4 = -1, v5 = -1, v6 = -1, v7 = -1;
		int item_id = -1, inv_slot = -1;
		hb::shared::item::item_instance_data item_data;
		std::string str1, str2;
	};

	ExchangeSlot m_slots[8];
	void reset_slots();
	int find_empty_slot(int start, int end) const;

private:
	void draw_items(short sX, short sY, short mouse_x, short mouse_y, int start_index, int end_index);
	void draw_item_info(short sX, short sY, short size_x, short mouse_x, short mouse_y, int item_index, short xadd);

	static constexpr ui_rect btn_exchange{200, 310, 75, 21};
	static constexpr ui_rect btn_cancel{450, 310, 75, 21};
};
