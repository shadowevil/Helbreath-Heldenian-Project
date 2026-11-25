// creature_icons.h: Creature icons from the map kit (mapkit/creatures.png + creatures.json)
//
// Each creature is one rect of the sheet, keyed here by the npc ids it lists (the id the server
// sends in a spawn event). Used by the guide map's monster-event marker.
//////////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include <memory>
#include <string>

namespace hb::shared::render
{
	class IRenderer;
	class ITexture;
}

namespace hb::client
{
	class creature_icons
	{
	public:
		creature_icons();
		~creature_icons();

		// Loads <sprite_dir>/mapkit/creatures.{json,png}; false (and empty) on any missing or bad file
		bool load(hb::shared::render::IRenderer& renderer, const std::string& sprite_dir);
		void unload();
		bool is_loaded() const { return m_sheet != nullptr; }

		// Draws the icon for npc_id fitted into a box_size square centred on (centre_x, centre_y),
		// never enlarged. Draws nothing for an unknown id.
		void draw(hb::shared::render::IRenderer& renderer, int npc_id, float centre_x, float centre_y, float box_size) const;

	private:
		struct icon
		{
			float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;   // sheet pixels
		};

		std::unique_ptr<hb::shared::render::ITexture> m_sheet;
		std::map<int, icon> m_icons;   // npc id -> rect
	};
}
