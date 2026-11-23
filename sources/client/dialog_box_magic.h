#pragma once
#include "dialog_box.h"

class DialogBox_Magic : public IDialogBox
{
public:
	DialogBox_Magic(CGame* game);
	~DialogBox_Magic() override = default;

	void on_draw() override;
	bool on_click() override;
	bool on_enable(int type, int64_t v1, int v2, const char* string) override;

	short m_circle_view{};

private:
	// Circle selector buttons (irregular widths for hand-drawn font)
	static constexpr ui_rect circle_0{16, 240, 23, 29};
	static constexpr ui_rect circle_1{39, 240, 18, 29};
	static constexpr ui_rect circle_2{57, 240, 25, 29};
	static constexpr ui_rect circle_3{82, 240, 20, 29};
	static constexpr ui_rect circle_4{102, 240, 15, 29};
	static constexpr ui_rect circle_5{117, 240, 21, 29};
	static constexpr ui_rect circle_6{138, 240, 28, 29};
	static constexpr ui_rect circle_7{166, 240, 32, 29};
	static constexpr ui_rect circle_8{198, 240, 20, 29};
	static constexpr ui_rect circle_9{218, 240, 22, 29};
	static constexpr ui_rect btn_alchemy{154, 285, 75, 21};
};
