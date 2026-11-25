// hud_kit.h: The HUD action bar drawn from the staged HUD kit (presentation only)
//
// Kit files (tools/panel_kit/build_hud_kit.py) under <sprite_dir>/hudkit/: hud_kit.png (the HD atlas, cut
// from the 2.4x HUD sheet) and hud_kit.json (frames by name with their atlas rect and game-pixel size, the
// bar-relative layout and the gauge offsets). Spec: .claude/design/plans/hud-kit-runtime.md.
//
// All positions, sizes and gauge math are in game pixels: the bar covers the same 800x53 at every screen
// size, and each HD frame is drawn scaled into its game-pixel rect at every scale, 1x included (HH-0100).
// Every draw takes the bar's top (the HUD dialog's y); x is bar-relative, the bar itself at x 0.
// Gauge rule (user, 2026-11-24, "A, hard cut"): trough, then the fill clipped to value / max with a straight
// edge, then the trough's caps over the fill's ends. HP, MP, SP and EXP are anchored left and deplete to the
// left; hunger is anchored at the bottom. Hit-testing, tooltips and values stay with DialogBox_HudPanel.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace hb::shared::render
{
	class IRenderer;
}

namespace hb::client
{
	namespace hud_tube
	{
		enum type : uint8_t
		{
			hp,
			mp
		};
	}

	namespace hud_combat
	{
		enum type : uint8_t
		{
			peace,
			attack,
			safe_attack
		};
	}

	class hud_kit
	{
	public:
		hud_kit();
		~hud_kit();

		// Loads <sprite_dir>/hudkit. False (and empty) on any missing or bad file; the HUD then keeps drawing
		// the old sprites. The renderer must outlive the kit.
		bool load(hb::shared::render::IRenderer& renderer, const std::string& sprite_dir);
		bool is_loaded() const { return m_data != nullptr; }

		void bar(int top) const;
		void message_frame(int top) const;
		// value is clamped to 0..max; max <= 0 draws an empty gauge
		void exp(int top, uint64_t value, uint64_t max) const;
		void tube(hud_tube::type which, int top, int value, int max) const;
		void stamina(int top, int value, int max) const;
		void hunger(int top, int value, int max) const;
		// Socket, the orb for the mode (peace has its own hover orb) and the hover ring
		void combat(int top, hud_combat::type mode, bool hover) const;
		// The super-attack orb over the combat orb, additive at 0.7 (drawn while ALT is held)
		void super_orb(int top) const;
		void crusade(int top, bool aresden, bool hover) const;
		// The six menu buttons in layout order; hovered = the index drawn in its hover state, -1 = none
		void menu(int top, int hovered) const;

	private:
		struct kit_data;   // the atlas, the parts and the layout (hud_kit.cpp)

		std::unique_ptr<kit_data> m_data;   // null until a load succeeds
	};
}
