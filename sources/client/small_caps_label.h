// small_caps_label.h: A label in simulated small caps, baked once into textures (HH-0089)
//
// The login screen's labels are gold small caps with a black drop shadow and, on Connect and Cancel, a white
// glow when hovered. The face has no small-cap glyphs, so each word is a full-size first letter plus the rest
// at a smaller size, all upper case, on one baseline. Pieces are baked with ITextRenderer::bake_run (UTF-8),
// the glow with bake_glow, at the window's resolution; drawing places them in UI units.
//////////////////////////////////////////////////////////////////////

#pragma once

#include "primitive_types.h"
#include "texture.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace hb::shared::render
{
	class IRenderer;
}

namespace hb::client
{
	class small_caps_label
	{
	public:
		// Sizes are window pixels. face is an ITextRenderer::load_face id; spacing_px is the extra space after
		// every letter; the glow is a dilate of glow_dilate px and a blur of glow_blur px (0 dilate = no glow).
		struct style
		{
			int face = 0;
			float big_px = 24.0f;
			float small_ratio = 0.82f;
			float spacing_px = 0.0f;
			int glow_dilate = 0;
			float glow_blur = 0.0f;
			float glow_intensity = 1.0f;   // the additive halo's strength, 0..1
		};

		enum class anchor
		{
			left,
			centre,
			right
		};

		// ui_scale: window pixels per UI unit at this moment, kept so the textures draw at their baked size
		// until the next bake
		void bake(const std::string& text, const style& style, float ui_scale);
		void release();
		bool empty() const { return m_pieces.empty(); }

		// The baseline at (x, y) in UI units, the text anchored at x. shadow is the black copy's offset in UI
		// units (right and down); glow draws the white halo behind. Positions snap to whole window pixels.
		void draw(hb::shared::render::IRenderer& renderer, float x, float y, anchor where,
			const hb::shared::render::Color& color, float alpha, float shadow, bool glow) const;

	private:
		struct piece
		{
			std::unique_ptr<hb::shared::render::ITexture> text;
			std::unique_ptr<hb::shared::render::ITexture> halo;
			float x = 0.0f;        // window px from the label's left edge to the text texture's left
			float y = 0.0f;        // window px from the baseline to the text texture's top
			float halo_x = 0.0f;   // the same for the glow texture
			float halo_y = 0.0f;
		};

		std::vector<piece> m_pieces;
		float m_width = 0.0f;   // window px
		float m_ui_scale = 1.0f;
		float m_glow_intensity = 1.0f;
	};
}
