#pragma once
#include "IDialogBox.h"

class DialogBox_SellOrRepair : public IDialogBox
{
public:
	DialogBox_SellOrRepair(CGame* game);
	~DialogBox_SellOrRepair() override = default;

	void on_draw() override;
	bool on_click() override;
	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
	bool on_disable() override;

	enum class mode : uint8_t
	{
		sell = 1,
		repair = 2,
		sell_pending = 3,
		repair_pending = 4,
	};

	mode m_mode{mode::sell};
	int m_item_index{};
	int m_sell_price{};
	int m_secondary_price{};
	int m_item_count{};

private:
	static constexpr ui_rect btn_confirm{30, 292, 75, 21};
	static constexpr ui_rect btn_cancel{154, 292, 75, 21};
};
