#pragma once
#include "dialog_box.h"
#include "item/item_enums.h"
#include <string>

namespace hb::client
{
	class kit_dialog;
}

struct EquipSlotLayout
{
	hb::shared::item::EquipPos equipPos;
	int offsetX;
	int offsetY;
};

class DialogBox_Character : public IDialogBox
{
public:
	DialogBox_Character(CGame* game);
	~DialogBox_Character() override = default;

	void on_draw() override;
	bool on_click() override;
	bool on_double_click() override;
	PressResult on_press() override;
	bool on_item_drop() override;

	bool cancels_text_input_on_enable() const override { return false; }
private:
	// Helper methods
	// A value centred both ways in its dialog-relative box, the dialog at (x, y)
	void draw_value(int x, int y, const hb::client::kit_rect& box, const std::string& text,
		const hb::shared::render::Color& color);
	void draw_equipped_item(hb::shared::item::EquipPos equipPos, int drawX, int drawY,
		const char* equip_poi_status, bool highlight, int spriteOffset = 0);
	// A kit text button at dialog-relative (btnX, btnY) in the dialog's kit frame, hovered when the mouse is on it
	void draw_hover_button(const hb::client::kit_dialog& kit, int sX, int sY, int btnX, int btnY,
		short mouse_x, short mouse_y, const char* label);
	void draw_male_character(short sX, short sY, short mouse_x, short mouse_y,
		const char* equip_poi_status, char& collison);
	void draw_female_character(short sX, short sY, short mouse_x, short mouse_y,
		const char* equip_poi_status, char& collison);

	// Shared helpers
	void build_equip_status_array(char (&equip_poi_status)[hb::shared::item::DEF_MAXITEMEQUIPPOS]) const;
	char find_equip_item_at_point(short mouse_x, short mouse_y, short sX, short sY,
		const char* equip_poi_status) const;

	static constexpr ui_rect btn_quest{15, 340, 75, 21};
	static constexpr ui_rect btn_party{98, 340, 75, 21};
	static constexpr ui_rect btn_levelup{180, 340, 75, 21};
};
