#pragma once
#include "dialog_box.h"

namespace hb::client
{
	class kit_dialog;
}

class DialogBox_SysMenu : public IDialogBox
{
public:
	// Tab indices
	enum Tab
	{
		TAB_GENERAL = 0,
		TAB_GRAPHICS,
		TAB_AUDIO,
		TAB_SYSTEM,
		TAB_COUNT
	};

	DialogBox_SysMenu(CGame* game);
	~DialogBox_SysMenu() override = default;

	void on_update() override;
	void on_draw() override;
	bool on_click() override;
	PressResult on_press() override;

	// Resolution: the next size the window offers (IWindow::get_resolution_options)
	static void cycle_resolution();

private:
	// Tab drawing
	void draw_tabs(short sX, short sY);
	void draw_tab_content(short sX, short sY, short mouse_x, short mouse_y, char lb);

	// Individual tab content
	void draw_general_tab(short sX, short sY);
	void draw_graphics_tab(short sX, short sY);
	void draw_audio_tab(short sX, short sY, short mouse_x, short mouse_y, char lb);
	void draw_system_tab(short sX, short sY);

	// Click handlers for each tab
	bool on_click_general(short sX, short sY);
	bool on_click_graphics(short sX, short sY);
	bool on_click_audio(short sX, short sY);
	bool on_click_system(short sX, short sY);

	// Helper to draw On/Off toggle
	void draw_toggle(int x, int y, bool enabled);
	bool is_in_toggle_area(int x, int y);

	// Panel kit parts at the old sprite parts' positions and sizes (screen coordinates); only while on_draw runs
	void draw_box(int frame, int x, int y);              // value box (button_frame 78 / 79 / 81)
	void draw_slider(int x, int y, int value);           // track (button_frame 80) + knob (kit btn_box_b)
	void draw_graphics_scrollbar(int max_scroll);

	// The kit frame while on_draw runs (it lives on on_draw's stack), else nullptr
	const hb::client::kit_dialog* m_kit = nullptr;

	int m_iActiveTab;
	int m_graphics_scroll_offset = 0;

	// Cached frame dimensions (initialized on first update)
	bool m_bFrameSizesInitialized;
	int m_iWideBoxWidth;    // Frame 78 width
	int m_iWideBoxHeight;   // Frame 78 height
	int m_iSmallBoxWidth;   // Frame 79 width
	int m_iSmallBoxHeight;  // Frame 79 height
	int m_iLargeBoxWidth;   // Frame 81 width
	int m_iLargeBoxHeight;  // Frame 81 height
};
