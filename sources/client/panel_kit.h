// panel_kit.h: Dialog panels and text buttons composed at runtime from the staged panel kit
//
// Kit files (tools/panel_kit/build_panel_kit.py) under <sprite_dir>/panelkit/: panel_kit.png (the atlas)
// and panel_kit.json (frames by name, backgrounds, border and corner styles, inner pieces, flourishes,
// buttons and text recipes). tools/panel_kit/compose.py is the reference composer: this code draws what
// it draws, with the same build order and integer layout.
//
// Classes (API: .claude/design/plans/panel-kit-runtime.md, "Client API"):
//   panel_kit    the kit: loaded once (CGame owns it), draws pieces, panels, buttons and text recipes
//   kit_button   a button: a predefined layout (button_layout::small / wide) or a custom button_style,
//                plus its text and position; draws itself and plugs into cc:: as a render handler
//   kit_dialog   a dialog frame of a predefined (dialog_size) or custom size at a position; the dialog
//                implementation adds the components (title, heading, field, inset, rule, buttons ...)
//                at positions it chooses, relative to the frame. The kit owns no dialog's layout.
// Hit-testing and hover stay with the caller (cc:: controls or the dialog's existing input code).
//
// Staging: steps 1-3 (HH-0060..0062) use only the buttons. kit_dialog, compose, the inner pieces
// (inset, raised, rules, slot rings, scrollbar, flourishes) and the title / heading / field recipes are
// deliberately in place for the dialog conversions of steps 4-7 (.claude/design/plans/panel-kit-runtime.md).
//////////////////////////////////////////////////////////////////////

#pragma once

#include "primitive_types.h"
#include "types.h"   // cc::render_handler

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace hb::shared::render
{
	class IRenderer;
	class ITexture;
}

namespace cc
{
	class button;
	class control;
}

namespace hb::client
{
	struct kit_point
	{
		int x = 0;
		int y = 0;
	};

	struct kit_rect
	{
		int x = 0;
		int y = 0;
		int w = 0;
		int h = 0;
	};

	// Panel build parts by name from panel_kit.json ("" = leave that part out)
	struct panel_style
	{
		std::string background = "stone_cloth";
		std::string border = "wood_light";
		std::string corner = "stone";
	};

	namespace text_anchor
	{
		enum type : uint8_t
		{
			cap,     // x = centre of the ink, y = centre of the cap height
			lcap,    // x = left edge of the ink
			rcap     // x = right edge of the ink
		};
	}

	// Predefined button layouts, read from the JSON "buttons" block
	namespace button_layout
	{
		enum type : uint8_t
		{
			small,   // 74x20, Tahoma label
			wide     // 176x24, Liberation Serif label laid out per word
		};
	}

	// A custom button from kit pieces. The frame is the 8 pieces <frame>_tl, _t, _tr, _l, _r, _bl, _b, _br
	// (corners 4x4, edges tiled between them). w or h 0 = from the text: ink width + 2 * auto_pad.
	struct button_style
	{
		std::string face_normal = "btn_face_normal";
		std::string face_hover = "btn_face_hover";
		std::string frame = "btn_frame";
		std::string cap_left = "btn_cap_left";     // "" = no caps
		std::string cap_right = "btn_cap_right";
		int cap_inset = 2;
		std::string recipe = "small_button_label";
		int w = 0;
		int h = 20;
		int auto_pad = 12;
	};

	// Predefined dialog frame sizes (the original dialogs' sizes); any other size is custom
	namespace dialog_size
	{
		enum type : uint8_t
		{
			character,      // 270x376
			chat_history,   // 364x162
			sys_menu        // 331x303
		};
	}

	class panel_kit
	{
	public:
		panel_kit();
		~panel_kit();

		// Loads <sprite_dir>/panelkit. False (and empty) on any missing or bad file; callers then keep
		// drawing the old sprites. The renderer must outlive the kit.
		bool load(hb::shared::render::IRenderer& renderer, const std::string& sprite_dir);
		void unload();
		bool is_loaded() const { return m_data != nullptr; }

		// The style a predefined layout stands for (JSON "buttons": sizes, faces, frame, caps, label styles)
		const button_style& style_of(button_layout::type layout) const;
		static kit_point size_of(dialog_size::type size);

		// ---- Panels: size -> background (inset 1 px) -> border -> corners; alpha reaches every draw
		void compose(int x, int y, int w, int h, const panel_style& style, float alpha = 1.0f) const;

		// ---- Components, absolute coordinates (kit_dialog offsets them by its origin)
		// field: background name ("" = no fill); darken f lays px_black at alpha 1 - f over it.
		// tile_origin: where the field's tile grid starts (a panel's (x + 1, y + 1) lines it up with the
		// cloth); none = the inset's own (x + 1, y + 1)
		void inset(const kit_rect& r, std::string_view field = "stone_field", float darken = 0.0f, float alpha = 1.0f,
			std::optional<kit_point> tile_origin = {}) const;
		void raised(const kit_rect& r, float alpha = 1.0f) const;
		void rule(int x, int y, int w, float alpha = 1.0f) const;
		void vrule(int x, int y, int h, float alpha = 1.0f) const;
		void slot_ring(int cx, int cy, float alpha = 1.0f) const;
		void scrollbar(const kit_rect& r, float pos, float alpha = 1.0f) const;   // pos 0 top .. 1 bottom
		// A flourish centred on (cx, cy) in its JSON tint and blend; scale <= 0 = the JSON scale
		void flourish(std::string_view name, int cx, int cy, bool mirror = false, float scale = 0.0f, float alpha = 1.0f) const;
		kit_point flourish_size(std::string_view name, float scale = 0.0f) const;
		void button(const button_style& style, std::string_view text, const kit_rect& r, bool hover, float alpha = 1.0f) const;

