#pragma once

#include <array>
#include <memory>
#include "game_constants.h"

class CItem;
class CGame;

class shop_manager
{
public:
	static shop_manager& get();
	void set_game(CGame* game);

	void request_shop_menu(int16_t npc_config_id);
	void handle_response(char* data);

	void clear_items();
	bool has_items() const;

	auto& get_item_list() { return m_item_list; }
	int16_t get_pending_npc_config_id() const { return m_pending_npc_config_id; }
	void set_pending_npc_config_id(int16_t npc_config_id) { m_pending_npc_config_id = npc_config_id; }

private:
	shop_manager();
	~shop_manager();

	void send_request(int16_t npc_config_id);

	CGame* m_game = nullptr;
	std::array<std::unique_ptr<CItem>, game_limits::max_menu_items> m_item_list;
	int16_t m_pending_npc_config_id = 0;
};
