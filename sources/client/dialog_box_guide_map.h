#pragma once
#include "dialog_box.h"
#include "creature_icons.h"
#include "minimap.h"
#include <string>

class DialogBox_GuideMap : public IDialogBox
{
public:
	DialogBox_GuideMap(CGame* game);
	~DialogBox_GuideMap() override = default;

	void on_update() override;
	void on_draw() override;
	bool on_click() override;
	bool on_double_click() override;
	// An empty minimap (a map without art) draws nothing, so the corner is world, not dialog
	bool is_hit_testable() const override;

	// Loads the minimap levels for a map; call on every map load
	void load_map(const std::string& map_name);
	// Steps the minimap tier (+1 zooms in, -1 out), clamped, and saves it in the config
	void step_zoom(int steps);

private:
	// A map with a valid index, art and a known size; everything else draws nothing and passes through
	bool is_active() const;
	void widget_origin(short& sX, short& sY) const;
	bool mouse_in_widget(short sX, short sY) const;
	void draw_border(short sX, short sY);
	void draw_map(short sX, short sY);
	void draw_location_tooltip(short mouse_x, short mouse_y, int tile_x, int tile_y);

	hb::client::minimap m_minimap;
	hb::client::creature_icons m_creature_icons;   // loaded on first map load
};
