#pragma once

#include "dialog_box_ids.h"
#include "dialog_box.h"
#include "claude_testing.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class CGame;
class CPlayer;

namespace z_layer
{
	enum type { underlay, normal, topmost };
}

// Shared state for GiveItem (has no dialog class — bare shared state)
struct give_item_state
{
	int item_index{};     // was m_v1
	int action_type{};    // was m_v3 (20=npc, 24=player)
	int object_id{};      // was m_v4
	int target_x{};       // was m_v5
	int target_y{};       // was m_v6
};

class DialogBoxManager
{
public:
	DialogBoxManager(CGame& game, CPlayer& player);
	~DialogBoxManager() = default;

	void initialize_dialog_boxes();

	CPlayer& get_player() { return m_player; }

	// --- Registration ---
	void register_dialog_box(std::unique_ptr<IDialogBox> dialog_box);

	// --- Access ---
	IDialogBox* get_dialog_box(DialogBoxId::Type id) const;
	IDialogBox* get_dialog_box(int box_id) const;

	template<typename T>
	T* get_dialog_as(DialogBoxId::Type id) const
	{
		return static_cast<T*>(get_dialog_box(id));
	}

	// --- Enable/Disable ---
	void enable_dialog_box(int box_id, int type, int64_t v1, int v2, const char* string = nullptr);
	void enable_dialog_box(DialogBoxId::Type id, int type, int64_t v1, int v2, const char* string = nullptr);
	void disable_dialog_box(int box_id);
	void disable_dialog_box(DialogBoxId::Type id);
	void toggle_dialog_box(DialogBoxId::Type id, int type = 0, int64_t v1 = 0, int v2 = 0, const char* string = nullptr);

	// Short aliases
	void enable(DialogBoxId::Type id, int type = 0, int64_t v1 = 0, int v2 = 0, const char* string = nullptr);
	void enable(int id, int type = 0, int64_t v1 = 0, int v2 = 0, const char* string = nullptr);
	void disable(DialogBoxId::Type id);
	void disable(int id);
	void toggle(DialogBoxId::Type id, int type = 0, int64_t v1 = 0, int v2 = 0, const char* string = nullptr);

	// --- Lifecycle methods ---
	void reset_all_for_map_change();
	void disable_npc_dialogs();
	void disable_crusade_dialogs();
	void disable_crafting_dialogs();

	bool is_enabled(DialogBoxId::Type id) const;
	bool is_enabled(int box_id) const;
	bool is_blocking_operation_active() const;
	void set_enabled(DialogBoxId::Type id, bool enabled);
	void set_enabled(int box_id, bool enabled);

	// --- Z-Order ---
	void set_z_layer(DialogBoxId::Type id, z_layer::type layer);
	int get_top_id() const;  // topmost normal-layer dialog
#ifdef DEF_CLAUDE_TESTING
	// Agent bridge, read-only: every enabled dialog in click order (topmost layer first, then normal, then underlay)
	std::vector<const IDialogBox*> agent_dialogs_top_to_bottom() const;
	// The stable snake_case agent name of a dialog ("inventory"), or "dialog<N>" for an id with no entry
	static std::string agent_dialog_name(DialogBoxId::Type id);
#endif

	// --- Rendering + Input (no mouse params — reads hb::shared::input) ---
	void draw_all();
	void update_all();
	bool handle_click();
	bool handle_double_click();
	int handle_mouse_down();
	bool handle_right_click(uint32_t time);
	bool handle_dragging_item_release();
	PressResult handle_press(int dlg_id);
	bool handle_item_drop(int dlg_id);

	// --- Shared state (no dialog class) ---
	give_item_state m_give_item;

private:
	CGame& m_game;
	CPlayer& m_player;

	// Dynamic dialog storage — no fixed arrays
	std::unordered_map<int, std::unique_ptr<IDialogBox>> m_dialogs;

	// Z-order (three layers: underlay drawn first, normal in middle, topmost drawn last)
	std::vector<uint8_t> m_underlay_order;
	std::vector<uint8_t> m_normal_order;
	std::vector<uint8_t> m_topmost_order;

	// Z-layer assignment per dialog
	std::unordered_map<int, z_layer::type> m_z_layers;

	uint32_t m_close_debounce_time = 0;

	// Internal helpers
	void bring_to_front(int id);
	void remove_from_order(int id);
	z_layer::type get_z_layer(int id) const;
	void clamp_position(IDialogBox* dlg);

	// Iterate dialogs from topmost to bottom; fn(IDialogBox*) returns true to stop
	template<typename Func>
	bool for_each_top_to_bottom(Func&& fn) const;
};