		// ---- Text recipes. fill replaces the recipe's "fill" colour (pass the recipe's own via recipe_fill)
		void text(std::string_view recipe, std::string_view text, int x, int y, text_anchor::type anchor,
			const hb::shared::render::Color& fill, float alpha = 1.0f) const;
		hb::shared::render::Color recipe_fill(std::string_view recipe, bool hover = false) const;
		// Ink width of text in a recipe (faux bold included), for auto-sized buttons and flourish gaps
		int text_width(std::string_view recipe, std::string_view text) const;
		// Gap between a field label's right edge and its value box (field_label recipe)
		int label_gap() const;

	private:
		struct kit_data;   // the atlas textures, the parsed JSON and the primitives (panel_kit.cpp)

		std::unique_ptr<kit_data> m_data;   // null until a load succeeds
	};

	class kit_button
	{
	public:
		kit_button(button_layout::type layout, std::string text, kit_point position = {});
		kit_button(button_style style, std::string text, kit_point position = {});

		void set_text(std::string text) { m_text = std::move(text); }
		void set_position(kit_point position) { m_position = position; }
		const std::string& text() const { return m_text; }

		// Size from the layout, or from the text when the style leaves it 0
		kit_rect rect(const panel_kit& kit) const;
		void draw(const panel_kit& kit, bool hover, float alpha = 1.0f) const;
		// The same at another position (a cc:: control's screen bounds, a dialog-relative point)
		void draw_at(const panel_kit& kit, kit_point position, bool hover, float alpha = 1.0f) const;

		// A cc:: render handler drawing this button at the control's screen bounds, hover when the
		// control is highlighted. The kit must outlive the handler.
		cc::render_handler render_handler(const panel_kit& kit) const;
		// A cc::button under parent (position local to it, size from rect) skinned by render_handler;
		// clicks, focus, hover and tooltips then come from the control collection
		cc::button* add_to(cc::control& parent, int id, const panel_kit& kit) const;

	private:
		const button_style& style(const panel_kit& kit) const;

		bool m_predefined = true;
		button_layout::type m_layout = button_layout::small;
		mutable button_style m_style;   // the custom style, or the layout's once resolved
		mutable bool m_resolved = false;
		std::string m_text;
		kit_point m_position;
	};

	// A dialog frame drawn when constructed; components are placed relative to its top-left
	class kit_dialog
	{
	public:
		kit_dialog(const panel_kit& kit, dialog_size::type size, kit_point position, float alpha = 1.0f,
			const panel_style& style = {});
		kit_dialog(const panel_kit& kit, kit_point size, kit_point position, float alpha = 1.0f,
			const panel_style& style = {});

		kit_point size() const { return m_size; }
		kit_point origin() const { return m_origin; }

		// Red title (title_red recipe) with the cap centred on y; flourish "" = none, one each side gap px
		// from the ink, the right one mirrored
		void title(std::string_view text, int y, const hb::shared::render::Color& fill,
			std::string_view flourish = "", int gap = 14) const;
		// Section heading (heading recipe), optional flourishes as for title
		void heading(std::string_view text, int y, std::string_view flourish = "", int gap = 6, int flourish_dy = 2) const;
		// Field label (right-aligned, field_label gap left of the box) + value box (inset) + value text
		// centred in the box in the client's default font and colour
		void field(std::string_view label, const kit_rect& box, std::string_view value,
			const hb::shared::render::Color& value_color = {25, 25, 25}) const;
		void inset(const kit_rect& r, std::string_view field = "stone_field", float darken = 0.0f) const;
		void raised(const kit_rect& r) const;
		void rule(int x, int y, int w) const;
		void vrule(int x, int y, int h) const;
		void slot_ring(int cx, int cy) const;
		void scrollbar(const kit_rect& r, float pos) const;
		void flourish(std::string_view name, int cx, int cy, bool mirror = false, float scale = 0.0f) const;
		// A text area: a darkened inset; returns the rect inside the bevel (absolute) for the caller's text
		kit_rect text_area(const kit_rect& r, float darken = 0.8f) const;
		void button(button_layout::type layout, std::string_view text, kit_point position, bool hover) const;
		void button(const kit_button& b, bool hover) const;

	private:
		kit_rect offset(const kit_rect& r) const { return {m_origin.x + r.x, m_origin.y + r.y, r.w, r.h}; }
		kit_point tile_origin() const { return {m_origin.x + 1, m_origin.y + 1}; }   // the frame's cloth grid
		void side_flourishes(std::string_view recipe, std::string_view text, int cx, int cy,
			std::string_view flourish, int gap) const;

		const panel_kit& m_kit;
		kit_point m_size;
		kit_point m_origin;
		float m_alpha;
	};
}
