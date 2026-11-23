#pragma once
#include "dialog_box.h"

class DialogBox_Commander : public IDialogBox
{
public:
	DialogBox_Commander(CGame* game);
	~DialogBox_Commander() override = default;

	void on_update() override;
	void on_draw() override;
	bool on_click() override;

	enum class mode : uint8_t
	{
		main = 0,
		set_tp = 1,
		use_tp = 2,
		summon = 3,
		set_construct = 4,
	};

	mode m_mode{mode::main};
	int m_selected_faction{};

private:
	// Toolbar buttons (mode 0)
	static constexpr ui_rect btn_set_tp{20, 340, 47, 53};
	static constexpr ui_rect btn_use_tp{70, 340, 47, 53};
	static constexpr ui_rect btn_summon{120, 340, 47, 53};
	static constexpr ui_rect btn_set_construct{170, 340, 47, 53};
	// Shared toolbar buttons (multiple modes)
	static constexpr ui_rect btn_back{194, 340, 47, 53};
	static constexpr ui_rect btn_help{244, 340, 47, 53};
	// Unit buttons (mode 3: summon)
	static constexpr ui_rect btn_unit_1{20, 220, 47, 51};
	static constexpr ui_rect btn_unit_2{70, 220, 46, 51};
	static constexpr ui_rect btn_unit_3{120, 220, 46, 51};
	static constexpr ui_rect btn_unit_4{170, 220, 46, 51};
	// Faction selection links (mode 3)
	static constexpr ui_rect link_faction_1{20, 141, 361, 19};
	static constexpr ui_rect link_faction_2{20, 161, 361, 14};
	// Map area (modes 1, 4 for clicks; 0,1,2,4 for display)
	static constexpr ui_rect area_map{15, 60, 279, 273};
};
