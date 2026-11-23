#pragma once
#include "dialog_box.h"

class DialogBox_Constructor : public IDialogBox
{
public:
	DialogBox_Constructor(CGame* game);
	~DialogBox_Constructor() override = default;

	void on_update() override;
	void on_draw() override;
	bool on_click() override;

	enum class mode : uint8_t
	{
		main = 0,
		select_building = 1,
		teleport = 2,
	};

	mode m_mode{mode::main};

private:
	// Mode 0 & 2: bottom toolbar buttons (y=340)
	static constexpr ui_rect btn_construct{20, 340, 47, 53};
	static constexpr ui_rect btn_set_tp{70, 340, 47, 53};
	static constexpr ui_rect btn_help_main{244, 340, 47, 53};
	static constexpr ui_rect btn_back_tp{194, 340, 47, 53};

	// Mode 1: building selection buttons (y=220)
	static constexpr ui_rect btn_building_1{20, 220, 47, 51};
	static constexpr ui_rect btn_building_2{70, 220, 46, 51};
	static constexpr ui_rect btn_building_3{120, 220, 46, 51};
	static constexpr ui_rect btn_building_4{170, 220, 46, 51};
	static constexpr ui_rect btn_back_build{194, 322, 47, 53};
	static constexpr ui_rect btn_help_build{244, 322, 47, 53};

	// Map overlay area
	static constexpr ui_rect area_map{15, 60, 279, 273};
};
