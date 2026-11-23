#pragma once
#include "dialog_box.h"

class DialogBox_Slates : public IDialogBox
{
public:
	DialogBox_Slates(CGame* game);
	~DialogBox_Slates() override = default;

	void on_draw() override;
	bool on_click() override;
	bool on_item_drop() override;
	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
	bool on_disable() override;

	enum class mode : uint8_t
	{
		waiting = 1,
		casting = 2,
	};
	mode m_mode{mode::waiting};
	int m_slot_ul{-1};
	int m_slot_ll{-1};
	int m_slot_ur{-1};
	int m_slot_lr{-1};
	int m_slot_extra1{-1};
	int m_slot_extra2{-1};
	uint32_t m_anim_tick{};
	char m_anim_amplitude{};

private:
	static constexpr ui_rect link_cast{120, 150, 61, 16};
};
