// TESTER MENU — entire file is tester-only
#pragma once
#ifdef TESTER_ONLY
#include "dialog_box.h"
#include "packet/packet_notify.h"

class DialogBox_TesterMenu : public IDialogBox
{
public:
	DialogBox_TesterMenu(CGame* game);
	~DialogBox_TesterMenu() override = default;

	void on_draw() override;
	bool on_click() override;

	void receive_map_list(const hb::net::PacketNotifyTesterMapListResult* pkt);

	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
private:
	static constexpr int action_count = 10;
	static constexpr int row_height = 18;
	static constexpr int first_row_y = 60;
	static constexpr int row_x1 = 20;
	static constexpr int row_x2 = 238;

	struct action_entry
	{
		const char* label;
	};

	static const action_entry actions[action_count];

	int get_hovered_row(short sX, short sY, short mouse_x, short mouse_y, int count, int scroll = 0) const;

	// Page system: 0 = main menu, 1 = level picker, 2 = teleport
	int m_page = 0;
	int m_selected_level = 1;

	// Teleport page state
	int m_map_count = 0;
	hb::net::TesterMapEntry m_maps[100]{};
	int m_map_scroll = 0;
	static constexpr int visible_map_rows = 10;

	void draw_main_menu(short sX, short sY, short size_x);
	void draw_level_picker(short sX, short sY, short size_x);
	void draw_teleport_page(short sX, short sY, short size_x);
	bool on_click_main_menu(short sX, short sY);
	bool on_click_level_picker(short sX, short sY);
	bool on_click_teleport(short sX, short sY);
};
#endif // TESTER_ONLY
