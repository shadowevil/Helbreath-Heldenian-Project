#pragma once
#include "IDialogBox.h"
#include "Item/ItemEnums.h"

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
	void draw_stat(int x1, int x2, int y, int baseStat, int angelicBonus);
	void draw_equipped_item(hb::shared::item::EquipPos equipPos, int drawX, int drawY,
		const char* equip_poi_status, bool highlight, int spriteOffset = 0);
	void draw_hover_button(int sX, int sY, int btnX, int btnY,
		short mouse_x, short mouse_y, int hoverFrame, int normalFrame);
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
