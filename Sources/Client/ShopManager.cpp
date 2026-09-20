#include "ShopManager.h"
#include "Game.h"
#include "Packet/PacketShop.h"
#include "Packet/PacketHelpers.h"
#include "Log.h"

using namespace hb::shared::net;

shop_manager::shop_manager() = default;
shop_manager::~shop_manager() = default;

shop_manager& shop_manager::get()
{
	static shop_manager instance;
	return instance;
}

void shop_manager::set_game(CGame* game)
{
	m_game = game;
}

void shop_manager::clear_items()
{
	for (auto& item : m_item_list) item.reset();
}

bool shop_manager::has_items() const
{
	return m_item_list[0] != nullptr;
}

void shop_manager::request_shop_menu(int16_t npc_config_id)
{
	// Request shop contents from server using NPC config ID
	send_request(npc_config_id);
}

void shop_manager::send_request(int16_t npc_config_id)
{
	// clear existing shop items
	for (int i = 0; i < game_limits::max_menu_items; i++) {
		m_item_list[i].reset();
	}

	// Build and send shop request packet
	char data[sizeof(hb::net::PacketShopRequest)]{};

	auto* req = reinterpret_cast<hb::net::PacketShopRequest*>(data);
	req->header.msg_id = MSGID_REQUEST_SHOP_CONTENTS;
	req->header.msg_type = MsgType::Confirm;
	req->npcConfigId = npc_config_id;

	m_game->m_g_sock->send_msg(data, sizeof(hb::net::PacketShopRequest));
}

void shop_manager::handle_response(char* data)
{
	const auto* resp = hb::net::PacketCast<hb::net::PacketShopResponseHeader>(
		data, sizeof(hb::net::PacketShopResponseHeader));
	if (!resp) {
		return;
	}

	uint16_t itemCount = resp->itemCount;

	if (itemCount > game_limits::max_menu_items) {
		itemCount = game_limits::max_menu_items;
	}

	// clear existing shop items
	for (int i = 0; i < game_limits::max_menu_items; i++) {
		m_item_list[i].reset();
	}

	// get item IDs from packet (they follow the header)
	const int16_t* itemIds = reinterpret_cast<const int16_t*>(data + sizeof(hb::net::PacketShopResponseHeader));

	// Populate shop list from item configs
	int shopIndex = 0;
	int skippedCount = 0;
	int notFoundCount = 0;
	for (uint16_t i = 0; i < itemCount && shopIndex < game_limits::max_menu_items; i++) {
		int16_t itemId = itemIds[i];
		if (itemId <= 0 || itemId >= 5000) {
			skippedCount++;
			continue;
		}
		if (m_game->m_item_config_list[itemId] == nullptr) {
			notFoundCount++;
			skippedCount++;
			continue;
		}

		// Create new item for shop based on config
		m_item_list[shopIndex] = std::make_unique<CItem>();
		CItem* item = m_item_list[shopIndex].get();
		CItem* config = m_game->m_item_config_list[itemId].get();

		// Copy item data from config
		item->m_id_num = itemId;
		std::memcpy(item->m_name, config->m_name, sizeof(item->m_name));
		item->m_item_type = config->m_item_type;
		item->m_equip_pos = config->m_equip_pos;
		item->m_sell_price = config->m_sell_price;
		item->m_weight = config->m_weight;
		item->m_item_effect_value1 = config->m_item_effect_value1;
		item->m_item_effect_value2 = config->m_item_effect_value2;
		item->m_item_effect_value3 = config->m_item_effect_value3;
		item->m_item_effect_value4 = config->m_item_effect_value4;
		item->m_item_effect_value5 = config->m_item_effect_value5;
		item->m_item_effect_value6 = config->m_item_effect_value6;
		item->m_durability = config->m_durability;
		item->m_level_requirement = config->m_level_requirement;
		item->m_gender_requirement = config->m_gender_requirement;
		item->m_special_effect = config->m_special_effect;
		item->m_swing_speed = config->m_swing_speed;
		item->m_display_id = config->m_display_id;

		shopIndex++;
	}

	// Only show shop dialog if we have items and there was a pending request
	if (shopIndex > 0 && m_pending_npc_config_id != 0) {
		// enable the SaleMenu dialog - this will call enable_dialog_box which sets up the dialog
		m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::SaleMenu, m_pending_npc_config_id, 0, 0, nullptr);
		m_pending_npc_config_id = 0;  // clear pending request
	} else if (m_pending_npc_config_id != 0) {
		// No items available - show message to user
		m_game->add_event_list("This shop has no items available.", 10);
		m_pending_npc_config_id = 0;
	}
}

