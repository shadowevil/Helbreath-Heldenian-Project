#pragma once

#include "dialog_box_ids.h"
#include "common_types.h"
#include "panel_kit.h"
#include <cstdint>

// Simple rect for dialog-relative hit-testing
struct ui_rect { int x, y, w, h; };

// Plain panel-kit frame styles that replace the old Game1-4 sprite backgrounds (IDialogBox::draw_kit_background)
namespace kit_frame
{
	enum type : uint8_t
	{
		gold_box,  // small message / query boxes: cloth, light wood border, gold corners (Game1 f2, Game2 f5 / f6)
		rivet_box  // silver-ball corners (Game2 f2, Game4 f1 / f2)
	};
}

// The tall 258x339 dialogs' frames (IDialogBox::draw_tall_frame): Game2 frame 0 (stone corners) or frame 2
// (silver-ball corners), both with a sunken list panel
namespace tall_frame
{
	enum type : uint8_t
	{
		stone,
		rivet
	};
}

// A tall frame's list panel: the original frames' full width, or narrowed for a dialog with a scrollbar
namespace tall_panel
{
	enum type : uint8_t
	{
		full,
		scrollbar
	};
}

// Kit layout shared by the converted dialogs, dialog-relative (HH-0066)
namespace kit_layout
{
	constexpr hb::shared::render::Color title_fill{148, 24, 16};   // the baked sprite titles' red
	constexpr hb::client::kit_point tall_size{258, 339};
	// The tall dialogs' list panel at the original frames' width (x 16 to 241)
	constexpr hb::client::kit_rect tall_panel{16, 31, 225, 288};
	// The panel of a tall dialog with a scrollbar: 220 wide so the groove sits beside it (the originals ran the
	// panel under their track overlay)
	constexpr hb::client::kit_rect tall_scroll_panel{16, 31, 220, 288};
	// The scroll groove: the thumb's top at 30 + 274 * pos, where the sprite thumb drew (frame 7, pivot -6,-5, at
	// x 242, y 35 + pointer_loc), so the dialogs' 274 px drag maths keeps it under the cursor
	constexpr hb::client::kit_rect tall_track{236, 30, 15, 274 + 15};
}

class CGame;
class CPlayer;
class DialogBoxManager;
namespace hb { namespace net { struct packet_base; } }

// Result of on_press - determines how the click is handled
enum class PressResult
{
	Normal = 0,        // Normal click, allow dialog dragging
	ItemSelected = 1,  // Item/equipment selected, dialog handles CursorTarget
	ScrollClaimed = -1 // Scroll/slider region claimed, prevent dragging
};

class IDialogBox
{
public:
	IDialogBox(DialogBoxId::Type id, CGame* game);
	virtual ~IDialogBox() = default;

	// --- Core virtual methods (no mouse params — use hb::shared::input) ---
	virtual void on_draw() = 0;
	virtual bool on_click() = 0;

	// Optional virtual methods - override as needed
	virtual void on_update() {}  // Called once per frame for enabled dialogs
	virtual bool on_double_click() { return false; }

	// Called on mouse button down within dialog bounds
	virtual PressResult on_press() { return PressResult::Normal; }

	virtual bool on_item_drop() { return false; }  // Item dropped on dialog
	virtual bool on_enable(int type, int64_t v1, int v2, const char* string) { return true; }
	virtual bool on_disable() { return true; }

	// --- Behavioral flags (override in subclasses) ---
	virtual bool is_draggable() const { return true; }
	virtual bool cancels_text_input_on_enable() const { return true; }
	// False while the dialog draws nothing: the manager then skips it for every mouse hit-test, so
	// its area behaves as world (click, double-click, press, right-click, drop)
	virtual bool is_hit_testable() const { return true; }

	// --- Manager injection (called by DialogBoxManager::register_dialog_box) ---
	void set_manager(DialogBoxManager& mgr) { m_manager = &mgr; }

