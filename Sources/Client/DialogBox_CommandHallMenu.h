#pragma once
#include "IDialogBox.h"

class DialogBox_CommandHallMenu : public IDialogBox
{
public:
	DialogBox_CommandHallMenu(CGame* game);
	~DialogBox_CommandHallMenu() override = default;

	void on_draw() override;
	bool on_click() override;

	enum class mode : uint8_t
	{
		main_menu = 0,
		teleport = 1,
		hire_soldier = 2,
		take_flag = 3,
		tutelary_angel = 4,
	};

	mode m_mode{mode::main_menu};

private:
	// Menu link rows (shared X range, 25px row spacing)
	static constexpr ui_rect link_1{36, 71, 184, 24};
	static constexpr ui_rect link_2{36, 96, 184, 24};
	static constexpr ui_rect link_3{36, 121, 184, 24};
	static constexpr ui_rect link_4{36, 146, 184, 24};
	static constexpr ui_rect link_5{36, 171, 184, 24};
	static constexpr ui_rect link_6{36, 196, 184, 24};

	// Mode 3: take flag
	static constexpr ui_rect link_take_flag{35, 140, 186, 26};

	// Mode 4: angel options
	static constexpr ui_rect link_angel_str{36, 176, 184, 24};
	static constexpr ui_rect link_angel_dex{36, 201, 184, 24};
	static constexpr ui_rect link_angel_int{36, 226, 184, 24};
	static constexpr ui_rect link_angel_mag{36, 251, 184, 24};
};
