#pragma once
#include "dialog_box.h"
#include "parchment_kit.h"
#include <string>

class DialogBox_Map : public IDialogBox
{
public:
	// v2 passed to on_enable when the server has no frame for the map; on_enable ignores it and
	// refuses (the dialog stays closed) when the kit has no map or fails to load
	static constexpr int no_legacy_frame = -1;

	DialogBox_Map(CGame* game);
	~DialogBox_Map() override = default;

	void on_draw() override;
	bool on_click() override;

	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
	bool on_disable() override;

	static bool kit_has_map(const std::string& map_name);

private:
	bool load_kit_for_current_map();
	void draw_marker(short sX, short sY, int tX, int tY);

	hb::client::parchment_kit m_kit;
	std::string m_opened_map;   // the map the dialog was opened on (lowercased by the kit when it has one)
};
