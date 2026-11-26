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
#include "text_renderer.h"   // text_run
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
	class check_box;
	class control;
	class dropdown;
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
			rcap,    // x = right edge of the ink
			middle   // (x, y) = centre of the ink box (titles and headings)
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

	// A flourish look that overrides the kit's default (black, from the JSON) for one title or heading:
	// the mask's colour, additive or normal blend, and opacity. An override draws no emboss.
	struct flourish_style
	{
		hb::shared::render::Color tint;
		bool additive = false;
		float opacity = 1.0f;
	};

	// The look of the kit-drawn form controls (cc::dropdown, cc::check_box); the defaults are the system menu
	// mockup's (b2). The field is the standard inset (d-037) shaded by field_darken: these controls sit in the
	// settings pane's own inset, the nested case.
	struct form_style
	{
		std::string field = "stone_field";
		float field_darken = 0.5f;            // the kit's darken f: the field at f brightness (0 = unshaded)
		float field_darken_disabled = 0.75f;
		float list_darken = 0.45f;            // the open list's cloth
		// The open list's drop shadow: black at distance 0 on its left, right and bottom (none above), a Gaussian
		// halo of radius shadow_blur composited shadow_passes times (user, 2026-11-25: alpha 255, blur 6, twice)
		float shadow = 1.0f;
		int shadow_blur = 6;
		int shadow_passes = 2;
		int text_x = 6;                       // value text from the field's left edge
		int mark_x = 6;                       // the current option's diamond centre from the row's left edge
		int row_text_x = 13;                  // option text from the row's left edge
		hb::shared::render::Color text{239, 222, 206};
		hb::shared::render::Color text_open{247, 247, 0};
		hb::shared::render::Color text_disabled{150, 138, 120};
		hb::shared::render::Color text_current{236, 200, 120};
		hb::shared::render::Color row_hover{120, 72, 36};
		hb::shared::render::Color mark{214, 170, 60};          // the current option's diamond, the check box's tick
		hb::shared::render::Color mark_outline{90, 60, 20};
	};

	// A kit draw's opacity under the dialog transparency setting (the sprites' 0.25 blend), else opaque
	constexpr float dialog_alpha(bool transparent)
	{
		return transparent ? 0.25f : 1.0f;
	}

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
		// The pos that puts the thumb's centre at y (clamped 0..1): the inverse of scrollbar's thumb placement,
		// so a dragged thumb stays under the cursor
		float scrollbar_pos_at(const kit_rect& r, int y) const;
		// The scrollbar's parts on their own: a groove of any size (a slider track) and the thumb centred on (cx, cy)
		void groove(const kit_rect& r, float alpha = 1.0f) const;
		void thumb(int cx, int cy, float alpha = 1.0f) const;
		// A named frame (a decor piece) at its natural size, top-left at (x, y); false if the kit lacks it
		// tint multiplies the piece (a white mask takes its colour); none = as drawn in the atlas
		bool piece(std::string_view name, int x, int y, float alpha = 1.0f,
			const hb::shared::render::Color* tint = nullptr) const;
		// A flourish centred on (cx, cy) in its JSON tint and blend (or style's); scale <= 0 = the JSON scale
		void flourish(std::string_view name, int cx, int cy, bool mirror = false, float scale = 0.0f, float alpha = 1.0f,
			const flourish_style* style = nullptr) const;
		kit_point flourish_size(std::string_view name, float scale = 0.0f) const;
		void button(const button_style& style, std::string_view text, const kit_rect& r, bool hover, float alpha = 1.0f) const;

		// ---- Form controls (the cc:: skins below draw with these)
		void fill(const kit_rect& r, const hb::shared::render::Color& color, float alpha = 1.0f) const;   // solid colour
		// A diamond centred on (cx, cy): radius in the outline colour, radius - 1 filled
		void diamond(int cx, int cy, int radius, const hb::shared::render::Color& fill,
			const hb::shared::render::Color& outline, float alpha = 1.0f) const;
		// Collapsed dropdown: the shaded field, the value at text_x, a square arrow at the right (left; down when
		// open; dark when disabled)
		void dropdown_field(const kit_rect& r, const std::string& value, bool enabled, bool open,
			const form_style& style = {}, float alpha = 1.0f) const;
		// An open list's box: the soft drop shadow, the darkened cloth, the inset bevel
		void list_box(const kit_rect& r, const form_style& style = {}, float alpha = 1.0f) const;
		// One line of default-font text, left-aligned and vertically centred in r, clipped to r
		void clipped_text(const kit_rect& r, const std::string& text, const hb::shared::render::Color& color,
			float alpha = 1.0f) const;
		// A check box: the shaded field; checked = a raised square filled with the mark colour
		void check_box(const kit_rect& r, bool checked, bool enabled, const form_style& style = {}, float alpha = 1.0f) const;

		// ---- Text recipes. fill replaces the recipe's "fill" colour (pass the recipe's own via recipe_fill)
		void text(std::string_view recipe, std::string_view text, int x, int y, text_anchor::type anchor,
			const hb::shared::render::Color& fill, float alpha = 1.0f) const;
		// The same with every draw in color (a recipe with fixed colours, such as field_label, recoloured)
		void text_recolored(std::string_view recipe, std::string_view text, int x, int y, text_anchor::type anchor,
			const hb::shared::render::Color& color, float alpha = 1.0f) const;
		hb::shared::render::Color recipe_fill(std::string_view recipe, bool hover = false) const;
		// Ink width of text in a recipe (faux bold included), for auto-sized buttons and flourish gaps
		int text_width(std::string_view recipe, std::string_view text) const;
		// y of the cap-height centre (cap top to baseline) of text drawn at y with anchor: where
		// side flourishes centre so they line up with the letters
		int cap_middle(std::string_view recipe, std::string_view text, int y, text_anchor::type anchor) const;
		// A recipe's face, pixel size and tracking (px and tracking in UI units; {} when the recipe is missing),
		// for baking its text elsewhere
		hb::shared::text::text_run recipe_run(std::string_view recipe) const;
		// The ink box (absolute) that text drawn at (x, y) with anchor covers; {} for a per-word recipe or
		// before the text renderer is up
		kit_rect text_ink(std::string_view recipe, std::string_view text, int x, int y, text_anchor::type anchor) const;
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

		button_layout::type m_layout = button_layout::small;
		std::optional<button_style> m_style;   // a custom style; none = the layout's, read from the kit
		std::string m_text;
		kit_point m_position;
	};

	// cc:: skins drawn from the kit at the control's screen bounds and opacity. The kit must outlive the handlers.
	// skin_dropdown sets the field's render handler and the list's (rows, hover, current mark, scrollbar).
	void skin_dropdown(cc::dropdown& control, const panel_kit& kit, const form_style& style = {});
	void skin_check_box(cc::check_box& control, const panel_kit& kit, const form_style& style = {});

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

		// Red title (title_red recipe), its ink centred on (frame centre, y); flourish "" = none, one each side gap px
		// from the ink, the right one mirrored
		void title(std::string_view text, int y, const hb::shared::render::Color& fill,
			std::string_view flourish = "", int gap = 14, const flourish_style* style = nullptr) const;
		// Section heading (heading recipe), optional flourishes as for title. Flourishes centre on the
		// text's cap height (cap top to baseline), not its ink box
		void heading(std::string_view text, int y, std::string_view flourish = "", int gap = 6,
			const flourish_style* style = nullptr) const;
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
		float scrollbar_pos_at(const kit_rect& r, int screen_y) const;   // r dialog-relative, y on screen
		void groove(const kit_rect& r) const;
		void thumb(int cx, int cy) const;
		bool piece(std::string_view name, kit_point position) const;
		// The y of the cap-height centre of a title drawn at y (for flourishes the dialog places itself)
		int title_cap_middle(std::string_view text, int y) const;
		void flourish(std::string_view name, int cx, int cy, bool mirror = false, float scale = 0.0f,
			const flourish_style* style = nullptr) const;
		// A text area: a darkened inset; returns the rect inside the bevel (absolute) for the caller's text
		kit_rect text_area(const kit_rect& r, float darken = 0.8f) const;
		void button(button_layout::type layout, std::string_view text, kit_point position, bool hover) const;
		void button(const kit_button& b, bool hover) const;

	private:
		kit_rect offset(const kit_rect& r) const { return {m_origin.x + r.x, m_origin.y + r.y, r.w, r.h}; }
		kit_point tile_origin() const { return {m_origin.x + 1, m_origin.y + 1}; }   // the frame's cloth grid
		void side_flourishes(std::string_view recipe, std::string_view text, int cx, int cy,
			std::string_view flourish, int gap, const flourish_style* style) const;

		const panel_kit& m_kit;
		kit_point m_size;
		kit_point m_origin;
		float m_alpha;
	};
}
