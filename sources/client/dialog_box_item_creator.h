// TESTER MENU — entire file is tester-only
#pragma once
#ifdef TESTER_ONLY
#include "dialog_box.h"
#include "packet/packet_notify.h"
#include "item/item_attributes.h"
#include <string>
#include <vector>

class DialogBox_ItemCreator : public IDialogBox
{
public:
	DialogBox_ItemCreator(CGame* game);
	~DialogBox_ItemCreator() override = default;

	void on_draw() override;
	bool on_click() override;
	bool on_disable() override;

	void receive_search_results(const hb::net::PacketNotifyTesterItemSearchResult* pkt);
	void on_enter_pressed();

	bool on_enable(int type, int64_t v1, int v2, const char* string) override;
private:
	enum class item_category { none, weapon, armor, magic_weapon };

	int m_page = 0;

	// Search state
	std::string m_search_text;
	std::string m_last_sent_search;
	int m_result_count = 0;
	hb::net::TesterItemSearchEntry m_results[50]{};
	int m_selected_index = -1;
	int m_scroll_offset = 0;
	bool m_initial_load = false;

	// Track dialog position to fix cursor drift
	short m_last_sx = 0;
	short m_last_sy = 0;

	// Attribute configuration
	int m_prefix_index = 0;
	int m_prefix_value = 1;
	int m_secondary_index = 0;
	int m_secondary_value = 1;
	static constexpr int max_value = 13;
	int m_enchant_value = 0;    // 0-15 (bits 28-31 of attribute)
	int m_item_count = 1;       // 1-10

	item_category m_category = item_category::none;
	struct attr_option
	{
		int type;
		const char* name;
		int multiplier;
	};
	std::vector<attr_option> m_valid_prefixes;
	std::vector<attr_option> m_valid_secondaries;

	// Dropdown state
	enum class dropdown_id : int
	{
		none = -1,
		prefix_type, prefix_value,
		effect_type, effect_value,
		upgrade, count
	};
	dropdown_id m_open_dropdown = dropdown_id::none;
	int m_dropdown_scroll = 0;
	static constexpr int dropdown_h = 14;
	static constexpr int dropdown_max_vis = 8;

	void build_valid_options(int16_t effect_type);
	static item_category classify_item(int16_t effect_type);
	static const char* category_name(item_category cat);
	std::string build_preview_string() const;

	void draw_search_page(short sX, short sY, short size_x, short mouse_x, short mouse_y, short z);
	void draw_configure_page(short sX, short sY, short size_x, short mouse_x, short mouse_y, short z);
	bool on_click_search(short sX, short sY, short size_x);
	bool on_click_configure(short sX, short sY, short size_x);

	// UI helpers
	void draw_dropdown_field(int x, int y, int w, const char* text, bool is_open, bool is_hover);
	int get_open_dropdown_count() const;
};
#endif // TESTER_ONLY
