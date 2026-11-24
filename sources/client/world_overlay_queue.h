// world_overlay_queue.h: world-anchored overlays, queued in the world pass and drawn in screen space
//
// Names and floating text (chat bubbles, damage numbers) are produced while the world draws, but
// belong to screen space so their text rasterises at window resolution. They are queued with an
// anchor in world-target pixels and replayed after end_world(), in queue order, as the world_overlay
// layer (above the world, below chat input and dialogs).
//////////////////////////////////////////////////////////////////////

#pragma once

#include "text_lib.h"
#include "floating_text_manager.h"
#include "global_def.h"  // before renderer.h: it owns the LOGICAL_* functions render_constants.h would redefine
#include "renderer.h"
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace hb::client {

class world_overlay_queue
{
public:
	struct point
	{
		int x = 0;
		int y = 0;
	};

	// World-target pixels -> UI units through the world rect and the UI rect. Every replayed anchor
	// goes through here. Identity when the rects coincide (letterbox, or a window with the logical aspect).
	static point world_to_ui(point world, const hb::shared::render::IRenderer* renderer)
	{
		if (!renderer)
			return world;
		hb::shared::geometry::GamePoint ui = renderer->world_to_ui({world.x, world.y});
		return {ui.x, ui.y};
	}

	// A text line anchored at world-target pixels, shifted by offset in UI units
	void add_text(int font_id, point anchor, point offset, std::string text, const hb::shared::text::TextStyle& style)
	{
		m_items.emplace_back(text_item{anchor, offset, font_id, std::move(text), style});
	}

	// A floating_text_manager message (chat bubble, damage number) by slot, anchored at world-target pixels
	void add_floating_text(int message_index, point anchor)
	{
		m_items.emplace_back(floating_text_item{anchor, message_index});
	}

	// Draw every queued item in queue order (screen space), then empty the queue
	void flush(floating_text_manager& floating_text, uint32_t cur_time, hb::shared::render::IRenderer* renderer)
	{
		for (const auto& item : m_items)
		{
			std::visit([&](const auto& entry)
			{
				point at = world_to_ui(entry.anchor, renderer);
				if constexpr (std::is_same_v<std::decay_t<decltype(entry)>, text_item>)
					hb::shared::text::draw_text(entry.font_id, at.x + entry.offset.x, at.y + entry.offset.y, entry.text.c_str(), entry.style);
				else
					floating_text.draw_single(entry.message_index, static_cast<short>(at.x), static_cast<short>(at.y), cur_time, renderer);
			}, item);
		}
		m_items.clear();
	}

private:
	struct text_item
	{
		point anchor;
		point offset;
		int font_id = 0;
		std::string text;
		hb::shared::text::TextStyle style;
	};

	struct floating_text_item
	{
		point anchor;
		int message_index = 0;
	};

	std::vector<std::variant<text_item, floating_text_item>> m_items;
};

} // namespace hb::client
