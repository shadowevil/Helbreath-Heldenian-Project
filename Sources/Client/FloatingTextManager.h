#pragma once

#include <array>
#include <memory>
#include <string_view>
#include <cstdint>
#include "FloatingText.h"
#include "GameConstants.h"

class CMapData;
namespace hb::shared::render { class IRenderer; }

class floating_text_manager {
public:
	// Add messages - find slot, bind to tile, return index or 0
	int add_chat_text(std::string_view msg, uint32_t time, int object_id,
	                CMapData* map_data, short sX, short sY);
	int add_damage_text(damage_text_type eType, std::string_view msg, uint32_t time,
	                  int object_id, CMapData* map_data);
	int add_notify_text(notify_text_type eType, std::string_view msg, uint32_t time,
	                  int object_id, CMapData* map_data);

	// Damage factory (replaces CreateDamageMsg logic)
	int add_damage_from_value(int damage, bool last_hit, uint32_t time,
	                       int object_id, CMapData* map_data);

	void remove_by_object_id(int object_id);
	void release_expired(uint32_t time);
	void clear(int index);
	void clear_all();

	// Rendering
	void draw_all(short min_x, short min_y, short max_x, short max_y,
	             uint32_t cur_time, hb::shared::render::IRenderer* renderer);
	void draw_single(int index, short sX, short sY,
	                uint32_t cur_time, hb::shared::render::IRenderer* renderer);

	// Position updates from entity renderers
	floating_text* get(int index);
	void update_position(int index, short sX, short sY);

	// Check if slot is occupied and matches objectID
	bool is_valid(int index, int object_id) const;
	bool is_occupied(int index) const;

private:
	static constexpr int max_messages = game_limits::max_chat_msgs;

	int find_free_slot() const;
	int bind_to_tile(int index, int object_id, CMapData* map_data, short sX, short sY);
	void draw_message(const floating_text& msg, short sX, short sY,
	                 uint32_t cur_time, hb::shared::render::IRenderer* renderer);

	std::array<std::unique_ptr<floating_text>, max_messages> m_messages;
};
