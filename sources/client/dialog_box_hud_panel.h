#pragma once
#include "dialog_box.h"
#include "dialog_box_ids.h"
#include "global_def.h"

class DialogBox_HudPanel : public IDialogBox
{
public:
	DialogBox_HudPanel(CGame* game);
	~DialogBox_HudPanel() override = default;

	void on_draw() override;
	bool on_click() override;
	bool on_item_drop() override;

	// HudPanel is not draggable and doesn't cancel text input
	bool is_draggable() const override { return false; }
	bool cancels_text_input_on_enable() const override { return false; }

	// Mouse in HUD units: every HUD hit test reads this, and so does the dialog manager for this
	// dialog. The HUD draws in the world rect, not the UI rect, so this is the UI mouse mapped
	// through both rects; it equals the UI mouse when the rects coincide.
	static hb::shared::geometry::GamePoint hud_mouse();

private:
	// Y offset for resolution - shifts all Y positions when running at higher resolutions
	// At 640x480: offset = 0, At 800x600: offset = 120
	static int hud_y_offset() { return LOGICAL_HEIGHT() - 480; }

	// X offset for resolution - centers the HUD when running at wider resolutions
	// At 640x480: offset = 0, At 800x600: offset = 80 (centered)
	static int hud_x_offset() { return (LOGICAL_WIDTH() - 640) / 2; }

	static constexpr int MAX_HUNGER = 100;

	// The HP bar's top (base value for 640x480; the Poisoned label sits on it)
	static constexpr int BASE_HP_BAR_Y = 437;
	static int HP_BAR_Y() { return BASE_HP_BAR_Y + hud_y_offset(); }

	// HP/MP/SP number positions
	static constexpr int BASE_HP_NUM_X = 80;
	static constexpr int BASE_HP_NUM_Y = 441;
	static constexpr int BASE_MP_NUM_Y = 463;
	static constexpr int BASE_SP_NUM_X = 230;
	static constexpr int BASE_SP_NUM_Y = 435;

	static int HP_NUM_X() { return BASE_HP_NUM_X + hud_x_offset(); }
	static int HP_NUM_Y() { return BASE_HP_NUM_Y + hud_y_offset(); }
	static int MP_NUM_Y() { return BASE_MP_NUM_Y + hud_y_offset(); }
	static int SP_NUM_X() { return BASE_SP_NUM_X + hud_x_offset(); }
	static int SP_NUM_Y() { return BASE_SP_NUM_Y + hud_y_offset(); }

	// Map message text position
	static constexpr int BASE_MAP_MSG_X1 = 142;
	static constexpr int BASE_MAP_MSG_X2 = 325;
	static constexpr int BASE_MAP_MSG_Y = 456;
	static int MAP_MSG_X1() { return BASE_MAP_MSG_X1 + hud_x_offset(); }
	static int MAP_MSG_X2() { return BASE_MAP_MSG_X2 + hud_x_offset(); }
	static int MAP_MSG_Y() { return BASE_MAP_MSG_Y + hud_y_offset(); }

	// Button regions (Y is shared, right-side X needs offset)
	static constexpr int BASE_BTN_Y1 = 434;
	static constexpr int BASE_BTN_Y2 = 475;
	static int BTN_Y1() { return BASE_BTN_Y1 + hud_y_offset(); }
	static int BTN_Y2() { return BASE_BTN_Y2 + hud_y_offset(); }

	// Right-side buttons (need X offset for wider resolutions)
	static constexpr int BASE_BTN_CRUSADE_X1 = 324;
	static constexpr int BASE_BTN_CRUSADE_X2 = 357;
	static constexpr int BASE_BTN_COMBAT_X1 = 362;
	static constexpr int BASE_BTN_COMBAT_X2 = 404;
	static int BTN_CRUSADE_X1() { return BASE_BTN_CRUSADE_X1 + hud_x_offset(); }
	static int BTN_CRUSADE_X2() { return BASE_BTN_CRUSADE_X2 + hud_x_offset(); }
	static int BTN_COMBAT_X1() { return BASE_BTN_COMBAT_X1 + hud_x_offset(); }
	static int BTN_COMBAT_X2() { return BASE_BTN_COMBAT_X2 + hud_x_offset(); }

	// toggle button info structure
	struct ToggleButtonInfo {
		int x1, x2;
		const char* tooltip;
		DialogBoxId::Type dialogId;
	};

	static const ToggleButtonInfo TOGGLE_BUTTONS[];
	static constexpr int TOGGLE_BUTTON_COUNT = 6;

	// Level up text position (class constants, not the global functions)
	static constexpr int LOCAL_LEVELUP_TEXT_X = 32;
	static constexpr int LOCAL_LEVELUP_TEXT_Y = 448;

	// Tester menu text position (above Level Up text)
	static constexpr int BASE_TESTER_TEXT_Y = 433;

	// hud_mouse() split per axis, for the HUD's own hit tests and tooltips
	static short hud_mouse_x();
	static short hud_mouse_y();

	// Helper methods
	void draw_gauge_bars();
	void draw_icon_buttons();
	void draw_status_icons();
	void draw_super_attack_overlay();
	bool is_in_button(int x1, int x2) const;
	void toggle_dialog_with_sound(DialogBoxId::Type dialogId);
};
