// copyright_notice.h: The two copyright lines at the bottom of the front-end screens (HH-0088)
//
// The lines carry the copyright sign, so they are baked once into textures (ITextRenderer::bake_run reads
// UTF-8; draw_text reads its const char* as ANSI) and drawn as sprites. CGame owns the one instance
// (get_copyright_notice) and releases it before the renderer goes away; the front-end screens reach it
// through IGameScreen::draw_copyright().
//////////////////////////////////////////////////////////////////////

#pragma once

#include "bake_debounce.h"
#include "texture.h"

#include <array>
#include <memory>

namespace hb::shared::render
{
	class IRenderer;
}

namespace hb::client
{
	class copyright_notice
	{
	public:
		// Draws the lines bottom-centre of the renderer's visible area, baking them first, and again when
		// the window's UI scale changes. show_company false leaves out the company line.
		void draw(hb::shared::render::IRenderer& renderer, bool show_company);
		void release();

	private:
		void bake(float scale);

		std::array<std::unique_ptr<hb::shared::render::ITexture>, 2> m_lines;   // company, recreation
		float m_scale = 0.0f;   // the UI scale they were baked for; 0 = not yet
		bake_debounce m_debounce;
	};
}