	// --- Common state ---
	DialogBoxId::Type get_id() const { return m_id; }
	bool is_enabled() const { return m_enabled; }
	void set_enabled(bool enabled) { m_enabled = enabled; }

	short m_x = 0, m_y = 0;
	short m_size_x = 0, m_size_y = 0;
	bool m_is_scroll_selected = false;
	bool m_can_close_on_right_click = true;

protected:
	// Returns true if mouse cursor is inside the rect (relative to dialog position)
	bool mouse_in(const ui_rect& r) const;

	// Helper methods - delegate to CGame
	void draw_new_dialog_box(char type, int sX, int sY, int frame, bool is_no_color_key = false, bool is_trans = false);
	// A plain panel-kit frame of w x h at (x, y); false when the kit is not loaded, and the caller then draws
	// its original sprite frame. transparent = the dialog transparency setting's 0.25 opacity
	bool draw_kit_background(kit_frame::type style, int x, int y, int w, int h, bool transparent = false);
	// The kit part names a kit_frame style stands for (for a kit_dialog built by the dialog itself)
	static const hb::client::panel_style& kit_frame_style(kit_frame::type style);
	// A tall 258x339 frame with the title the Text sprite frame title_frame carries (-1 = none; 11 Item for
	// Sale, 10 Repair Item, 21 Items In Storage, 1 Skills, 4 Quest, 3 Party Menu, 18 Cityhall Menu, 19 Guild
	// Menu, 2 Level up Setting, 5 Item Upgrade): a plain kit frame, title and sunken list panel, else the original
	// Game2 frame and Text title
	void draw_tall_frame(tall_frame::type style, int x, int y, int title_frame = -1,
		tall_panel::type panel = tall_panel::full);
	// The crusade map dialogs' frame (commander, constructor, soldier) with the Text strip title_frame carries
	// (15 Commander Menu, 16 Constructor Menu, 17 Soldier Menu): a plain gold-cornered kit frame, title and the
	// sunken status bar, else the original Crusade frame 0 and title strip. transparent = dialog transparency
	void draw_crusade_frame(int title_frame, int x, int y, bool transparent);
	// A tall dialog's scrollbar at the original 274 px thumb travel: the kit groove and thumb at pos (0..1,
	// clamped), else the original Game2 track frame (3, or 1 beside a stone frame) and thumb frame 7 at pointer_loc
	void draw_tall_scrollbar(int x, int y, float pos, int pointer_loc, int track_frame = 3);
	void put_string(int iX, int iY, const char* string, const hb::shared::render::Color& color);
	void put_aligned_string(int x1, int x2, int iY, const char* string, const hb::shared::render::Color& color = GameColors::UIBlack);
	void add_event_list(const char* txt, char color = 0, bool dup_allow = true);
	bool send_game_packet_impl(const hb::net::packet_base& pkt, size_t size, bool encrypt = true);

	template<typename PacketT>
	bool send_game_packet(const PacketT& pkt, bool encrypt = true)
	{
		return send_game_packet_impl(pkt, sizeof(PacketT), encrypt);
	}
	void set_default_rect(short sX, short sY, short size_x, short size_y);

	// Dialog management helpers
	void enable_dialog_box(DialogBoxId::Type id, int type = 0, int64_t v1 = 0, int v2 = 0, const char* string = nullptr);
	void disable_dialog_box(DialogBoxId::Type id);
	void disable_this_dialog();

	// Inter-dialog communication
	IDialogBox* get_dialog_box(DialogBoxId::Type id);
	template<typename T>
	T* get_dialog_box_as(DialogBoxId::Type id) { return static_cast<T*>(get_dialog_box(id)); }

	// Player access via manager
	CPlayer& player() const;
	DialogBoxManager& manager() const { return *m_manager; }

	// Direct access to game - use m_game->member for all game state
	CGame* m_game;
	DialogBoxId::Type m_id;

private:
	DialogBoxManager* m_manager = nullptr;
	bool m_enabled = false;
};
