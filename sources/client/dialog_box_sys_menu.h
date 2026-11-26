#pragma once
#include "dialog_box.h"
#include "control_collection.h"
#include "display_settings.h"
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace cc
{
	class button;
	class check_box;
	class dropdown;
	class label;
}

// The system menu (mockup b2, HH-0119): a tab column (General, Graphics, Audio, Gameplay, then Log Out and
// Restart), a settings pane of labelled dropdowns, check boxes and sliders, and a footer. Every control is a
// CControls control; the frame panel follows the dialog. The display rows are staged (HH-0118): Apply applies
// them and a popup keeps them or reverts after 15 s; VSync and the window border apply at once.
class DialogBox_SysMenu : public IDialogBox
{
public:
	DialogBox_SysMenu(CGame* game);
	~DialogBox_SysMenu() override = default;

	void on_update() override;
	void on_draw() override;
	bool on_click() override;
	PressResult on_press() override;
	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
	bool on_disable() override;
	// Leaving the game or a map reset is not a Keep: an applied, unconfirmed display change goes back
	void discard_unconfirmed_display() { revert_display(); }

private:
	struct tab
	{
		enum type
		{
			general,
			graphics,
			audio,
			gameplay,
			count
		};
	};

	// One page of the settings pane: its rows scroll in whole rows; a row partly outside the pane is hidden
	struct pane_page
	{
		cc::panel* panel = nullptr;
		int content_bottom = 0;   // dialog-relative bottom of the last row
		int scroll = 0;           // px, a multiple of the row height
	};
	static int max_scroll_of(const pane_page& p);   // px, whole rows; 0 when the page fits

	// A volume row: its on/off box and slider, read from and written to the audio manager
	struct volume_row
	{
		cc::check_box* enabled = nullptr;
		cc::button* slider = nullptr;
		std::function<int()> get;
		std::function<void(int)> set;
		std::function<void()> save;
	};

	// ---- Building (once; values are synced every frame, so nothing is rebuilt from a callback)
	void build_controls();
	void build_general(cc::panel* page);
	void build_graphics(cc::panel* page);
	void build_audio(cc::panel* page);
	void build_gameplay(cc::panel* page);
	void build_popup();

	// Row builders on a page at m_row_y (dialog-relative cap middle); each advances m_row_y
	void add_heading(cc::panel* page, const char* text);
	cc::label* add_label(cc::panel* page, const char* text);
	cc::check_box* add_check(cc::panel* page, const char* text, std::function<void(bool)> on_change);
	cc::dropdown* add_dropdown(cc::panel* page, const char* text, std::function<void(int)> on_change);
	void add_info(cc::panel* page, const char* text, std::function<std::string()> value);
	void add_volume(cc::panel* page, const char* text, volume_row row);

	// ---- Per frame
	void update_controls();
	void sync_controls();         // values, enabled and visible states from the settings
	void layout_page();           // the active page's scroll: rows in or out of the pane
	void handle_wheel(int notches);
	void handle_drags();
	bool any_list_open() const;
	// The mouse for the controls: away unless this dialog is the one under it (or a press began here). Mouse
	// and wheel only: the keys belong to the game (chat, hotkeys) while the menu is open
	cc::input_state controls_input();

	// ---- Display staging (HH-0118)
	void rebuild_display_options();   // the mode, resolution and refresh lists the window offers
	bool is_display_pending() const;  // a staged row differs from what is in effect
	void apply_pending();             // Apply: the staged rows over the current settings
	// Apply through the window with a keep-or-revert popup; nothing is saved until Keep
	void apply_staged(const hb::shared::render::display_settings& request);
	void keep_display();
	void revert_display();

	struct display_confirm
	{
		bool active = false;
		hb::shared::render::display_settings before;      // what Revert re-applies
		hb::shared::render::display_settings requested;   // what Keep saves
		uint32_t deadline_ms = 0;                         // Revert by itself at this time
	};
	display_confirm m_confirm;
	// Window mode, size, refresh rate and aspect as picked; the other fields are unused
	hb::shared::render::display_settings m_staged;
	std::vector<hb::shared::render::window_mode::type> m_modes;   // the window mode dropdown's rows
	std::vector<hb::shared::render::resolution_option> m_resolutions;
	std::vector<hb::shared::render::refresh_option> m_refresh_rates;

	// ---- Controls
	cc::control_collection m_controls;
	cc::panel* m_frame = nullptr;     // at the dialog's position
	cc::panel* m_main = nullptr;      // everything but the popup (disabled while it asks)
	cc::panel* m_popup = nullptr;
	cc::panel* m_pane_area = nullptr; // the settings pane, for the wheel
	cc::panel* m_tab_area = nullptr;  // the tab column, for the wheel
	cc::button* m_tabs[tab::count] = {};
	cc::button* m_logout = nullptr;
	cc::button* m_restart = nullptr;
	cc::button* m_scrollbar = nullptr;
	cc::button* m_apply = nullptr;
	cc::button* m_revert = nullptr;
	pane_page m_pages[tab::count];
	std::vector<std::pair<cc::label*, cc::control*>> m_label_links;   // a row's label greys with its control
	std::vector<volume_row> m_volumes;

	cc::dropdown* m_window_mode = nullptr;
	cc::dropdown* m_resolution = nullptr;
	cc::dropdown* m_refresh = nullptr;
	cc::check_box* m_vsync = nullptr;
	cc::dropdown* m_aspect = nullptr;
	cc::check_box* m_window_border = nullptr;
	cc::dropdown* m_detail = nullptr;
	// Check boxes mirroring a setting: the getter re-reads it every frame
	std::vector<std::pair<cc::check_box*, std::function<bool()>>> m_checks;

	int m_active_tab = tab::general;
	int m_row_y = 0;
	int m_next_id = 100;
	float m_alpha = 1.0f;
	cc::input_state m_input;   // this frame's controls input
	int m_wheel_total = 0;   // the wheel as a level, for the collection
	int m_wheel_delta = 0;   // this frame's share of it, for the menu's own wheel
	bool m_press_ours = false;
	bool m_prev_left = false;
	int m_dragged_volume = -1;   // the volume slider being dragged, saved on release
};
