#pragma once
#include "IDialogBox.h"
#include "NetConstants.h"
#include <array>

class DialogBox_RepairAll : public IDialogBox
{
public:
	DialogBox_RepairAll(CGame* game);
	~DialogBox_RepairAll() override = default;

	void on_draw() override;
	bool on_click() override;
	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
	bool on_disable() override;

	char m_mode{};
	short m_scroll_offset{};
	std::array<bool, hb::shared::limits::MaxItems> m_locked_by_us{};

private:
	static constexpr ui_rect btn_repair{30, 292, 75, 21};
	static constexpr ui_rect btn_cancel{154, 292, 75, 21};
};
