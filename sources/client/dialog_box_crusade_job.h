#pragma once
#include "dialog_box.h"

class DialogBox_CrusadeJob : public IDialogBox
{
public:
	DialogBox_CrusadeJob(CGame* game);
	~DialogBox_CrusadeJob() override = default;

	void on_draw() override;
	bool on_click() override;

	bool on_enable(int type, int64_t v1, int v2, const char* string) override;

	enum class mode : uint8_t
	{
		select_job = 1,
		confirm = 2,
	};
	mode m_mode{mode::select_job};
	int m_npc_type{};

private:
	void draw_mode_select_job(short sX, short sY);
	void draw_mode_confirm(short sX, short sY);

	static constexpr ui_rect link_job_1{25, 151, 220, 13};
	static constexpr ui_rect link_job_2{25, 176, 220, 13};
	static constexpr ui_rect btn_help{211, 296, 49, 21};
	static constexpr ui_rect link_details{25, 161, 220, 13};
	static constexpr ui_rect btn_ok{154, 293, 75, 19};
};
