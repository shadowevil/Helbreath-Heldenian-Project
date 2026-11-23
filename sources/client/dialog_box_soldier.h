#pragma once
#include "dialog_box.h"

class DialogBox_Soldier : public IDialogBox
{
public:
	DialogBox_Soldier(CGame* game);
	~DialogBox_Soldier() override = default;

	void on_update() override;
	void on_draw() override;
	bool on_click() override;

	enum class mode : uint8_t
	{
		overview = 0,
		teleport = 1,
	};
	mode m_mode{mode::overview};

private:
	static constexpr ui_rect btn_left{20, 340, 47, 53};
	static constexpr ui_rect btn_right{244, 340, 47, 53};
	static constexpr ui_rect btn_middle{194, 340, 47, 53};
	static constexpr ui_rect btn_tp_wide{20, 340, 97, 53};
	static constexpr ui_rect area_map{15, 60, 279, 273};
};
