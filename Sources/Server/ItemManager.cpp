#include "ItemManager.h"
#include "Game.h"
#include "StatusEffectManager.h"
#include "WarManager.h"
#include "SkillManager.h"
#include "MagicManager.h"
#include "Item.h"
#include "CombatManager.h"
#include "EntityManager.h"
#include "DynamicObjectManager.h"
#include "DelayEventManager.h"
#include "LootManager.h"
#include "CraftingManager.h"
#include "QuestManager.h"
#include "FishingManager.h"
#include "MiningManager.h"
#include "Packet/SharedPackets.h"
#include "ObjectIDRange.h"
#include "Skill.h"
#include "GameConfigSqliteStore.h"
#include "SharedCalculations.h"
#include "BalanceConstants.h"
#include "Log.h"
#include "ServerLogChannels.h"
#include "StringCompat.h"
#include "TimeUtils.h"

using namespace hb::shared::net;
using namespace hb::shared::direction;

using hb::log_channel;
using namespace hb::shared::action;
using namespace hb::server::net;
using namespace hb::server::config;
using namespace hb::shared::item;
namespace sock = hb::shared::net::socket;
namespace dynamic_object = hb::shared::dynamic_object;
namespace smap = hb::server::map;
namespace sdelay = hb::server::delay_event;
using namespace hb::server::npc;
using namespace hb::server::skill;

extern char G_cTxt[512];
extern char G_cData50000[50000];

static std::string format_item_info(CItem* item)
{
	if (item == nullptr) return "(null)";
	char buf[256];
	std::snprintf(buf, sizeof(buf), "%s(count=%llu pfx=%d:%d sec=%d:%d enc=%d cm=%d touch=%d:%d:%d:%d)",
		item->m_name,
		static_cast<unsigned long long>(item->m_instance.count),
		static_cast<int>(item->m_instance.prefix_type), item->m_instance.prefix_value,
		static_cast<int>(item->m_instance.secondary_type), item->m_instance.secondary_value,
		item->m_instance.enchant_bonus, item->m_instance.custom_made ? 1 : 0,
		item->m_instance.touch_effect_type,
		item->m_instance.touch_effect_value1,
		item->m_instance.touch_effect_value2,
		item->m_instance.touch_effect_value3);
	return buf;
}

static bool is_item_suspicious(CItem* item)
{
	if (item == nullptr) return false;
	if (item->m_id_num == 90) return false; // Gold
	if (item->has_special_attributes() && item->get_touch_effect_type() != TouchEffectType::ID)
		return true;
	if (item->get_touch_effect_type() == TouchEffectType::None && item->has_special_attributes())
		return true;
	return false;
}

// Helper function (duplicated from Game.cpp - static file-scope function)
static void NormalizeItemName(const char* src, char* dst, size_t dstSize)
{
	size_t j = 0;
	for (size_t i = 0; src[i] && j < dstSize - 1; ++i) {
		if (src[i] != ' ' && src[i] != '_') {
			dst[j++] = src[i];
		}
	}
	dst[j] = '\0';
}

bool ItemManager::send_client_item_configs(int client_h)
{
	if (m_game->m_client_list[client_h] == 0) {
		return false;
	}

	// Calculate how many items per packet - keep packets small (~7KB) for reliable delivery
	constexpr size_t maxPacketSize = 7000;
	constexpr size_t headerSize = sizeof(hb::net::PacketItemConfigHeader);
	constexpr size_t entrySize = sizeof(hb::net::PacketItemConfigEntry);
	constexpr size_t maxEntriesPerPacket = (maxPacketSize - headerSize) / entrySize;

	// First count total items
	int totalItems = 0;
	for(int i = 0; i < MaxItemTypes; i++) {
		if (m_game->m_item_config_list[i] != 0) {
			totalItems++;
		}
	}

	// Send items in packets
	int itemsSent = 0;
	int packetIndex = 0;

	while (itemsSent < totalItems) {
		// Build packet
		std::memset(G_cData50000, 0, sizeof(G_cData50000));

		auto* pktHeader = reinterpret_cast<hb::net::PacketItemConfigHeader*>(G_cData50000);
		pktHeader->header.msg_id = MsgId::ItemConfigContents;
		pktHeader->header.msg_type = MsgType::Confirm;
		pktHeader->totalItems = static_cast<uint16_t>(totalItems);
		pktHeader->packetIndex = static_cast<uint16_t>(packetIndex);

		auto* entries = reinterpret_cast<hb::net::PacketItemConfigEntry*>(G_cData50000 + headerSize);

		uint16_t entriesInPacket = 0;
		int configIndex = 0;
		int skipped = 0;

		// Find items for this packet
		for(int i = 0; i < MaxItemTypes && entriesInPacket < maxEntriesPerPacket; i++) {
			if (m_game->m_item_config_list[i] == 0) {
				continue;
			}

			// Skip items already sent in previous packets
			if (skipped < itemsSent) {
				skipped++;
				continue;
			}

			const CItem* item = m_game->m_item_config_list[i];
			auto& entry = entries[entriesInPacket];

			entry.itemId = item->m_id_num;
			std::memset(entry.name, 0, sizeof(entry.name));
			std::snprintf(entry.name, sizeof(entry.name), "%s", item->m_name);
			entry.itemType = item->m_item_type;
			entry.itemSubType = item->m_item_sub_type;
			entry.equipPos = item->m_equip_pos;
			entry.weaponClass = item->m_weapon_class;
			entry.effectType = item->m_item_effect_type;
			entry.effectValue1 = item->m_item_effect_value1;
			entry.effectValue2 = item->m_item_effect_value2;
			entry.effectValue3 = item->m_item_effect_value3;
			entry.effectValue4 = item->m_item_effect_value4;
			entry.effectValue5 = item->m_item_effect_value5;
			entry.effectValue6 = item->m_item_effect_value6;
			entry.durability = item->m_durability;
			entry.specialEffect = item->m_special_effect;
			entry.sellPrice = item->m_sell_price;
			entry.weight = item->m_weight;
			entry.swingSpeed = item->m_swing_speed;
			entry.levelRequirement = item->m_level_requirement;
			entry.genderRequirement = item->m_gender_requirement;
			entry.specialEffectValue1 = item->m_special_effect_value1;
			entry.specialEffectValue2 = item->m_special_effect_value2;
			entry.relatedSkill = item->m_related_skill;
			entry.hideArmor = item->m_hide_armor;
			entry.isSkirt = item->m_is_skirt;
			entry.stackable = item->m_stackable;
			entry.isDyeable = item->m_is_dyeable;
			entry.armorClass = item->m_armor_class;
			entry.setId = item->m_set_id;
			entry.itemColor = item->m_instance.item_color;
			entry.displayId = item->m_display_id;

			entriesInPacket++;
		}

		pktHeader->itemCount = entriesInPacket;
		size_t packetSize = headerSize + (entriesInPacket * entrySize);

		int ret = m_game->m_client_list[client_h]->m_socket->send_msg(G_cData50000, static_cast<int>(packetSize));
		switch (ret) {
		case sock::Event::QueueFull:
		case sock::Event::SocketError:
		case sock::Event::CriticalError:
		case sock::Event::SocketClosed:
			hb::logger::log("Failed to send item configs: Client({}) Packet({})", client_h, packetIndex);
			m_game->delete_client(client_h, true, true);
			delete m_game->m_client_list[client_h];
			m_game->m_client_list[client_h] = 0;
			return false;
		}

		itemsSent += entriesInPacket;
		packetIndex++;
	}

	return true;
}

const DropTable* ItemManager::get_drop_table(int id) const
{
	if (id <= 0) {
		return nullptr;
	}
	auto it = m_game->m_drop_tables.find(id);
	if (it == m_game->m_drop_tables.end()) {
		return nullptr;
	}
	return &it->second;
}

void ItemManager::clear_item_config_list()
{
	for(int i = 0; i < MaxItemTypes; i++) {
		if (m_game->m_item_config_list[i] != 0) {
			delete m_game->m_item_config_list[i];
			m_game->m_item_config_list[i] = 0;
		}
	}
}

bool ItemManager::init_item_attr(CItem* item, const char* item_name)
{
	
	char tmp_name[hb::shared::limits::NpcNameLen];
	char normalized_input[21];
	char normalized_config[21];

	std::memset(tmp_name, 0, sizeof(tmp_name));
	strcpy(tmp_name, item_name);

	// Normalize the input name for comparison (client may send "MagicStaff" while DB has "Magic Staff")
	NormalizeItemName(tmp_name, normalized_input, sizeof(normalized_input));

	for(int i = 0; i < MaxItemTypes; i++)
		if (m_game->m_item_config_list[i] != 0) {
			// Normalize the config name for comparison
			NormalizeItemName(m_game->m_item_config_list[i]->m_name, normalized_config, sizeof(normalized_config));
			if (hb_stricmp(normalized_input, normalized_config) == 0) {
				std::memset(item->m_name, 0, sizeof(item->m_name));
				strcpy(item->m_name, m_game->m_item_config_list[i]->m_name);
				item->m_item_type = m_game->m_item_config_list[i]->m_item_type;
				item->m_equip_pos = m_game->m_item_config_list[i]->m_equip_pos;
				item->m_item_effect_type = m_game->m_item_config_list[i]->m_item_effect_type;
				item->m_item_effect_value1 = m_game->m_item_config_list[i]->m_item_effect_value1;
				item->m_item_effect_value2 = m_game->m_item_config_list[i]->m_item_effect_value2;
				item->m_item_effect_value3 = m_game->m_item_config_list[i]->m_item_effect_value3;
				item->m_item_effect_value4 = m_game->m_item_config_list[i]->m_item_effect_value4;
				item->m_item_effect_value5 = m_game->m_item_config_list[i]->m_item_effect_value5;
				item->m_item_effect_value6 = m_game->m_item_config_list[i]->m_item_effect_value6;
				item->m_durability = m_game->m_item_config_list[i]->m_durability;
				item->m_instance.cur_durability = item->m_durability;
				item->m_special_effect = m_game->m_item_config_list[i]->m_special_effect;

				item->m_sell_price = m_game->m_item_config_list[i]->m_sell_price;
				item->m_weight = m_game->m_item_config_list[i]->m_weight;
				item->m_weapon_class = m_game->m_item_config_list[i]->m_weapon_class;
				item->m_swing_speed = m_game->m_item_config_list[i]->m_swing_speed;
				item->m_level_requirement = m_game->m_item_config_list[i]->m_level_requirement;
				item->m_gender_requirement = m_game->m_item_config_list[i]->m_gender_requirement;

				item->m_special_effect_value1 = m_game->m_item_config_list[i]->m_special_effect_value1;
				item->m_special_effect_value2 = m_game->m_item_config_list[i]->m_special_effect_value2;

				item->m_related_skill = m_game->m_item_config_list[i]->m_related_skill;
				item->m_item_sub_type = m_game->m_item_config_list[i]->m_item_sub_type;
				item->m_id_num = m_game->m_item_config_list[i]->m_id_num;

				item->m_hide_armor = m_game->m_item_config_list[i]->m_hide_armor;
				item->m_is_skirt = m_game->m_item_config_list[i]->m_is_skirt;
				item->m_stackable = m_game->m_item_config_list[i]->m_stackable;
				item->m_is_dyeable = m_game->m_item_config_list[i]->m_is_dyeable;
				item->m_armor_class = m_game->m_item_config_list[i]->m_armor_class;
				item->m_set_id = m_game->m_item_config_list[i]->m_set_id;
				item->m_instance.item_color = m_game->m_item_config_list[i]->m_instance.item_color;
				item->m_display_id = m_game->m_item_config_list[i]->m_display_id;

				return true;
			}
		}

	return false;
}

bool ItemManager::init_item_attr(CItem* item, int item_id)
{
	if (item_id < 0 || item_id >= MaxItemTypes) return false;
	if (m_game->m_item_config_list[item_id] == nullptr) return false;

	CItem* config = m_game->m_item_config_list[item_id];

	std::memset(item->m_name, 0, sizeof(item->m_name));
	strcpy(item->m_name, config->m_name);
	item->m_item_type = config->m_item_type;
	item->m_equip_pos = config->m_equip_pos;
	item->m_item_effect_type = config->m_item_effect_type;
	item->m_item_effect_value1 = config->m_item_effect_value1;
	item->m_item_effect_value2 = config->m_item_effect_value2;
	item->m_item_effect_value3 = config->m_item_effect_value3;
	item->m_item_effect_value4 = config->m_item_effect_value4;
	item->m_item_effect_value5 = config->m_item_effect_value5;
	item->m_item_effect_value6 = config->m_item_effect_value6;
	item->m_durability = config->m_durability;
	item->m_instance.cur_durability = item->m_durability;
	item->m_special_effect = config->m_special_effect;
	item->m_sell_price = config->m_sell_price;
	item->m_weight = config->m_weight;
	item->m_weapon_class = config->m_weapon_class;
	item->m_swing_speed = config->m_swing_speed;
	item->m_level_requirement = config->m_level_requirement;
	item->m_gender_requirement = config->m_gender_requirement;
	item->m_special_effect_value1 = config->m_special_effect_value1;
	item->m_special_effect_value2 = config->m_special_effect_value2;
	item->m_related_skill = config->m_related_skill;
	item->m_item_sub_type = config->m_item_sub_type;
	item->m_id_num = config->m_id_num;
	item->m_hide_armor = config->m_hide_armor;
	item->m_is_skirt = config->m_is_skirt;
	item->m_stackable = config->m_stackable;
	item->m_is_dyeable = config->m_is_dyeable;
	item->m_armor_class = config->m_armor_class;
	item->m_set_id = config->m_set_id;
	item->m_instance.item_color = config->m_instance.item_color;
	item->m_display_id = config->m_display_id;

	return true;
}

void ItemManager::drop_item_handler(int client_h, short item_index, int amount, const char* item_name, bool by_player)
{
	CItem* item;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_on_server_change) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;
	if ((item_index < 0) || (item_index >= hb::shared::limits::MaxItems)) return;
	if (m_game->m_client_list[client_h]->m_item_list[item_index] == 0) return;
	if ((amount != -1) && (amount < 0)) return;

	if ((m_game->m_client_list[client_h]->m_item_list[item_index]->is_stackable()) &&
		(amount == -1))
		amount = static_cast<int>(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.count);

	if ((m_game->m_client_list[client_h]->m_item_list[item_index]->is_stackable()) &&
		(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.count > static_cast<uint64_t>(amount))) {
		item = new CItem;
		if (init_item_attr(item, m_game->m_client_list[client_h]->m_item_list[item_index]->m_name) == false) {
			delete item;
			return;
		}
		else {
			if (amount <= 0) {
				delete item;
				return;
			}
			item->m_instance.count = amount;
		}

		if (static_cast<uint64_t>(amount) > m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.count) {
			delete item;
			return;
		}

		m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.count -= amount;

		// v1.41 !!!
		set_item_count(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.count);

		m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(m_game->m_client_list[client_h]->m_x,
			m_game->m_client_list[client_h]->m_y, item);

		// v1.411 
		// v2.17 2002-7-31
		if (by_player)
			item_log(ItemLogAction::Drop, client_h, (int)-1, item);
		else
			item_log(ItemLogAction::Drop, client_h, (int)-1, item, true);

		m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index,
			m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, item);

		m_game->send_notify_msg(0, client_h, Notify::DropItemFinCountChanged, item_index, amount, 0, 0);
	}
	else {

		release_item_handler(client_h, item_index, true);

		// v2.17
		if (m_game->m_client_list[client_h]->m_is_item_equipped[item_index])
			m_game->send_notify_msg(0, client_h, Notify::ItemReleased, m_game->m_client_list[client_h]->m_item_list[item_index]->m_equip_pos, item_index, 0, 0);

		// v1.432
		if ((m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type() == ItemEffectType::AlterItemDrop) &&
			(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.cur_durability == 0)) {
			delete m_game->m_client_list[client_h]->m_item_list[item_index];
			m_game->m_client_list[client_h]->m_item_list[item_index] = 0;
		}
		else {
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(m_game->m_client_list[client_h]->m_x,
				m_game->m_client_list[client_h]->m_y,
				m_game->m_client_list[client_h]->m_item_list[item_index]);

			// v1.41
			// v2.17 2002-7-31
			if (by_player)
				item_log(ItemLogAction::Drop, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_index]);
			else
				item_log(ItemLogAction::Drop, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_index], true);

			m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index,
				m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y,
				m_game->m_client_list[client_h]->m_item_list[item_index]);
		}

		m_game->m_client_list[client_h]->m_item_list[item_index] = 0;
		m_game->m_client_list[client_h]->m_is_item_equipped[item_index] = false;

		m_game->send_notify_msg(0, client_h, Notify::DropItemFinEraseItem, item_index, amount, 0, 0);

		m_game->m_client_list[client_h]->m_arrow_index = get_arrow_item_index(client_h);
	}

	m_game->calc_total_weight(client_h);
}

int ItemManager::client_motion_get_item_handler(int client_h, short sX, short sY, direction dir)
{
	int   ret, erase_req;
	CItem* item;

	if (m_game->m_client_list[client_h] == 0) return 0;
	if ((dir <= 0) || (dir > 8))       return 0;
	if (m_game->m_client_list[client_h]->m_is_killed) return 0;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return 0;

	if ((sX != m_game->m_client_list[client_h]->m_x) || (sY != m_game->m_client_list[client_h]->m_y)) return 2;

	int st_x, st_y;
	if (m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index] != 0) {
		st_x = m_game->m_client_list[client_h]->m_x / 20;
		st_y = m_game->m_client_list[client_h]->m_y / 20;
		m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].player_activity++;

		switch (m_game->m_client_list[client_h]->m_side) {
		case 0: m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].neutral_activity++; break;
		case 1: m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].aresden_activity++; break;
		case 2: m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_temp_sector_info[st_x][st_y].elvine_activity++;  break;
		}
	}

	m_game->m_skill_manager->clear_skill_using_status(client_h);

	m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->clear_owner(0, client_h, hb::shared::owner_class::Player, sX, sY);
	m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_owner(client_h, hb::shared::owner_class::Player, sX, sY);

	CItem* remain = nullptr;
	item = m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_item(sX, sY, &remain);
	if (item != 0) {
		if (add_client_item_list(client_h, item, &erase_req)) {

			item_log(ItemLogAction::get, client_h, 0, item);

			ret = send_item_notify_msg(client_h, Notify::ItemObtained, item, 0);
			switch (ret) {
			case sock::Event::QueueFull:
			case sock::Event::SocketError:
			case sock::Event::CriticalError:
			case sock::Event::SocketClosed:
				m_game->delete_client(client_h, true, true);
				return 0;
			}

			// Broadcast remaining item state to nearby clients (clears tile if no items remain)
			m_game->send_ground_item_event(CommonType::SetItem,
				m_game->m_client_list[client_h]->m_map_index, sX, sY, remain);
		}
		else
		{
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(sX, sY, item);

			ret = send_item_notify_msg(client_h, Notify::CannotCarryMoreItem, 0, 0);
			switch (ret) {
			case sock::Event::QueueFull:
			case sock::Event::SocketError:
			case sock::Event::CriticalError:
			case sock::Event::SocketClosed:
				m_game->delete_client(client_h, true, true);
				return 0;
			}
		}
	}

	{
		hb::net::PacketResponseMotionHeader pkt{};
		pkt.header.msg_id = MsgId::ResponseMotion;
		pkt.header.msg_type = Confirm::MotionConfirm;
		ret = m_game->m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	switch (ret) {
	case sock::Event::QueueFull:
	case sock::Event::SocketError:
	case sock::Event::CriticalError:
	case sock::Event::SocketClosed:
		m_game->delete_client(client_h, true, true);
		return 0;
	}

	return 1;
}

bool ItemManager::add_client_item_list(int client_h, CItem* item, int* del_req)
{

	if (m_game->m_client_list[client_h] == 0) return false;
	if (item == 0) return false;

	if (item->is_stackable()) {
		if ((m_game->m_client_list[client_h]->m_cur_weight_load + get_item_weight(item, static_cast<int>(item->m_instance.count))) > m_game->calc_max_load(client_h))
			return false;
	}
	else {
		if ((m_game->m_client_list[client_h]->m_cur_weight_load + get_item_weight(item, 1)) > m_game->calc_max_load(client_h))
			return false;
	}

	if (item->is_stackable()) {
		for(int i = 0; i < hb::shared::limits::MaxItems; i++)
			if ((m_game->m_client_list[client_h]->m_item_list[i] != 0) &&
				(m_game->m_client_list[client_h]->m_item_list[i]->m_id_num == item->m_id_num)) {
				m_game->m_client_list[client_h]->m_item_list[i]->m_instance.count += item->m_instance.count;
				//delete item;
				*del_req = 1;

				m_game->calc_total_weight(client_h);

				return true;
			}
	}

	for(int i = 0; i < hb::shared::limits::MaxItems; i++)
		if (m_game->m_client_list[client_h]->m_item_list[i] == 0) {

			m_game->m_client_list[client_h]->m_item_list[i] = item;
			m_game->m_client_list[client_h]->m_item_pos_list[i].x = 40;
			m_game->m_client_list[client_h]->m_item_pos_list[i].y = 30;

			*del_req = 0;

			if (item->get_item_sub_type() == hb::shared::item::item_sub_type::ammo)
				m_game->m_client_list[client_h]->m_arrow_index = get_arrow_item_index(client_h);

			m_game->calc_total_weight(client_h);

			return true;
		}

	return false;
}

int ItemManager::add_client_bulk_item_list(int client_h, const char* item_name, int amount)
{
	if (m_game->m_client_list[client_h] == nullptr) return 0;
	if (item_name == nullptr || amount < 1) return 0;

	int created = 0;
	CItem* first_item = nullptr;

	for (int i = 0; i < amount; i++)
	{
		CItem* item = new CItem();
		if (!init_item_attr(item, item_name))
		{
			delete item;
			break;
		}

		// Weight check
		if ((m_game->m_client_list[client_h]->m_cur_weight_load + get_item_weight(item, 1)) > m_game->calc_max_load(client_h))
		{
			delete item;
			break;
		}

		// Find an empty slot directly (no merge)
		bool added = false;
		for (int j = 0; j < hb::shared::limits::MaxItems; j++)
		{
			if (m_game->m_client_list[client_h]->m_item_list[j] == nullptr)
			{
				m_game->m_client_list[client_h]->m_item_list[j] = item;
				m_game->m_client_list[client_h]->m_item_pos_list[j].x = 40;
				m_game->m_client_list[client_h]->m_item_pos_list[j].y = 30;
				m_game->calc_total_weight(client_h);
				if (first_item == nullptr) first_item = item;
				created++;
				added = true;
				break;
			}
		}

		if (!added)
		{
			delete item;
			break;
		}
	}

	// Send one bulk notification with total count
	if (created > 0 && first_item != nullptr)
	{
		hb::net::PacketNotifyItemObtained pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = Notify::ItemObtainedBulk;
		pkt.is_new = 1;
		memcpy(pkt.name, first_item->m_name, sizeof(pkt.name));
		pkt.count = created;
		pkt.item_type = first_item->m_item_type;
		pkt.equip_pos = first_item->m_equip_pos;
		pkt.is_equipped = 0;
		pkt.level_limit = first_item->m_level_requirement;
		pkt.gender_limit = first_item->m_gender_requirement;
		pkt.cur_durability = first_item->m_instance.cur_durability;
		pkt.weight = first_item->m_weight;
		pkt.item_color = first_item->m_instance.item_color;
		pkt.spec_value2 = static_cast<uint8_t>(first_item->m_instance.special_effect_value2);
		first_item->copy_attributes_to(pkt);
		pkt.item_id = first_item->m_id_num;
		pkt.max_durability = first_item->m_durability;
		m_game->m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}

	return created;
}

bool ItemManager::equip_item_handler(int client_h, short item_index, bool notify)
{
	char hero_armor_type;
	EquipPos equip_pos;

	if (m_game->m_client_list[client_h] == 0) return false;
	if ((item_index < 0) || (item_index >= hb::shared::limits::MaxItems)) return false;
	if (m_game->m_client_list[client_h]->m_item_list[item_index] == 0) return false;
	if (m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_type() != hb::shared::item::item_type::equipment) return false;

	if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.cur_durability == 0) return false;

	if (!(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.custom_made) &&
		(m_game->m_client_list[client_h]->m_item_list[item_index]->m_level_requirement > m_game->m_client_list[client_h]->m_level)) return false;

	if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_gender_requirement != 0) {
		switch (m_game->m_client_list[client_h]->m_type) {
		case 1:
		case 2:
		case 3:
			if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_gender_requirement != 1) return false;
			break;
		case 4:
		case 5:
		case 6:
			if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_gender_requirement != 2) return false;
			break;
		}
	}

	if (get_item_weight(m_game->m_client_list[client_h]->m_item_list[item_index], 1) > (m_game->m_client_list[client_h]->m_str + m_game->m_client_list[client_h]->m_angelic_str) * hb::shared::balance::weight_units_per_stone) return false;

	equip_pos = m_game->m_client_list[client_h]->m_item_list[item_index]->get_equip_pos();

	if ((equip_pos == EquipPos::Body) || (equip_pos == EquipPos::Boots) ||
		(equip_pos == EquipPos::Arms) || (equip_pos == EquipPos::Head)) {
		switch (m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value4) {
		case 10: // Str
			if ((m_game->m_client_list[client_h]->m_str + m_game->m_client_list[client_h]->m_angelic_str) < m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value5) {
				m_game->send_notify_msg(0, client_h, Notify::ItemReleased, m_game->m_client_list[client_h]->m_item_list[item_index]->m_equip_pos, item_index, 0, 0);
				release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(equip_pos)], true);
				return false;
			}
			break;
		case 11: // Dex
			if ((m_game->m_client_list[client_h]->m_dex + m_game->m_client_list[client_h]->m_angelic_dex) < m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value5) {
				m_game->send_notify_msg(0, client_h, Notify::ItemReleased, m_game->m_client_list[client_h]->m_item_list[item_index]->m_equip_pos, item_index, 0, 0);
				release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(equip_pos)], true);
				return false;
			}
			break;
		case 12: // Vit
			if (m_game->m_client_list[client_h]->m_vit < m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value5) {
				m_game->send_notify_msg(0, client_h, Notify::ItemReleased, m_game->m_client_list[client_h]->m_item_list[item_index]->m_equip_pos, item_index, 0, 0);
				release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(equip_pos)], true);
				return false;
			}
			break;
		case 13: // Int
			if ((m_game->m_client_list[client_h]->m_int + m_game->m_client_list[client_h]->m_angelic_int) < m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value5) {
				m_game->send_notify_msg(0, client_h, Notify::ItemReleased, m_game->m_client_list[client_h]->m_item_list[item_index]->m_equip_pos, item_index, 0, 0);
				release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(equip_pos)], true);
				return false;
			}
			break;
		case 14: // Mag
			if ((m_game->m_client_list[client_h]->m_mag + m_game->m_client_list[client_h]->m_angelic_mag) < m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value5) {
				m_game->send_notify_msg(0, client_h, Notify::ItemReleased, m_game->m_client_list[client_h]->m_item_list[item_index]->m_equip_pos, item_index, 0, 0);
				release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(equip_pos)], true);
				return false;
			}
			break;
		case 15: // Chr
			if (m_game->m_client_list[client_h]->m_charisma < m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value5) {
				m_game->send_notify_msg(0, client_h, Notify::ItemReleased, m_game->m_client_list[client_h]->m_item_list[item_index]->m_equip_pos, item_index, 0, 0);
				release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(equip_pos)], true);
				return false;
			}
			break;
		}
	}

	if (equip_pos == EquipPos::TwoHand) {
		// Stormbringer
		if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num == 845) {
			if ((m_game->m_client_list[client_h]->m_int + m_game->m_client_list[client_h]->m_angelic_int) < 65) {
				m_game->send_notify_msg(0, client_h, Notify::ItemReleased, m_game->m_client_list[client_h]->m_special_ability_equip_pos, item_index, 0, 0);
				release_item_handler(client_h, item_index, true);
				return false;
			}
		}
	}

	if (equip_pos == EquipPos::RightHand) {
		// Resurrection wand(MS.10) or Resurrection wand(MS.20)
		if ((m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num == 865) || (m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num == 866)) {
			if ((m_game->m_client_list[client_h]->m_int + m_game->m_client_list[client_h]->m_angelic_int) > 99 && (m_game->m_client_list[client_h]->m_mag + m_game->m_client_list[client_h]->m_angelic_mag) > 99 && m_game->m_client_list[client_h]->m_special_ability_time < 1) {
				m_game->m_client_list[client_h]->m_magic_mastery[94] = true;
				m_game->send_notify_msg(0, client_h, Notify::StateChangeSuccess, 0, 0, 0, 0);
			}
		}
	}

	if ((m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type() == ItemEffectType::AttackSpecAbility) ||
		(m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type() == ItemEffectType::DefenseSpecAbility)) {

		if ((m_game->m_client_list[client_h]->m_special_ability_type != 0)) {
			if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_equip_pos != m_game->m_client_list[client_h]->m_special_ability_equip_pos) {
				m_game->send_notify_msg(0, client_h, Notify::ItemReleased, m_game->m_client_list[client_h]->m_special_ability_equip_pos, m_game->m_client_list[client_h]->m_item_equipment_status[m_game->m_client_list[client_h]->m_special_ability_equip_pos], 0, 0);
				release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[m_game->m_client_list[client_h]->m_special_ability_equip_pos], true);
			}
		}
	}

	if (equip_pos == EquipPos::None) return false;

	if (equip_pos == EquipPos::TwoHand) {
		if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(equip_pos)] != -1)
			release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(equip_pos)], false);
		else {
			if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)] != -1)
				release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)], false);
			if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::LeftHand)] != -1)
				release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::LeftHand)], false);
		}
	}
	else {
		if ((equip_pos == EquipPos::LeftHand) || (equip_pos == EquipPos::RightHand)) {
			if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)] != -1)
				release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)], false);
		}

		if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(equip_pos)] != -1)
			release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(equip_pos)], false);
	}

	if (equip_pos == EquipPos::FullBody) {
		if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(equip_pos)] != -1) {
			release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(equip_pos)], false);
		}
		if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::Head)] != -1) {
			release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::Head)], false);
		}
		if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::Body)] != -1) {
			release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::Body)], false);
		}
		if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::Arms)] != -1) {
			release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::Arms)], false);
		}
		if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::Boots)] != -1) {
			release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::Boots)], false);
		}
		if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::Leggings)] != -1) {
			release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::Leggings)], false);
		}
		if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::Back)] != -1) {
			release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::Back)], false);
		}
	}
	else {
		if (equip_pos == EquipPos::Head || equip_pos == EquipPos::Body || equip_pos == EquipPos::Arms ||
			equip_pos == EquipPos::Boots || equip_pos == EquipPos::Leggings || equip_pos == EquipPos::Back) {
			if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::FullBody)] != -1) {
				release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::FullBody)], false);
			}
		}
		if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(equip_pos)] != -1)
			release_item_handler(client_h, m_game->m_client_list[client_h]->m_item_equipment_status[to_int(equip_pos)], false);
	}

	m_game->m_client_list[client_h]->m_item_equipment_status[to_int(equip_pos)] = item_index;
	m_game->m_client_list[client_h]->m_is_item_equipped[item_index] = true;

	// Set item color and armor visibility for this equip slot
	{
		auto& appr = m_game->m_client_list[client_h]->m_appearance;
		auto* item = m_game->m_client_list[client_h]->m_item_list[item_index];
		uint8_t color = static_cast<uint8_t>(item->m_instance.item_color);
		switch (equip_pos) {
		case EquipPos::Head:      appr.helm_color = color; break;
		case EquipPos::Body:
			appr.armor_color = color;
			appr.hide_armor = (item->m_hide_armor != 0);
			break;
		case EquipPos::Arms:      appr.arm_color = color; break;
		case EquipPos::Leggings:     appr.pants_color = color; break;
		case EquipPos::Boots:  appr.boots_color = color; break;
		case EquipPos::LeftHand:  appr.shield_color = color; break;
		case EquipPos::RightHand:
		case EquipPos::TwoHand:   appr.weapon_color = color; break;
		case EquipPos::Back:      appr.mantle_color = color; break;
		case EquipPos::FullBody:  appr.mantle_color = 0; break;
		default: break;
		}
	}

	// Weapon-specific: compute attack delay and reset combo
	if (equip_pos == EquipPos::RightHand || equip_pos == EquipPos::TwoHand) {
		m_game->m_client_list[client_h]->m_status.attack_delay = static_cast<uint8_t>(hb::shared::calc::attack_delay(
			m_game->m_client_list[client_h]->m_item_list[item_index]->m_swing_speed,
			m_game->m_client_list[client_h]->m_str,
			m_game->m_client_list[client_h]->m_angelic_str));
		m_game->m_client_list[client_h]->m_combo_attack_count = 0;
	}

	// AttackSpecAbility = offensive items (weapons) → weapon_glare
	if (m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type() == ItemEffectType::AttackSpecAbility) {
		m_game->m_client_list[client_h]->m_appearance.weapon_glare = 0;
		switch (m_game->m_client_list[client_h]->m_item_list[item_index]->m_special_effect) {
		case 0: break;
		case 1:
			m_game->m_client_list[client_h]->m_appearance.weapon_glare = 1;
			break;
		case 2:
			m_game->m_client_list[client_h]->m_appearance.weapon_glare = 3;
			break;
		case 3:
			m_game->m_client_list[client_h]->m_appearance.weapon_glare = 2;
			break;
		}
	}

	// DefenseSpecAbility = defensive items (shields) → shield_glare
	if (m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type() == ItemEffectType::DefenseSpecAbility) {
		m_game->m_client_list[client_h]->m_appearance.shield_glare = 0;
		switch (m_game->m_client_list[client_h]->m_item_list[item_index]->m_special_effect) {
		case 0:
			break;
		case 50:
		case 51:
		case 52:
			m_game->m_client_list[client_h]->m_appearance.shield_glare = 1;
			break;
		default:
			break;
		}
	}
	hero_armor_type = check_hero_item_equipped(client_h);
	if (hero_armor_type != 0x0FFFFFFFF) m_game->m_client_list[client_h]->m_hero_armour_bonus = hero_armor_type;

	m_game->send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
	calc_total_item_effect(client_h, item_index, notify);
	validate_equipped_items(client_h);
	return true;

}

void ItemManager::validate_equipped_items(int client_h)
{
	auto* p = m_game->m_client_list[client_h];
	if (p == nullptr) return;

	for (int slot = 0; slot < hb::shared::item::DEF_MAXITEMEQUIPPOS; slot++)
	{
		short item_index = p->m_item_equipment_status[slot];
		if (item_index < 0) continue;
		if (p->m_item_list[item_index] == nullptr) continue;

		auto* item = p->m_item_list[item_index];
		bool must_unequip = false;

		// Level check (skip custom-made items — bit 0 of attribute)
		if (!(item->m_instance.custom_made) &&
			(item->m_level_requirement > p->m_level))
			must_unequip = true;

		// Weight check
		if (!must_unequip &&
			get_item_weight(item, 1) > (p->m_str + p->m_angelic_str) * hb::shared::balance::weight_units_per_stone)
			must_unequip = true;

		// Armor stat requirements (effect_value4 = stat type 10-15)
		if (!must_unequip && item->m_item_effect_value4 >= 10 &&
			item->m_item_effect_value4 <= 15)
		{
			int req = item->m_item_effect_value5;
			switch (item->m_item_effect_value4)
			{
			case 10: must_unequip = (p->m_str + p->m_angelic_str) < req; break;
			case 11: must_unequip = (p->m_dex + p->m_angelic_dex) < req; break;
			case 12: must_unequip = p->m_vit < req; break;
			case 13: must_unequip = (p->m_int + p->m_angelic_int) < req; break;
			case 14: must_unequip = (p->m_mag + p->m_angelic_mag) < req; break;
			case 15: must_unequip = p->m_charisma < req; break;
			}
		}

		if (must_unequip)
			release_item_handler(client_h, item_index, true);
	}
}

void ItemManager::request_purchase_item_handler(int client_h, const char* item_name, int num, int item_id)
{
	CItem* item;
	uint64_t gold_count;
	uint32_t item_count;
	uint16_t temp_price;
	int   ret, erase_req, gold_weight;
	int   cost, discount_ratio, discount_cost;
	double tmp1, tmp2, tmp3;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;
	//if ( (memcmp(m_game->m_client_list[client_h]->m_location, "NONE", 4) != 0) &&
	//	 (memcmp(m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_location_name, m_game->m_client_list[client_h]->m_location, 10) != 0) ) return;

	if (memcmp(m_game->m_client_list[client_h]->m_location, "NONE", 4) != 0) {
		if (memcmp(m_game->m_client_list[client_h]->m_location, "are", 3) == 0) {
			if ((memcmp(m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_location_name, "aresden", 7) == 0) ||
				(memcmp(m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_location_name, "arefarm", 7) == 0)) {

			}
			else return;
		}

		if (memcmp(m_game->m_client_list[client_h]->m_location, "elv", 3) == 0) {
			if ((memcmp(m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_location_name, "elvine", 6) == 0) ||
				(memcmp(m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_location_name, "elvfarm", 7) == 0)) {

			}
			else return;
		}
	}

	// New 18/05/2004
	if (m_game->m_client_list[client_h]->m_is_processing_allowed == false) return;

	// Determine item ID and count from client-provided item ID
	short resolved_item_id = 0;
	item_count = 1;

	if (item_id > 0 && item_id < MaxItemTypes) {
		resolved_item_id = static_cast<short>(item_id);
	}
	else {
		// No valid item ID provided
		return;
	}

	for(int i = 1; i <= num; i++) {

		item = new CItem;
		bool init_ok = init_item_attr(item, resolved_item_id);
		if (init_ok == false) {
			delete item;
		}
		else {

			if (item->m_sell_price == 0) {
				delete item;
				return;
			}

			item->m_instance.count = item_count;

			cost = static_cast<int>(item->m_sell_price * item->m_instance.count);

			gold_count = get_item_count_by_id(client_h, hb::shared::item::ItemId::Gold);

			discount_ratio = ((m_game->m_client_list[client_h]->m_charisma - 10) / 4);

			// 2.03 Discount Method
			// Charisma
			// discount_ratio = (m_game->m_client_list[client_h]->m_charisma / 4) -1;
			// if (discount_ratio == 0) discount_ratio = 1;

			tmp1 = (double)(discount_ratio);
			tmp2 = tmp1 / 100.0f;
			tmp1 = (double)cost;
			tmp3 = tmp1 * tmp2;
			discount_cost = (int)tmp3;

			if (discount_cost >= (cost / 2)) discount_cost = (cost / 2) - 1;
			if (discount_cost < 0) discount_cost = 0;

			if (gold_count < static_cast<uint64_t>(cost - discount_cost)) {
				delete item;

				{
					hb::net::PacketNotifyNotEnoughGold pkt{};
					pkt.header.msg_id = MsgId::Notify;
					pkt.header.msg_type = Notify::NotEnoughGold;
					pkt.item_index = -1;
					ret = m_game->m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
				}
				switch (ret) {
				case sock::Event::QueueFull:
				case sock::Event::SocketError:
				case sock::Event::CriticalError:
				case sock::Event::SocketClosed:
					m_game->delete_client(client_h, true, true);
					return;
				}
				return;
			}

			if (add_client_item_list(client_h, item, &erase_req)) {
				if (m_game->m_client_list[client_h]->m_cur_weight_load < 0) m_game->m_client_list[client_h]->m_cur_weight_load = 0;

				temp_price = (cost - discount_cost);
				ret = send_item_notify_msg(client_h, Notify::ItemPurchased, item, temp_price);
				if (erase_req == 1) delete item;

				// Gold  .      .
				gold_weight = set_item_count_by_id(client_h, hb::shared::item::ItemId::Gold, gold_count - temp_price);
				m_game->calc_total_weight(client_h);

				m_game->m_city_status[m_game->m_client_list[client_h]->m_side].funds += temp_price;

				switch (ret) {
				case sock::Event::QueueFull:
				case sock::Event::SocketError:
				case sock::Event::CriticalError:
				case sock::Event::SocketClosed:
					m_game->delete_client(client_h, true, true);
					return;
				}
			}
			else
			{
				delete item;

				m_game->calc_total_weight(client_h);

				ret = send_item_notify_msg(client_h, Notify::CannotCarryMoreItem, 0, 0);

				switch (ret) {
				case sock::Event::QueueFull:
				case sock::Event::SocketError:
				case sock::Event::CriticalError:
				case sock::Event::SocketClosed:
					m_game->delete_client(client_h, true, true);
					return;
				}
			}
		}
	}
}

void ItemManager::give_item_handler(int client_h, short item_index, int amount, short dX, short dY, uint16_t object_id, const char* item_name)
{
	int ret, erase_req;
	short owner_h;
	char owner_type, char_name[hb::shared::limits::NpcNameLen];
	CItem* item;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_on_server_change) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;
	if (m_game->m_client_list[client_h]->m_item_list[item_index] == 0) return;
	if ((item_index < 0) || (item_index >= hb::shared::limits::MaxItems)) return;
	if (amount <= 0) return;

	std::memset(char_name, 0, sizeof(char_name));

	if ((m_game->m_client_list[client_h]->m_item_list[item_index]->is_stackable()) &&
		(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.count > static_cast<uint64_t>(amount))) {

		item = new CItem;
		if (init_item_attr(item, m_game->m_client_list[client_h]->m_item_list[item_index]->m_name) == false) {
			delete item;
			return;
		}
		else {
			item->m_instance.count = amount;
		}

		m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.count -= amount;

		set_item_count(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.count);

		// dX, dY     .
		m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);

		if (object_id != 0) {
			if (hb::shared::object_id::is_player_id(object_id)) {
				if ((object_id > 0) && (object_id < MaxClients)) {
					if (m_game->m_client_list[object_id] != 0) {
						if ((uint16_t)owner_h != object_id) owner_h = 0;
					}
				}
			}
			else {
				// NPC
				uint16_t npcIdx = hb::shared::object_id::ToNpcIndex(object_id);
				if (hb::shared::object_id::IsNpcID(object_id) && (npcIdx > 0) && (npcIdx < MaxNpcs)) {
					if (m_game->m_npc_list[npcIdx] != 0) {
						if ((uint16_t)owner_h != npcIdx) owner_h = 0;
					}
				}
			}
		}

		if (owner_h == 0) {
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, item);

			// v1.411
			item_log(ItemLogAction::Drop, client_h, 0, item);

			m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index,
				m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, item);
		}
		else {
			if (owner_type == hb::shared::owner_class::Player) {
				memcpy(char_name, m_game->m_client_list[owner_h]->m_char_name, hb::shared::limits::CharNameLen - 1);

				if (owner_h == client_h) {
					delete item;
					return;
				}

				if (add_client_item_list(owner_h, item, &erase_req)) {
					ret = send_item_notify_msg(owner_h, Notify::ItemObtained, item, 0);
					switch (ret) {
					case sock::Event::QueueFull:
					case sock::Event::SocketError:
					case sock::Event::CriticalError:
					case sock::Event::SocketClosed:
						m_game->delete_client(owner_h, true, true);
						break;
					}

					// v1.4
					m_game->send_notify_msg(0, client_h, Notify::GiveItemFinCountChanged, item_index, amount, 0, char_name);
				}
				else {
					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(m_game->m_client_list[client_h]->m_x,
						m_game->m_client_list[client_h]->m_y,
						item);

					// v1.411  
					item_log(ItemLogAction::Drop, client_h, 0, item);

					m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index,
						m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, item);

					{
						ret = send_item_notify_msg(owner_h, Notify::CannotCarryMoreItem, 0, 0);
					}

					switch (ret) {
					case sock::Event::QueueFull:
					case sock::Event::SocketError:
					case sock::Event::CriticalError:
					case sock::Event::SocketClosed:
						m_game->delete_client(owner_h, true, true);
						break;
					}

					m_game->send_notify_msg(0, client_h, Notify::CannotGiveItem, item_index, amount, 0, char_name);
				}

			}
			else {
				// NPC  .
				memcpy(char_name, m_game->m_npc_list[owner_h]->m_npc_name, hb::shared::limits::NpcNameLen - 1);

				if (m_game->m_npc_list[owner_h]->m_npc_config_id == 58) { // Warehouse Keeper
					// NPC     .
					if (set_item_to_bank_item(client_h, item) == false) {
						m_game->send_notify_msg(0, client_h, Notify::CannotItemToBank, 0, 0, 0, 0);

						m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, item);

						// v1.411
						item_log(ItemLogAction::Drop, client_h, 0, item);

						m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index,
							m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, item);
					}
				}
				else {
					// NPC cannot receive items — restore count and reject
					m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.count += amount;
					set_item_count(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.count);
					m_game->send_notify_msg(0, client_h, Notify::CannotGiveItem, item_index, amount, 0, char_name);
					delete item;
					m_game->calc_total_weight(client_h);
					return;
				}
			}
		}
	}
	else {

		release_item_handler(client_h, item_index, true);

		if (m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_sub_type() == hb::shared::item::item_sub_type::ammo)
			m_game->m_client_list[client_h]->m_arrow_index = -1;

		// dX, dY     .
		m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY); // dX, dY   .         .

		if (object_id != 0) {
			if (hb::shared::object_id::is_player_id(object_id)) {
				if ((object_id > 0) && (object_id < MaxClients)) {
					if (m_game->m_client_list[object_id] != 0) {
						if ((uint16_t)owner_h != object_id) owner_h = 0;
					}
				}
			}
			else {
				// NPC
				uint16_t npcIdx = hb::shared::object_id::ToNpcIndex(object_id);
				if (hb::shared::object_id::IsNpcID(object_id) && (npcIdx > 0) && (npcIdx < MaxNpcs)) {
					if (m_game->m_npc_list[npcIdx] != 0) {
						if ((uint16_t)owner_h != npcIdx) owner_h = 0;
					}
				}
			}
		}

		if (owner_h == 0) {
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(m_game->m_client_list[client_h]->m_x,
				m_game->m_client_list[client_h]->m_y,
				m_game->m_client_list[client_h]->m_item_list[item_index]);
			// v1.411  
			item_log(ItemLogAction::Drop, client_h, 0, m_game->m_client_list[client_h]->m_item_list[item_index]);

			m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index,
				m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y,
				m_game->m_client_list[client_h]->m_item_list[item_index]);

			m_game->send_notify_msg(0, client_h, Notify::DropItemFinEraseItem, item_index, amount, 0, 0);
		}
		else {
			// . @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

			if (owner_type == hb::shared::owner_class::Player) {
				memcpy(char_name, m_game->m_client_list[owner_h]->m_char_name, hb::shared::limits::CharNameLen - 1);
				item = m_game->m_client_list[client_h]->m_item_list[item_index];

				{
					if (add_client_item_list(owner_h, item, &erase_req)) {

						item_log(ItemLogAction::Give, client_h, owner_h, item);

						ret = send_item_notify_msg(owner_h, Notify::ItemObtained, item, 0);
						switch (ret) {
						case sock::Event::QueueFull:
						case sock::Event::SocketError:
						case sock::Event::CriticalError:
						case sock::Event::SocketClosed:
							m_game->delete_client(owner_h, true, true);
							break;
						}
					}
					else {
						m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(m_game->m_client_list[client_h]->m_x,
							m_game->m_client_list[client_h]->m_y,
							m_game->m_client_list[client_h]->m_item_list[item_index]);
						item_log(ItemLogAction::Drop, client_h, 0, m_game->m_client_list[client_h]->m_item_list[item_index]);

						m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index,
							m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y,
							m_game->m_client_list[client_h]->m_item_list[item_index]);

						{
							ret = send_item_notify_msg(owner_h, Notify::CannotCarryMoreItem, 0, 0);
						}

						switch (ret) {
						case sock::Event::QueueFull:
						case sock::Event::SocketError:
						case sock::Event::CriticalError:
						case sock::Event::SocketClosed:
							m_game->delete_client(owner_h, true, true);
							break;
						}

						std::memset(char_name, 0, sizeof(char_name));
					}
				}
			}
			else {
				memcpy(char_name, m_game->m_npc_list[owner_h]->m_npc_name, hb::shared::limits::NpcNameLen - 1);

				if (m_game->m_npc_list[owner_h]->m_npc_config_id == 58) { // Warehouse Keeper
					if (set_item_to_bank_item(client_h, item_index) == false) {
						m_game->send_notify_msg(0, client_h, Notify::CannotItemToBank, 0, 0, 0, 0);

						m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(m_game->m_client_list[client_h]->m_x,
							m_game->m_client_list[client_h]->m_y,
							m_game->m_client_list[client_h]->m_item_list[item_index]);

						item_log(ItemLogAction::Drop, client_h, 0, m_game->m_client_list[client_h]->m_item_list[item_index]);

						m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index,
							m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y,
							m_game->m_client_list[client_h]->m_item_list[item_index]);
					}
				}
				else if (m_game->m_npc_list[owner_h]->m_npc_config_id == 56) { // Shop Keeper
					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(m_game->m_client_list[client_h]->m_x,
						m_game->m_client_list[client_h]->m_y,
						m_game->m_client_list[client_h]->m_item_list[item_index]);

					item_log(ItemLogAction::Drop, client_h, 0, m_game->m_client_list[client_h]->m_item_list[item_index]);

					m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index,
						m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y,
						m_game->m_client_list[client_h]->m_item_list[item_index]);

					std::memset(char_name, 0, sizeof(char_name));
				}
				else {
					// NPC cannot receive items — reject and keep item in inventory
					m_game->send_notify_msg(0, client_h, Notify::CannotGiveItem, item_index, amount, 0, char_name);
					m_game->calc_total_weight(client_h);
					return;
				}
			}

			m_game->send_notify_msg(0, client_h, Notify::GiveItemFinEraseItem, item_index, amount, 0, char_name);
		}

		if (m_game->m_client_list[client_h] == 0) return;

		// . delete !
		m_game->m_client_list[client_h]->m_item_list[item_index] = 0;
		m_game->m_client_list[client_h]->m_is_item_equipped[item_index] = false;

		m_game->m_client_list[client_h]->m_arrow_index = get_arrow_item_index(client_h);
	}

	m_game->calc_total_weight(client_h);
}

int ItemManager::set_item_count(int client_h, int item_index, uint64_t count)
{
	uint16_t weight;

	if (m_game->m_client_list[client_h] == 0) return -1;
	if (m_game->m_client_list[client_h]->m_item_list[item_index] == 0) return -1;

	weight = get_item_weight(m_game->m_client_list[client_h]->m_item_list[item_index], 1);//m_game->m_client_list[client_h]->m_item_list[item_index]->m_weight;

	if (count == 0) {
		item_deplete_handler(client_h, item_index, false);
	}
	else {
		m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.count = count;
		m_game->send_notify_msg(0, client_h, Notify::set_item_count, item_index, count, (char)true, 0);
	}

	return weight;
}

uint64_t ItemManager::get_item_count_by_id(int client_h, short item_id)
{
	if (m_game->m_client_list[client_h] == nullptr) return 0;

	for(int i = 0; i < hb::shared::limits::MaxItems; i++) {
		if (m_game->m_client_list[client_h]->m_item_list[i] != nullptr &&
		    m_game->m_client_list[client_h]->m_item_list[i]->m_id_num == item_id) {
			return m_game->m_client_list[client_h]->m_item_list[i]->m_instance.count;
		}
	}

	return 0;
}

int ItemManager::set_item_count_by_id(int client_h, short item_id, uint64_t count)
{
	if (m_game->m_client_list[client_h] == nullptr) return -1;

	for(int i = 0; i < hb::shared::limits::MaxItems; i++) {
		if (m_game->m_client_list[client_h]->m_item_list[i] != nullptr &&
		    m_game->m_client_list[client_h]->m_item_list[i]->m_id_num == item_id) {

			uint16_t weight = get_item_weight(m_game->m_client_list[client_h]->m_item_list[i], 1);

			if (count == 0) {
				item_deplete_handler(client_h, i, false);
			}
			else {
				m_game->m_client_list[client_h]->m_item_list[i]->m_instance.count = count;
				m_game->send_notify_msg(0, client_h, Notify::set_item_count, i, count, (char)true, 0);
			}

			return weight;
		}
	}

	return -1;
}

void ItemManager::release_item_handler(int client_h, short item_index, bool notice)
{
	char hero_armor_type;
	EquipPos equip_pos;

	if (m_game->m_client_list[client_h] == 0) return;
	if ((item_index < 0) || (item_index >= hb::shared::limits::MaxItems)) return;
	if (m_game->m_client_list[client_h]->m_item_list[item_index] == 0) return;
	if (m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_type() != hb::shared::item::item_type::equipment) return;

	if (m_game->m_client_list[client_h]->m_is_item_equipped[item_index] == false) return;

	hero_armor_type = check_hero_item_equipped(client_h);
	if (hero_armor_type != 0x0FFFFFFFF) m_game->m_client_list[client_h]->m_hero_armour_bonus = 0;

	equip_pos = m_game->m_client_list[client_h]->m_item_list[item_index]->get_equip_pos();
	if (equip_pos == EquipPos::RightHand) {
		if (m_game->m_client_list[client_h]->m_item_list[item_index] != 0) {
			if ((m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num == 865) || (m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num == 866)) {
				m_game->m_client_list[client_h]->m_magic_mastery[94] = false;
				m_game->send_notify_msg(0, client_h, Notify::StateChangeSuccess, 0, 0, 0, 0);
			}
		}
	}

	// Clear color and flags for unequipped slot
	switch (equip_pos) {
	case EquipPos::RightHand:
		m_game->m_client_list[client_h]->m_appearance.weapon_color = 0;
		m_game->m_client_list[client_h]->m_status.attack_delay = 0;
		break;
	case EquipPos::LeftHand:
		m_game->m_client_list[client_h]->m_appearance.shield_color = 0;
		break;
	case EquipPos::TwoHand:
		m_game->m_client_list[client_h]->m_appearance.weapon_color = 0;
		break;
	case EquipPos::Body:
		m_game->m_client_list[client_h]->m_appearance.hide_armor = false;
		m_game->m_client_list[client_h]->m_appearance.armor_color = 0;
		break;
	case EquipPos::Back:
		m_game->m_client_list[client_h]->m_appearance.mantle_color = 0;
		break;
	case EquipPos::Arms:
		m_game->m_client_list[client_h]->m_appearance.arm_color = 0;
		break;
	case EquipPos::Leggings:
		m_game->m_client_list[client_h]->m_appearance.pants_color = 0;
		break;
	case EquipPos::Boots:
		m_game->m_client_list[client_h]->m_appearance.boots_color = 0;
		break;
	case EquipPos::Head:
		m_game->m_client_list[client_h]->m_appearance.helm_color = 0;
		break;
	case EquipPos::FullBody:
		m_game->m_client_list[client_h]->m_appearance.mantle_color = 0;
		break;
	}

	// AttackSpecAbility = offensive items (weapons) → weapon_glare
	if (m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type() == ItemEffectType::AttackSpecAbility) {
		m_game->m_client_list[client_h]->m_appearance.weapon_glare = 0;
	}

	// DefenseSpecAbility = defensive items (shields) → shield_glare
	if (m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type() == ItemEffectType::DefenseSpecAbility) {
		m_game->m_client_list[client_h]->m_appearance.shield_glare = 0;
	}

	// Notify the owning client before clearing equip state
	if (notice)
		m_game->send_notify_msg(0, client_h, Notify::ItemReleased, to_int(equip_pos), item_index, 0, 0);

	m_game->m_client_list[client_h]->m_is_item_equipped[item_index] = false;
	m_game->m_client_list[client_h]->m_item_equipment_status[to_int(equip_pos)] = -1;

	if (notice)
		m_game->send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);

	calc_total_item_effect(client_h, item_index, true);
}

void ItemManager::request_retrieve_item_handler(int client_h, char* data)
{
	char bank_item_index;
	int j, ret, item_weight;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;

	const auto* pkt = hb::net::PacketCast<hb::net::PacketRequestRetrieveItem>(
		data, sizeof(hb::net::PacketRequestRetrieveItem));
	if (!pkt) return;
	bank_item_index = static_cast<char>(pkt->item_slot);
	//wh remove
	//if (m_game->m_client_list[client_h]->m_is_inside_warehouse == false) return;

	if ((bank_item_index < 0) || (bank_item_index >= hb::shared::limits::MaxBankItems)) return;
	if (m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index] == 0) {
		// Bank item missing.
		hb::net::PacketResponseRetrieveItem pkt{};
		pkt.header.msg_id = MsgId::ResponseRetrieveItem;
		pkt.header.msg_type = MsgType::Reject;
		pkt.bank_index = 0;
		pkt.item_index = 0;
		ret = m_game->m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	else {
		/*
		if ( m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index]->is_stackable() ) {
			//item_weight = m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index]->m_weight * m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index]->m_instance.count;
			item_weight = get_item_weight(m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index], m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index]->m_instance.count);
		}
		else item_weight = get_item_weight(m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index], 1); //m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index]->m_weight;
		*/
		// v1.432
		item_weight = get_item_weight(m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index], static_cast<int>(m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index]->m_instance.count));

		if ((item_weight + m_game->m_client_list[client_h]->m_cur_weight_load) > m_game->calc_max_load(client_h)) {
		// Notify cannot carry more items.
			ret = send_item_notify_msg(client_h, Notify::CannotCarryMoreItem, 0, 0);
			switch (ret) {
			case sock::Event::QueueFull:
			case sock::Event::SocketError:
			case sock::Event::CriticalError:
			case sock::Event::SocketClosed:
				m_game->delete_client(client_h, true, true);
				break;
			}
			return;
		}

		if (m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index]->is_stackable()) {
			for(int i = 0; i < hb::shared::limits::MaxItems; i++)
				if ((m_game->m_client_list[client_h]->m_item_list[i] != 0) &&
					(m_game->m_client_list[client_h]->m_item_list[i]->get_item_type() == m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index]->get_item_type()) &&
					(m_game->m_client_list[client_h]->m_item_list[i]->m_id_num == m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index]->m_id_num)) {
					// v1.41 !!! 
					set_item_count(client_h, i, m_game->m_client_list[client_h]->m_item_list[i]->m_instance.count + m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index]->m_instance.count);

					delete m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index];
					m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index] = 0;

					for (j = 0; j <= hb::shared::limits::MaxBankItems - 2; j++) {
						if ((m_game->m_client_list[client_h]->m_item_in_bank_list[j + 1] != 0) && (m_game->m_client_list[client_h]->m_item_in_bank_list[j] == 0)) {
							m_game->m_client_list[client_h]->m_item_in_bank_list[j] = m_game->m_client_list[client_h]->m_item_in_bank_list[j + 1];

							m_game->m_client_list[client_h]->m_item_in_bank_list[j + 1] = 0;
						}
					}

					// Send retrieve confirmation.
					hb::net::PacketResponseRetrieveItem pkt{};
					pkt.header.msg_id = MsgId::ResponseRetrieveItem;
					pkt.header.msg_type = MsgType::Confirm;
					pkt.bank_index = bank_item_index;
					pkt.item_index = static_cast<int8_t>(i);

					m_game->calc_total_weight(client_h);
					m_game->m_client_list[client_h]->m_arrow_index = get_arrow_item_index(client_h);

					ret = m_game->m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));

					switch (ret) {
					case sock::Event::QueueFull:
					case sock::Event::SocketError:
					case sock::Event::CriticalError:
					case sock::Event::SocketClosed:
						m_game->delete_client(client_h, true, true);
						return;
					}
					return;
				}

		}

		{
			for(int i = 0; i < hb::shared::limits::MaxItems; i++)
				if (m_game->m_client_list[client_h]->m_item_list[i] == 0) {
					m_game->m_client_list[client_h]->m_item_list[i] = m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index];
					// v1.3 1-27 12:22
					m_game->m_client_list[client_h]->m_item_pos_list[i].x = 40;
					m_game->m_client_list[client_h]->m_item_pos_list[i].y = 30;

					m_game->m_client_list[client_h]->m_is_item_equipped[i] = false;

					m_game->m_client_list[client_h]->m_item_in_bank_list[bank_item_index] = 0;

					for (j = 0; j <= hb::shared::limits::MaxBankItems - 2; j++) {
						if ((m_game->m_client_list[client_h]->m_item_in_bank_list[j + 1] != 0) && (m_game->m_client_list[client_h]->m_item_in_bank_list[j] == 0)) {
							m_game->m_client_list[client_h]->m_item_in_bank_list[j] = m_game->m_client_list[client_h]->m_item_in_bank_list[j + 1];

							m_game->m_client_list[client_h]->m_item_in_bank_list[j + 1] = 0;
						}
					}

					// Send retrieve confirmation.
					hb::net::PacketResponseRetrieveItem pktConfirm{};
					pktConfirm.header.msg_id = MsgId::ResponseRetrieveItem;
					pktConfirm.header.msg_type = MsgType::Confirm;
					pktConfirm.bank_index = bank_item_index;
					pktConfirm.item_index = static_cast<int8_t>(i);

					m_game->calc_total_weight(client_h);
					m_game->m_client_list[client_h]->m_arrow_index = get_arrow_item_index(client_h);

					ret = m_game->m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pktConfirm), sizeof(pktConfirm));
					switch (ret) {
					case sock::Event::QueueFull:
					case sock::Event::SocketError:
					case sock::Event::CriticalError:
					case sock::Event::SocketClosed:
						m_game->delete_client(client_h, true, true);
						return;
					}
					return;
				}
			// No empty inventory slot.
			hb::net::PacketResponseRetrieveItem pktReject{};
			pktReject.header.msg_id = MsgId::ResponseRetrieveItem;
			pktReject.header.msg_type = MsgType::Reject;
			pktReject.bank_index = 0;
			pktReject.item_index = 0;
			ret = m_game->m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pktReject), sizeof(pktReject));
		}
	}

	switch (ret) {
	case sock::Event::QueueFull:
	case sock::Event::SocketError:
	case sock::Event::CriticalError:
	case sock::Event::SocketClosed:
		m_game->delete_client(client_h, true, true);
		return;
	}
}

bool ItemManager::set_item_to_bank_item(int client_h, short item_index)
{
	int ret;
	CItem* item;

	if (m_game->m_client_list[client_h] == 0) return false;
	if ((item_index < 0) || (item_index >= hb::shared::limits::MaxItems)) return false;
	if (m_game->m_client_list[client_h]->m_item_list[item_index] == 0) return false;
	//wh remove
	//if (m_game->m_client_list[client_h]->m_is_inside_warehouse == false) return false;

	for(int i = 0; i < hb::shared::limits::MaxBankItems; i++)
		if (m_game->m_client_list[client_h]->m_item_in_bank_list[i] == 0) {

			m_game->m_client_list[client_h]->m_item_in_bank_list[i] = m_game->m_client_list[client_h]->m_item_list[item_index];
			item = m_game->m_client_list[client_h]->m_item_in_bank_list[i];
			// !!!      NULL .
			m_game->m_client_list[client_h]->m_item_list[item_index] = 0;

			m_game->calc_total_weight(client_h);

			{
				hb::net::PacketNotifyItemToBank pkt{};
				pkt.header.msg_id = MsgId::Notify;
				pkt.header.msg_type = Notify::ItemToBank;
				pkt.bank_index = static_cast<uint8_t>(i);
				pkt.is_new = 1;
				memcpy(pkt.name, item->m_name, sizeof(pkt.name));
				pkt.count = item->m_instance.count;
				pkt.item_type = item->m_item_type;
				pkt.equip_pos = item->m_equip_pos;
				pkt.is_equipped = 0;
				pkt.level_limit = item->m_level_requirement;
				pkt.gender_limit = item->m_gender_requirement;
				pkt.cur_durability = item->m_instance.cur_durability;
				pkt.weight = item->m_weight;
				pkt.item_color = item->m_instance.item_color;
				pkt.item_effect_value2 = item->m_item_effect_value2;
				item->copy_attributes_to(pkt);
				pkt.spec_effect_value2 = static_cast<uint8_t>(item->m_instance.special_effect_value2);
				pkt.item_id = item->m_id_num;
				pkt.max_durability = item->m_durability;
				ret = m_game->m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
			}
			switch (ret) {
			case sock::Event::QueueFull:
			case sock::Event::SocketError:
			case sock::Event::CriticalError:
			case sock::Event::SocketClosed:
				// . v1.41  .
				// m_game->delete_client(client_h, true, true);
				return true;
			}

			return true;
		}

	return false;
}

void ItemManager::calculate_ssn_item_index(int client_h, short weapon_index, int value)
{
	short skill_index;
	int   old_ssn, ss_npoint, weap_idx;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;
	if (m_game->m_client_list[client_h]->m_item_list[weapon_index] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_killed) return;

	skill_index = m_game->m_client_list[client_h]->m_item_list[weapon_index]->m_related_skill;
	if ((skill_index < 0) || (skill_index >= hb::shared::limits::MaxSkillType)) return;
	if (m_game->m_client_list[client_h]->m_skill_mastery[skill_index] == 0) return;

	old_ssn = m_game->m_client_list[client_h]->m_skill_progress[skill_index];
	m_game->m_client_list[client_h]->m_skill_progress[skill_index] += value;

	ss_npoint = m_game->m_skill_progress_threshold[m_game->m_client_list[client_h]->m_skill_mastery[skill_index] + 1];

	if ((m_game->m_client_list[client_h]->m_skill_mastery[skill_index] < 100) &&
		(m_game->m_client_list[client_h]->m_skill_progress[skill_index] > ss_npoint)) {

		m_game->m_client_list[client_h]->m_skill_mastery[skill_index]++;

		switch (skill_index) {
		case 0:  // Mining
		case 5:  // Hand-Attack
		case 13: // Manufacturing
			if (m_game->m_client_list[client_h]->m_skill_mastery[skill_index] > ((m_game->m_client_list[client_h]->m_str + m_game->m_client_list[client_h]->m_angelic_str) * 2)) {
				m_game->m_client_list[client_h]->m_skill_mastery[skill_index]--;
				m_game->m_client_list[client_h]->m_skill_progress[skill_index] = old_ssn;
			}
			else m_game->m_client_list[client_h]->m_skill_progress[skill_index] = 0;
			break;

		case 3: // Magic-Resistance
			if (m_game->m_client_list[client_h]->m_skill_mastery[skill_index] > (m_game->m_client_list[client_h]->m_level * 2)) {
				m_game->m_client_list[client_h]->m_skill_mastery[skill_index]--;
				m_game->m_client_list[client_h]->m_skill_progress[skill_index] = old_ssn;
			}
			else m_game->m_client_list[client_h]->m_skill_progress[skill_index] = 0;
			break;

		case 4:  // Magic
		case 18: // Crafting
		case 21: // Staff-Attack
			if (m_game->m_client_list[client_h]->m_skill_mastery[skill_index] > ((m_game->m_client_list[client_h]->m_mag + m_game->m_client_list[client_h]->m_angelic_mag) * 2)) {
				m_game->m_client_list[client_h]->m_skill_mastery[skill_index]--;
				m_game->m_client_list[client_h]->m_skill_progress[skill_index] = old_ssn;
			}
			else m_game->m_client_list[client_h]->m_skill_progress[skill_index] = 0;
			break;

		case 1:  // Fishing
		case 6:  // Archery
		case 7:  // Short-Sword
		case 8:  // Long-Sword
		case 9:  // Fencing 
		case 10: // Axe-Attack
		case 11: // Shield        	
		case 14: // Hammer 
			if (m_game->m_client_list[client_h]->m_skill_mastery[skill_index] > ((m_game->m_client_list[client_h]->m_dex + m_game->m_client_list[client_h]->m_angelic_dex) * 2)) {
				m_game->m_client_list[client_h]->m_skill_mastery[skill_index]--;
				m_game->m_client_list[client_h]->m_skill_progress[skill_index] = old_ssn;
			}
			else m_game->m_client_list[client_h]->m_skill_progress[skill_index] = 0;
			break;

		case 2:	 // Farming
		case 12: // Alchemy
		case 15:
		case 19: // Pretend-Corpse
		case 20: // Enchanting
			if (m_game->m_client_list[client_h]->m_skill_mastery[skill_index] > ((m_game->m_client_list[client_h]->m_int + m_game->m_client_list[client_h]->m_angelic_int) * 2)) {
				m_game->m_client_list[client_h]->m_skill_mastery[skill_index]--;
				m_game->m_client_list[client_h]->m_skill_progress[skill_index] = old_ssn;
			}
			else m_game->m_client_list[client_h]->m_skill_progress[skill_index] = 0;
			break;

		case 23: // Poison-Resistance
			if (m_game->m_client_list[client_h]->m_skill_mastery[skill_index] > (m_game->m_client_list[client_h]->m_vit * 2)) {
				m_game->m_client_list[client_h]->m_skill_mastery[skill_index]--;
				m_game->m_client_list[client_h]->m_skill_progress[skill_index] = old_ssn;
			}
			else m_game->m_client_list[client_h]->m_skill_progress[skill_index] = 0;
			break;

		default:
			m_game->m_client_list[client_h]->m_skill_progress[skill_index] = 0;
			break;
		}

		if (m_game->m_client_list[client_h]->m_skill_progress[skill_index] == 0) {
			if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)] != -1) {
				weap_idx = m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)];
				if (m_game->m_client_list[client_h]->m_item_list[weap_idx]->m_related_skill == skill_index) {
					m_game->m_client_list[client_h]->m_hit_ratio++;
				}
			}

			if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)] != -1) {
				weap_idx = m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)];
				if (m_game->m_client_list[client_h]->m_item_list[weap_idx]->m_related_skill == skill_index) {
					// Mace    .  1 .
					m_game->m_client_list[client_h]->m_hit_ratio++;
				}
			}
		}

		if (m_game->m_client_list[client_h]->m_skill_progress[skill_index] == 0) {
			// SKill  600     1 .
			m_game->m_skill_manager->check_total_skill_mastery_points(client_h, skill_index);
			// Skill    .
			m_game->send_notify_msg(0, client_h, Notify::Skill, skill_index, m_game->m_client_list[client_h]->m_skill_mastery[skill_index], 0, 0);
		}
	}
}

int ItemManager::get_arrow_item_index(int client_h)
{
	

	if (m_game->m_client_list[client_h] == 0) return -1;

	for(int i = 0; i < hb::shared::limits::MaxItems; i++)
		if (m_game->m_client_list[client_h]->m_item_list[i] != 0) {

			// Arrow  1     .
			if ((m_game->m_client_list[client_h]->m_item_list[i]->get_item_sub_type() == hb::shared::item::item_sub_type::ammo) &&
				(m_game->m_client_list[client_h]->m_item_list[i]->m_instance.count > 0))
				return i;
		}

	return -1;
}

void ItemManager::item_deplete_handler(int client_h, short item_index, bool is_use_item_result)
{

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;
	if ((item_index < 0) || (item_index >= hb::shared::limits::MaxItems)) return;
	if (m_game->m_client_list[client_h]->m_item_list[item_index] == 0) return;

	item_log(ItemLogAction::Deplete, client_h, 0, m_game->m_client_list[client_h]->m_item_list[item_index]);

	release_item_handler(client_h, item_index, true);

	m_game->send_notify_msg(0, client_h, Notify::ItemDepletedEraseItem, item_index, (int)is_use_item_result, 0, 0);

	delete m_game->m_client_list[client_h]->m_item_list[item_index];
	m_game->m_client_list[client_h]->m_item_list[item_index] = 0;

	m_game->m_client_list[client_h]->m_is_item_equipped[item_index] = false;

	// !!! BUG POINT
	// . ArrowIndex     .
	m_game->m_client_list[client_h]->m_arrow_index = get_arrow_item_index(client_h);

	m_game->calc_total_weight(client_h);
}

void ItemManager::use_item_handler(int client_h, short item_index, short dX, short dY, short dest_item_id)
{
	int max, v1, v2, v3, sev1, effect_result = 0;
	uint32_t time;
	short temp_short, tmp_type;
	char slate_type[20];

	time = GameClock::GetTimeMS();
	std::memset(slate_type, 0, sizeof(slate_type));

	//testcode
	//std::snprintf(G_cTxt, sizeof(G_cTxt), "%d", dest_item_id);
	//PutLogList(G_cTxt);

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_killed) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;

	if ((item_index < 0) || (item_index >= hb::shared::limits::MaxItems)) return;
	if (m_game->m_client_list[client_h]->m_item_list[item_index] == 0) return;

	namespace item_type = hb::shared::item::item_type;
	namespace item_sub_type = hb::shared::item::item_sub_type;
	auto it = m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_type();
	auto ist = m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_sub_type();

	if (it != item_type::consumable && it != item_type::tool && ist != item_sub_type::ammo) return;

	if (it == item_type::consumable && ist != item_sub_type::target) {

		switch (m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type()) {
		case ItemEffectType::Warm:

			if (m_game->m_client_list[client_h]->m_magic_effect_status[hb::shared::magic::Ice] == 1) {
				//	m_game->m_status_effect_manager->set_ice_flag(client_h, hb::shared::owner_class::Player, false);

				m_game->m_delay_event_manager->remove_from_delay_event_list(client_h, hb::shared::owner_class::Player, hb::shared::magic::Ice);

				m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (1 * 1000),
					client_h, hb::shared::owner_class::Player, 0, 0, 0, 1, 0, 0);

				//				m_game->send_notify_msg(0, client_h, Notify::MagicEffectOff, hb::shared::magic::Ice, 0, 0, 0);
			}

			m_game->m_client_list[client_h]->m_warm_effect_time = time;
			break;

		case ItemEffectType::Lottery:
			// EV1(:  100) EV2( ) EV3( )
			temp_short = m_game->dice(1, m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1);
			if (temp_short == m_game->dice(1, m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1)) {

			}
			else {

			}
			break;

		case ItemEffectType::Slates:
			if (m_game->m_client_list[client_h]->m_item_list[item_index] != 0) {
				// Full Ancient Slate ??
				if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num == 867) {
					// Slates dont work on Heldenian Map
					switch (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2) {
					case 2: // Bezerk slate
						m_game->m_client_list[client_h]->m_magic_effect_status[hb::shared::magic::Berserk] = true;
						m_game->m_status_effect_manager->set_berserk_flag(client_h, hb::shared::owner_class::Player, true);
						m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Berserk, time + (1000 * 600),
							client_h, hb::shared::owner_class::Player, 0, 0, 0, 1, 0, 0);
						m_game->send_notify_msg(0, client_h, Notify::MagicEffectOn, hb::shared::magic::Berserk, 1, 0, 0);
						strcpy(slate_type, "Berserk");
						break;

					case 1: // Invincible slate
						if (strlen(slate_type) == 0) {
							strcpy(slate_type, "Invincible");
						}
					case 3: // Mana slate
						if (strlen(slate_type) == 0) {
							strcpy(slate_type, "Mana");
						}
					case 4: // Exp slate
						if (strlen(slate_type) == 0) {
							strcpy(slate_type, "Exp");
						}
						set_slate_flag(client_h, m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2, true);
						m_game->m_delay_event_manager->register_delay_event(sdelay::Type::AncientTablet, m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2,
							time + (1000 * 600), client_h, hb::shared::owner_class::Player, 0, 0, 0, 1, 0, 0);
						switch (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2) {
						case 1:
							effect_result = 4;
							break;
						case 3:
							effect_result = 5;
							break;
						case 4:
							effect_result = 6;
							break;
						}
					}
					//if (strlen(slate_type) > 0)
					//	item_log(ItemLogAction::Use, client_h, strlen(slate_type), m_game->m_client_list[client_h]->m_item_list[item_index]);
				}
			}
			break;
		case ItemEffectType::HP:
			max = m_game->get_max_hp(client_h);
			if (m_game->m_client_list[client_h]->m_hp < max) {

				if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1 == 0) {
					v1 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1;
					v2 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
					v3 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value3;
				}
				else {
					v1 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1;
					v2 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2;
					v3 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value3;
				}

				m_game->m_client_list[client_h]->m_hp += (m_game->dice(v1, v2) + v3);
				if (m_game->m_client_list[client_h]->m_hp > max) m_game->m_client_list[client_h]->m_hp = max;
				if (m_game->m_client_list[client_h]->m_hp <= 0)   m_game->m_client_list[client_h]->m_hp = 1;

				effect_result = 1;
			}
			break;

		case ItemEffectType::MP:
			max = m_game->get_max_mp(client_h);

			if (m_game->m_client_list[client_h]->m_mp < max) {

				if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1 == 0) {
					v1 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1;
					v2 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
					v3 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value3;
				}
				else
				{
					v1 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1;
					v2 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2;
					v3 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value3;
				}

				m_game->m_client_list[client_h]->m_mp += (m_game->dice(v1, v2) + v3);
				if (m_game->m_client_list[client_h]->m_mp > max)
					m_game->m_client_list[client_h]->m_mp = max;

				effect_result = 2;
			}
			break;
		case ItemEffectType::CritKomm:
			//CritInc(client_h);
			break;
		case ItemEffectType::SP:
			max = m_game->get_max_sp(client_h);

			if (m_game->m_client_list[client_h]->m_sp < max) {

				if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1 == 0) {
					v1 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1;
					v2 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
					v3 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value3;
				}
				else {
					v1 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1;
					v2 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2;
					v3 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value3;
				}

				m_game->m_client_list[client_h]->m_sp += (m_game->dice(v1, v2) + v3);
				if (m_game->m_client_list[client_h]->m_sp > max)
					m_game->m_client_list[client_h]->m_sp = max;

				effect_result = 3;
			}

			if (m_game->m_client_list[client_h]->m_is_poisoned) {
				m_game->m_client_list[client_h]->m_is_poisoned = false;
				m_game->m_status_effect_manager->set_poison_flag(client_h, hb::shared::owner_class::Player, false);
				m_game->send_notify_msg(0, client_h, Notify::MagicEffectOff, hb::shared::magic::Poison, 0, 0, 0);
				m_game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0, "Poison has been cured.");
			}
			break;

		case ItemEffectType::HPStock:
			v1 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1;
			v2 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
			v3 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value3;

			m_game->m_client_list[client_h]->m_hp_stock += m_game->dice(v1, v2) + v3;
			if (m_game->m_client_list[client_h]->m_hp_stock < 0)   m_game->m_client_list[client_h]->m_hp_stock = 0;
			if (m_game->m_client_list[client_h]->m_hp_stock > 500) m_game->m_client_list[client_h]->m_hp_stock = 500;

			m_game->m_client_list[client_h]->m_hunger_status += m_game->dice(v1, v2) + v3;
			if (m_game->m_client_list[client_h]->m_hunger_status > 100) m_game->m_client_list[client_h]->m_hunger_status = 100;
			if (m_game->m_client_list[client_h]->m_hunger_status < 0)   m_game->m_client_list[client_h]->m_hunger_status = 0;
			m_game->send_notify_msg(0, client_h, Notify::Hunger, m_game->m_client_list[client_h]->m_hunger_status, 0, 0, 0);
			break;

		case ItemEffectType::StudySkill:
			v1 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1;
			v2 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
			sev1 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1;
			// v1  Skill . v2  , sev1    ()
			if (sev1 == 0) {
				m_game->m_skill_manager->train_skill_response(true, client_h, v1, v2);
			}
			else {
				m_game->m_skill_manager->train_skill_response(true, client_h, v1, sev1);
			}
			break;

		case ItemEffectType::StudyMagic:
			// v1   .
			v1 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1;
			if (m_game->m_magic_config_list[v1] != 0)
				m_game->m_magic_manager->request_study_magic_handler(client_h, m_game->m_magic_config_list[v1]->m_name, false);
			break;

			/*case ItemEffectType::Lottery:
				lottery = m_game->dice(1, m_game->m_client_list[client_h]->m_item_list[item_index]->
				break;*/

				// New 15/05/2004 Changed
		case ItemEffectType::Magic:
			if (m_game->m_client_list[client_h]->m_status.invisibility) {
				m_game->m_status_effect_manager->set_invisibility_flag(client_h, hb::shared::owner_class::Player, false);

				m_game->m_delay_event_manager->remove_from_delay_event_list(client_h, hb::shared::owner_class::Player, hb::shared::magic::Invisibility);
				m_game->m_client_list[client_h]->m_magic_effect_status[hb::shared::magic::Invisibility] = 0;
			}

			switch (m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1) {
			case 1:
				// Recall    .
				// testcode
				m_game->request_teleport_handler(client_h, "1   ");
				break;

			case 2:
				m_game->m_magic_manager->player_magic_handler(client_h, m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, 32, true);
				break;

			case 3:
				if (m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_is_fight_zone == false)
					m_game->m_magic_manager->player_magic_handler(client_h, m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, 34, true);
				break;

			case 4:
				// fixed location teleportation:
				switch (m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2) {
				case 1:
					if (memcmp(m_game->m_client_list[client_h]->m_map_name, "bisle", 5) != 0) {
						//v1.42
						item_deplete_handler(client_h, item_index, true);
						m_game->request_teleport_handler(client_h, "2   ", "bisle", -1, -1);
					}
					break;
				case 2: //lotery
					item_deplete_handler(client_h, item_index, true);
					m_game->lotery_handler(client_h);
					break;

				case 11:
				case 12:
				case 13:
				case 14:
				case 15:
				case 16:
				case 17:
				case 18:
				case 19:
					hb::time::local_time SysTime{};

					SysTime = hb::time::local_time::now();
					if ((m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value1 != SysTime.month) ||
						(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value2 != SysTime.day) ||
						(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value3 <= SysTime.hour)) {
					}
					else {
						char dest_map_name[hb::shared::limits::MapNameLen]{};
						int zoneNum = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2 - 10;
						std::snprintf(dest_map_name, sizeof(dest_map_name), "fightzone%c", '0' + (zoneNum % 10));
						if (memcmp(m_game->m_client_list[client_h]->m_map_name, dest_map_name, 10) != 0) {
							//v1.42
							item_deplete_handler(client_h, item_index, true);
							m_game->request_teleport_handler(client_h, "2   ", dest_map_name, -1, -1);
						}
					}
					break;
				}
				break;

			case 5:
				m_game->m_magic_manager->player_magic_handler(client_h, m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, 31, true,
					m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2);
				break;
			}
			break;

		case ItemEffectType::FirmStamina:
			m_game->m_client_list[client_h]->m_time_left_firm_stamina += m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1;
			if (m_game->m_client_list[client_h]->m_time_left_firm_stamina > 20 * 30) m_game->m_client_list[client_h]->m_time_left_firm_stamina = 20 * 30;
			break;

		case ItemEffectType::ChangeAttr:
			switch (m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1) {
			case 1:
				m_game->m_client_list[client_h]->m_hair_color++;
				if (m_game->m_client_list[client_h]->m_hair_color > 15) m_game->m_client_list[client_h]->m_hair_color = 0;

				m_game->m_client_list[client_h]->m_appearance.hair_style = m_game->m_client_list[client_h]->m_hair_style;
				m_game->m_client_list[client_h]->m_appearance.hair_color = m_game->m_client_list[client_h]->m_hair_color;
				m_game->m_client_list[client_h]->m_appearance.underwear_type = m_game->m_client_list[client_h]->m_underwear;
				break;

			case 2:
				m_game->m_client_list[client_h]->m_hair_style++;
				if (m_game->m_client_list[client_h]->m_hair_style > 7) m_game->m_client_list[client_h]->m_hair_style = 0;

				m_game->m_client_list[client_h]->m_appearance.hair_style = m_game->m_client_list[client_h]->m_hair_style;
				m_game->m_client_list[client_h]->m_appearance.hair_color = m_game->m_client_list[client_h]->m_hair_color;
				m_game->m_client_list[client_h]->m_appearance.underwear_type = m_game->m_client_list[client_h]->m_underwear;
				break;

			case 3:
				// Appearance , .
				m_game->m_client_list[client_h]->m_skin++;
				if (m_game->m_client_list[client_h]->m_skin > 3)
					m_game->m_client_list[client_h]->m_skin = 1;

				if (m_game->m_client_list[client_h]->m_sex == 1)      temp_short = 1;
				else if (m_game->m_client_list[client_h]->m_sex == 2) temp_short = 4;

				switch (m_game->m_client_list[client_h]->m_skin) {
				case 2:	temp_short += 1; break;
				case 3:	temp_short += 2; break;
				}
				m_game->m_client_list[client_h]->m_type = temp_short;
				break;

			case 4:
			{
				auto* client = m_game->m_client_list[client_h];

				// Toggle sex
				if (client->m_sex == 1)
					client->m_sex = 2;
				else
					client->m_sex = 1;

				// Update character type based on new sex + skin
				tmp_type = (client->m_sex == 1) ? 1 : 4;
				switch (client->m_skin)
				{
				case 2: tmp_type += 1; break;
				case 3: tmp_type += 2; break;
				}
				client->m_type = tmp_type;
				client->m_appearance.hair_style = client->m_hair_style;
				client->m_appearance.hair_color = client->m_hair_color;
				client->m_appearance.underwear_type = client->m_underwear;

				// Swap all gendered hero items (403-426) to the correct sex variant.
				// Covers inventory (including equipped) and warehouse.
				bool is_female = (client->m_sex == 2);
				constexpr int gendered_first = ItemId::AresdenHeroHelmM;
				constexpr int gendered_last = ItemId::ElvineHeroLeggingsW;
				constexpr int group_size = 4;

				auto swap_hero_item = [&](CItem* item) -> bool
				{
					if (item == nullptr) return false;
					if (item->m_id_num < gendered_first || item->m_id_num > gendered_last) return false;

					int group_base = gendered_first
						+ ((item->m_id_num - gendered_first) / group_size) * group_size;
					int current_offset = item->m_id_num - group_base;
					int side_offset = (current_offset >= 2) ? 2 : 0;
					int sex_offset = is_female ? 1 : 0;
					int new_id = group_base + side_offset + sex_offset;

					if (new_id == item->m_id_num) return false;

					CItem* config = m_game->m_item_config_list[new_id];
					if (config == nullptr) return false;

					std::memcpy(item->m_name, config->m_name, sizeof(item->m_name));
					item->m_id_num = config->m_id_num;
					item->m_gender_requirement = config->m_gender_requirement;
					item->m_weapon_class = config->m_weapon_class;
					item->m_hide_armor = config->m_hide_armor;
					item->m_is_skirt = config->m_is_skirt;
					return true;
				};

				// Swap inventory items and notify client in-place (preserves positions)
				for (int i = 0; i < hb::shared::limits::MaxItems; i++)
				{
					if (swap_hero_item(client->m_item_list[i]))
					{
						CItem* item = client->m_item_list[i];
						m_game->send_gizon_item_change(client_h, i, item);
					}
				}

				// Swap bank items and notify client per-item
				for (int i = 0; i < hb::shared::limits::MaxBankItems; i++)
				{
					if (swap_hero_item(client->m_item_in_bank_list[i]))
					{
						CItem* item = client->m_item_in_bank_list[i];
						hb::net::PacketNotifyItemToBank pkt{};
						pkt.header.msg_id = MsgId::Notify;
						pkt.header.msg_type = Notify::ItemToBank;
						pkt.bank_index = static_cast<uint8_t>(i);
						pkt.is_new = 0;
						std::memcpy(pkt.name, item->m_name, sizeof(pkt.name));
						pkt.count = item->m_instance.count;
						pkt.item_type = item->m_item_type;
						pkt.equip_pos = item->m_equip_pos;
						pkt.is_equipped = 0;
						pkt.level_limit = item->m_level_requirement;
						pkt.gender_limit = item->m_gender_requirement;
						pkt.cur_durability = item->m_instance.cur_durability;
						pkt.weight = item->m_weight;
						pkt.item_color = item->m_instance.item_color;
						pkt.item_effect_value2 = item->m_item_effect_value2;
						item->copy_attributes_to(pkt);
						pkt.spec_effect_value2 = static_cast<uint8_t>(item->m_instance.special_effect_value2);
						pkt.item_id = item->m_id_num;
						pkt.max_durability = item->m_durability;
						client->m_socket->send_msg(
							reinterpret_cast<char*>(&pkt), sizeof(pkt));
					}
				}

				// Unequip all non-hero equipped items (except neck, rings, angels)
				// so the client refreshes their sprites with the new gender.
				// Hero items are already swapped above. Accessories have no gender sprites.
				for (int i = 0; i < hb::shared::limits::MaxItems; i++)
				{
					if (client->m_item_list[i] == nullptr) continue;
					if (client->m_is_item_equipped[i] == false) continue;

					// Skip hero items (already handled by swap above)
					int id = client->m_item_list[i]->m_id_num;
					if (id >= gendered_first && id <= gendered_last) continue;

					// Skip accessories: neck, rings, angel pendants
					EquipPos pos = client->m_item_list[i]->get_equip_pos();
					if (pos == EquipPos::Neck || pos == EquipPos::RightFinger || pos == EquipPos::LeftFinger) continue;

					release_item_handler(client_h, static_cast<short>(i), false);
					m_game->send_notify_msg(0, client_h, Notify::ItemReleased, static_cast<int>(pos), i, 0, 0);
				}

				break;
			}
			}

			m_game->send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
			break;
		}
		// *** Request Teleport Handler           .
		item_deplete_handler(client_h, item_index, true);

		switch (effect_result) {
		case 1:
			m_game->send_notify_msg(0, client_h, Notify::Hp, 0, 0, 0, 0);
			break;
		case 2:
			m_game->send_notify_msg(0, client_h, Notify::Mp, 0, 0, 0, 0);
			break;
		case 3:
			m_game->send_notify_msg(0, client_h, Notify::Sp, 0, 0, 0, 0);
			break;
		case 4: // Invincible
			m_game->send_notify_msg(0, client_h, Notify::SlateInvincible, 0, 0, 0, 0);
			break;
		case 5: // Mana
			m_game->send_notify_msg(0, client_h, Notify::SlateMana, 0, 0, 0, 0);
			break;
		case 6: // EXP
			m_game->send_notify_msg(0, client_h, Notify::SlateExp, 0, 0, 0, 0);
			break;
		default:
			break;
		}
	}
	else if (ist == item_sub_type::target) {
		if (deplete_dest_type_item_use_effect(client_h, dX, dY, item_index, dest_item_id))
			item_deplete_handler(client_h, item_index, true);
	}
	else if (ist == item_sub_type::ammo) {
		m_game->m_client_list[client_h]->m_arrow_index = get_arrow_item_index(client_h);
	}
	else if (it == item_type::tool && m_game->m_client_list[client_h]->m_item_list[item_index]->m_durability == 0) {
		// .     . (ex: )
		switch (m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type()) {
		case ItemEffectType::ShowLocation:
			v1 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1;
			switch (v1) {
			case 1:
				if (strcmp(m_game->m_client_list[client_h]->m_map_name, "aresden") == 0)
					m_game->send_notify_msg(0, client_h, Notify::ShowMap, v1, 1, 0, 0);
				else if (strcmp(m_game->m_client_list[client_h]->m_map_name, "elvine") == 0)
					m_game->send_notify_msg(0, client_h, Notify::ShowMap, v1, 2, 0, 0);
				else if (strcmp(m_game->m_client_list[client_h]->m_map_name, "middleland") == 0)
					m_game->send_notify_msg(0, client_h, Notify::ShowMap, v1, 3, 0, 0);
				else if (strcmp(m_game->m_client_list[client_h]->m_map_name, "default") == 0)
					m_game->send_notify_msg(0, client_h, Notify::ShowMap, v1, 4, 0, 0);
				else if (strcmp(m_game->m_client_list[client_h]->m_map_name, "huntzone2") == 0)
					m_game->send_notify_msg(0, client_h, Notify::ShowMap, v1, 5, 0, 0);
				else if (strcmp(m_game->m_client_list[client_h]->m_map_name, "huntzone1") == 0)
					m_game->send_notify_msg(0, client_h, Notify::ShowMap, v1, 6, 0, 0);
				else if (strcmp(m_game->m_client_list[client_h]->m_map_name, "huntzone4") == 0)
					m_game->send_notify_msg(0, client_h, Notify::ShowMap, v1, 7, 0, 0);
				else if (strcmp(m_game->m_client_list[client_h]->m_map_name, "huntzone3") == 0)
					m_game->send_notify_msg(0, client_h, Notify::ShowMap, v1, 8, 0, 0);
				else if (strcmp(m_game->m_client_list[client_h]->m_map_name, "arefarm") == 0)
					m_game->send_notify_msg(0, client_h, Notify::ShowMap, v1, 9, 0, 0);
				else if (strcmp(m_game->m_client_list[client_h]->m_map_name, "elvfarm") == 0)
					m_game->send_notify_msg(0, client_h, Notify::ShowMap, v1, 10, 0, 0);
				else m_game->send_notify_msg(0, client_h, Notify::ShowMap, v1, 0, 0, 0);
				break;
			}
			break;
		}
	}
	else if (it == item_type::tool && m_game->m_client_list[client_h]->m_item_list[item_index]->m_durability > 0) {

		if ((m_game->m_client_list[client_h]->m_item_list[item_index] == 0) ||
			(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.cur_durability <= 0) ||
			(m_game->m_client_list[client_h]->m_skill_using_status[m_game->m_client_list[client_h]->m_item_list[item_index]->m_related_skill])) {
			return;
		}
		else {
			if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_durability != 0) {
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.cur_durability--;
				m_game->send_notify_msg(0, client_h, Notify::CurDurability, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.cur_durability, 0, 0);
				if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.cur_durability <= 0) {
					m_game->send_notify_msg(0, client_h, Notify::ItemDurabilityEnd, to_int(EquipPos::None), item_index, 0, 0);
				}
				else {
					// ID . v1.12
					int skill_using_time_id = (int)GameClock::GetTimeMS();

					m_game->m_delay_event_manager->register_delay_event(sdelay::Type::UseItemSkill, m_game->m_client_list[client_h]->m_item_list[item_index]->m_related_skill,
						time + m_game->m_skill_config_list[m_game->m_client_list[client_h]->m_item_list[item_index]->m_related_skill]->m_value_2 * 1000,
						client_h, hb::shared::owner_class::Player, m_game->m_client_list[client_h]->m_map_index, dX, dY,
						m_game->m_client_list[client_h]->m_skill_mastery[m_game->m_client_list[client_h]->m_item_list[item_index]->m_related_skill], skill_using_time_id, 0);

					m_game->m_client_list[client_h]->m_skill_using_status[m_game->m_client_list[client_h]->m_item_list[item_index]->m_related_skill] = true;
					m_game->m_client_list[client_h]->m_skill_using_time_id[m_game->m_client_list[client_h]->m_item_list[item_index]->m_related_skill] = skill_using_time_id; //v1.12
				}
			}
		}
	}
}

bool ItemManager::set_item_to_bank_item(int client_h, CItem* item)
{
	int ret;

	if (m_game->m_client_list[client_h] == 0) return false;
	if (item == 0) return false;
	//wh remove
	//if (m_game->m_client_list[client_h]->m_is_inside_warehouse == false) return false;

	for(int i = 0; i < hb::shared::limits::MaxBankItems; i++)
		if (m_game->m_client_list[client_h]->m_item_in_bank_list[i] == 0) {

			m_game->m_client_list[client_h]->m_item_in_bank_list[i] = item;

			{
				hb::net::PacketNotifyItemToBank pkt{};
				pkt.header.msg_id = MsgId::Notify;
				pkt.header.msg_type = Notify::ItemToBank;
				pkt.bank_index = static_cast<uint8_t>(i);
				pkt.is_new = 1;
				memcpy(pkt.name, item->m_name, sizeof(pkt.name));
				pkt.count = item->m_instance.count;
				pkt.item_type = item->m_item_type;
				pkt.equip_pos = item->m_equip_pos;
				pkt.is_equipped = 0;
				pkt.level_limit = item->m_level_requirement;
				pkt.gender_limit = item->m_gender_requirement;
				pkt.cur_durability = item->m_instance.cur_durability;
				pkt.weight = item->m_weight;
				pkt.item_color = item->m_instance.item_color;
				pkt.item_effect_value2 = item->m_item_effect_value2;
				item->copy_attributes_to(pkt);
				pkt.spec_effect_value2 = static_cast<uint8_t>(item->m_instance.special_effect_value2);
				pkt.item_id = item->m_id_num;
				pkt.max_durability = item->m_durability;
				ret = m_game->m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
			}
			switch (ret) {
			case sock::Event::QueueFull:
			case sock::Event::SocketError:
			case sock::Event::CriticalError:
			case sock::Event::SocketClosed:
				// . v1.41  .
				// m_game->delete_client(client_h, true, true);
				return true;
			}

			return true;
		}

	return false;
}

int ItemManager::calculate_use_skill_item_effect(int owner_h, char owner_type, char owner_skill, int skill_num, char map_index, int dX, int dY)
{
	CItem* item;
	char  item_name[hb::shared::limits::ItemNameLen];
	short lX, lY;
	int   result, fish;

	switch (owner_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[owner_h] == 0) return 0;
		if (m_game->m_client_list[owner_h]->m_map_index != map_index) return 0;
		lX = m_game->m_client_list[owner_h]->m_x;
		lY = m_game->m_client_list[owner_h]->m_y;
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[owner_h] == 0) return 0;
		if (m_game->m_npc_list[owner_h]->m_map_index != map_index) return 0;
		lX = m_game->m_npc_list[owner_h]->m_x;
		lY = m_game->m_npc_list[owner_h]->m_y;
		break;
	}

	if (owner_skill == 0) return 0;

	// 100       1D105
	result = m_game->dice(1, 105);
	if (owner_skill <= result)	return 0;

	if (m_game->m_map_list[map_index]->get_is_water(dX, dY) == false) return 0;

	if (owner_type == hb::shared::owner_class::Player)
		m_game->m_skill_manager->calculate_ssn_skill_index(owner_h, skill_num, 1);

	switch (m_game->m_skill_config_list[skill_num]->m_type) {
	case EffectType::Taming:
		// : dX, dY   .
		m_game->m_skill_manager->taming_handler(owner_h, skill_num, map_index, dX, dY);
		break;

	case EffectType::get:
		std::memset(item_name, 0, sizeof(item_name));
		bool is_fish = false;
		switch (m_game->m_skill_config_list[skill_num]->m_value_1) {
		case 1:
			std::snprintf(item_name, sizeof(item_name), "Meat");
			break;

		case 2:
			if (owner_type == hb::shared::owner_class::Player) {
				fish = m_game->m_fishing_manager->check_fish(owner_h, map_index, dX, dY);
				if (fish == 0) {
					std::snprintf(item_name, sizeof(item_name), "Fish");
					is_fish = true;
				}
			}
			else {
				std::snprintf(item_name, sizeof(item_name), "Fish");
				is_fish = true;
			}
			break;
		}

		if (strlen(item_name) != 0) {

			if (is_fish) {
				m_game->send_notify_msg(0, owner_h, Notify::FishSuccess, 0, 0, 0, 0);
				m_game->m_client_list[owner_h]->m_exp_stock += m_game->dice(1, 2);
			}

			item = new CItem;
			if (item == 0) return 0;
			if (init_item_attr(item, item_name)) {
				m_game->m_map_list[map_index]->set_item(lX, lY, item);

				m_game->send_ground_item_event(CommonType::ItemDrop, map_index, lX, lY, item);
			}
		}
		break;
	}

	return 1;
}

void ItemManager::req_sell_item_handler(int client_h, char item_id, char sell_to_whom, int num, const char* item_name)
{
	short remain_life;
	int   price;
	double d1, d2, d3;
	bool   neutral;
	uint32_t  swe_type, swe_value, add_price1, add_price2, mul1, mul2;
	CItem* m_pGold;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;
	if ((item_id < 0) || (item_id >= 50)) return;
	if (m_game->m_client_list[client_h]->m_item_list[item_id] == 0) return;
	if (num <= 0) return;
	if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.count < static_cast<uint32_t>(num)) return;

	// Can't sell gold
	if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_id_num == hb::shared::item::ItemId::Gold)
	{
		m_game->send_notify_msg(0, client_h, Notify::CannotSellItem, item_id, 1, 0, m_game->m_client_list[client_h]->m_item_list[item_id]->m_name);
		return;
	}

	m_game->calc_total_weight(client_h);

	m_pGold = new CItem;
	init_item_attr(m_pGold, hb::shared::item::ItemId::Gold);

	// v1.42
	neutral = false;
	if (memcmp(m_game->m_client_list[client_h]->m_location, "NONE", 4) == 0) neutral = true;
	switch (sell_to_whom) {
	case 15:
	case 24:
		if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_sell_price == 0) {
			m_game->send_notify_msg(0, client_h, Notify::CannotSellItem, item_id, 1, 0, m_game->m_client_list[client_h]->m_item_list[item_id]->m_name);
			break;
		}

		if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_durability > 0) {
			// Equipment with durability: price scaled by remaining durability
			remain_life = m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.cur_durability;

			if (remain_life == 0) {
				m_game->send_notify_msg(0, client_h, Notify::CannotSellItem, item_id, 2, 0, m_game->m_client_list[client_h]->m_item_list[item_id]->m_name);
				break;
			}

			d1 = (double)remain_life;
			if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_durability != 0)
				d2 = (double)m_game->m_client_list[client_h]->m_item_list[item_id]->m_durability;
			else d2 = 1.0f;
			d3 = (d1 / d2) * 0.5f;
			d2 = (double)m_game->m_client_list[client_h]->m_item_list[item_id]->m_sell_price;
			d3 = d3 * d2;

			price = (int)d3;
			price = price * num;

			// Attribute bonus pricing for equipment with special attributes
			add_price1 = 0;
			add_price2 = 0;
			if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.prefix_type != static_cast<uint8_t>(hb::shared::item::AttributePrefixType::None)) {
				swe_type = static_cast<int>(m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.prefix_type);
				swe_value = m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.prefix_value;

				switch (swe_type) {
				case 6: mul1 = 2; break;
				case 8: mul1 = 2; break;
				case 5: mul1 = 3; break;
				case 1: mul1 = 4; break;
				case 7: mul1 = 5; break;
				case 2: mul1 = 6; break;
				case 3: mul1 = 15; break;
				case 9: mul1 = 20; break;
				default: mul1 = 1; break;
				}

				d1 = (double)price * mul1;
				switch (swe_value) {
				case 1: d2 = 10.0f; break;
				case 2: d2 = 20.0f; break;
				case 3: d2 = 30.0f; break;
				case 4: d2 = 35.0f; break;
				case 5: d2 = 40.0f; break;
				case 6: d2 = 50.0f; break;
				case 7: d2 = 100.0f; break;
				case 8: d2 = 200.0f; break;
				case 9: d2 = 300.0f; break;
				case 10: d2 = 400.0f; break;
				case 11: d2 = 500.0f; break;
				case 12: d2 = 700.0f; break;
				case 13: d2 = 900.0f; break;
				default: d2 = 0.0f; break;
				}
				d3 = d1 * (d2 / 100.0f);

				add_price1 = (int)(d1 + d3);
			}

			if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.secondary_type != static_cast<uint8_t>(hb::shared::item::SecondaryEffectType::None)) {
				swe_type = static_cast<int>(m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.secondary_type);
				swe_value = m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.secondary_value;

				switch (swe_type) {
				case 1:
				case 12: mul2 = 2; break;

				case 2:
				case 3:
				case 4:
				case 5:
				case 6:
				case 7: mul2 = 4; break;

				case 8:
				case 9:
				case 10:
				case 11: mul2 = 6; break;
				}

				d1 = (double)price * mul2;
				switch (swe_value) {
				case 1: d2 = 10.0f; break;
				case 2: d2 = 20.0f; break;
				case 3: d2 = 30.0f; break;
				case 4: d2 = 35.0f; break;
				case 5: d2 = 40.0f; break;
				case 6: d2 = 50.0f; break;
				case 7: d2 = 100.0f; break;
				case 8: d2 = 200.0f; break;
				case 9: d2 = 300.0f; break;
				case 10: d2 = 400.0f; break;
				case 11: d2 = 500.0f; break;
				case 12: d2 = 700.0f; break;
				case 13: d2 = 900.0f; break;
				default: d2 = 0.0f; break;
				}
				d3 = d1 * (d2 / 100.0f);

				add_price2 = (int)(d1 + d3);
			}

			price = price + (add_price1 - (add_price1 / 3)) + (add_price2 - (add_price2 / 3));

			if (neutral) price = price / 2;
			if (price <= 0) price = 1;
			if (price > 1000000) price = 1000000;

			if (m_game->m_client_list[client_h]->m_cur_weight_load + get_item_weight(m_pGold, price) > m_game->calc_max_load(client_h)) {
				m_game->send_notify_msg(0, client_h, Notify::CannotSellItem, item_id, 4, 0, m_game->m_client_list[client_h]->m_item_list[item_id]->m_name);
			}
			else m_game->send_notify_msg(0, client_h, Notify::SellItemPrice, item_id, remain_life, price, m_game->m_client_list[client_h]->m_item_list[item_id]->m_name, num);
		}
		else {
			// Non-durability items: flat half-price
			price = m_game->m_client_list[client_h]->m_item_list[item_id]->m_sell_price / 2;
			price = price * num;

			if (neutral) price = price / 2;
			if (price <= 0) price = 1;
			if (price > 1000000) price = 1000000;

			if (m_game->m_client_list[client_h]->m_cur_weight_load + get_item_weight(m_pGold, price) > m_game->calc_max_load(client_h)) {
				m_game->send_notify_msg(0, client_h, Notify::CannotSellItem, item_id, 4, 0, m_game->m_client_list[client_h]->m_item_list[item_id]->m_name);
			}
			else m_game->send_notify_msg(0, client_h, Notify::SellItemPrice, item_id, 0, price, m_game->m_client_list[client_h]->m_item_list[item_id]->m_name, num);
		}
		break;

	default:
		break;
	}
	if (m_pGold != 0) delete m_pGold;
}

void ItemManager::req_sell_item_confirm_handler(int client_h, char item_id, int num, const char* string)
{
	CItem* item_gold;
	short remain_life;
	int   price;
	double d1, d2, d3;
	uint32_t mul1, mul2, swe_type, swe_value, add_price1, add_price2;
	int    erase_req, ret;
	bool   neutral;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;
	if ((item_id < 0) || (item_id >= 50)) return;
	if (m_game->m_client_list[client_h]->m_item_list[item_id] == 0) return;
	if (num <= 0) return;
	if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.count < static_cast<uint32_t>(num)) return;

	// Can't sell gold
	if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_id_num == hb::shared::item::ItemId::Gold) return;

	// New 18/05/2004
	if (m_game->m_client_list[client_h]->m_is_processing_allowed == false) return;

	if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_sell_price == 0) return;

	m_game->calc_total_weight(client_h);

	// v1.42
	neutral = false;
	if (memcmp(m_game->m_client_list[client_h]->m_location, "NONE", 4) == 0) neutral = true;

	price = 0;
	if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_durability > 0) {
		// Equipment with durability: price scaled by remaining durability + attribute bonuses
		remain_life = m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.cur_durability;

		if (remain_life <= 0) {
			return;
		}
		else {
			d1 = (double)remain_life;
			if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_durability != 0)
				d2 = (double)m_game->m_client_list[client_h]->m_item_list[item_id]->m_durability;
			else d2 = 1.0f;
			d3 = (d1 / d2) * 0.5f;
			d2 = (double)m_game->m_client_list[client_h]->m_item_list[item_id]->m_sell_price;
			d3 = d3 * d2;

			price = (short)d3;
			price = price * num;

			add_price1 = 0;
			add_price2 = 0;
			if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.prefix_type != static_cast<uint8_t>(hb::shared::item::AttributePrefixType::None)) {
				swe_type = static_cast<int>(m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.prefix_type);
				swe_value = m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.prefix_value;

				// 0-None 1- 2- 3- 4-
				// 5- 6- 7- 8- 9-
				switch (swe_type) {
				case 6: mul1 = 2; break;
				case 8: mul1 = 2; break;
				case 5: mul1 = 3; break;
				case 1: mul1 = 4; break;
				case 7: mul1 = 5; break;
				case 2: mul1 = 6; break;
				case 3: mul1 = 15; break;
				case 9: mul1 = 20; break;
				default: mul1 = 1; break;
				}

				d1 = (double)price * mul1;
				switch (swe_value) {
				case 1: d2 = 10.0f; break;
				case 2: d2 = 20.0f; break;
				case 3: d2 = 30.0f; break;
				case 4: d2 = 35.0f; break;
				case 5: d2 = 40.0f; break;
				case 6: d2 = 50.0f; break;
				case 7: d2 = 100.0f; break;
				case 8: d2 = 200.0f; break;
				case 9: d2 = 300.0f; break;
				case 10: d2 = 400.0f; break;
				case 11: d2 = 500.0f; break;
				case 12: d2 = 700.0f; break;
				case 13: d2 = 900.0f; break;
				default: d2 = 0.0f; break;
				}
				d3 = d1 * (d2 / 100.0f);
				add_price1 = (int)(d1 + d3);
			}

			if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.secondary_type != static_cast<uint8_t>(hb::shared::item::SecondaryEffectType::None)) {
				swe_type = static_cast<int>(m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.secondary_type);
				swe_value = m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.secondary_value;

				// (1),  (2),  (3), HP  (4), SP  (5)
				// MP  (6),  (7),   (8),   (9)
				// (10),   (11),  Gold(12)
				switch (swe_type) {
				case 1:
				case 12: mul2 = 2; break;

				case 2:
				case 3:
				case 4:
				case 5:
				case 6:
				case 7: mul2 = 4; break;

				case 8:
				case 9:
				case 10:
				case 11: mul2 = 6; break;
				}

				d1 = (double)price * mul2;
				switch (swe_value) {
				case 1: d2 = 10.0f; break;
				case 2: d2 = 20.0f; break;
				case 3: d2 = 30.0f; break;
				case 4: d2 = 35.0f; break;
				case 5: d2 = 40.0f; break;
				case 6: d2 = 50.0f; break;
				case 7: d2 = 100.0f; break;
				case 8: d2 = 200.0f; break;
				case 9: d2 = 300.0f; break;
				case 10: d2 = 400.0f; break;
				case 11: d2 = 500.0f; break;
				case 12: d2 = 700.0f; break;
				case 13: d2 = 900.0f; break;
				default: d2 = 0.0f; break;
				}
				d3 = d1 * (d2 / 100.0f);
				add_price2 = (int)(d1 + d3);
			}

			price = price + (add_price1 - (add_price1 / 3)) + (add_price2 - (add_price2 / 3));

			if (neutral) price = price / 2;
			if (price <= 0) price = 1;
			if (price > 1000000) price = 1000000; // New 06/05/2004

			m_game->send_notify_msg(0, client_h, Notify::ItemSold, item_id, 0, 0, 0);

			item_log(ItemLogAction::Sell, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_id]);

			if (m_game->m_client_list[client_h]->m_item_list[item_id]->is_stackable()) {
				// v1.41 !!!
				set_item_count(client_h, item_id, m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.count - num);
			}
			else item_deplete_handler(client_h, item_id, false);
		}
	}
	else {
		// Non-durability items: flat half-price
		price = m_game->m_client_list[client_h]->m_item_list[item_id]->m_sell_price / 2;
		price = price * num;

		if (neutral) price = price / 2;
		if (price <= 0) price = 1;
		if (price > 1000000) price = 1000000;

		m_game->send_notify_msg(0, client_h, Notify::ItemSold, item_id, 0, 0, 0);

		item_log(ItemLogAction::Sell, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_id]);

		if (m_game->m_client_list[client_h]->m_item_list[item_id]->is_stackable()) {
			set_item_count(client_h, item_id, m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.count - num);
		}
		else item_deplete_handler(client_h, item_id, false);
	}

	// Gold .    0     .
	if (price <= 0) return;

	item_gold = new CItem;
	init_item_attr(item_gold, hb::shared::item::ItemId::Gold);
	item_gold->m_instance.count = price;

	if (add_client_item_list(client_h, item_gold, &erase_req)) {

		ret = send_item_notify_msg(client_h, Notify::ItemObtained, item_gold, 0);

		m_game->calc_total_weight(client_h);

		switch (ret) {
		case sock::Event::QueueFull:
		case sock::Event::SocketError:
		case sock::Event::CriticalError:
		case sock::Event::SocketClosed:
			m_game->delete_client(client_h, true, true);
			break;
		}
	}
	else {
		m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(m_game->m_client_list[client_h]->m_x,
			m_game->m_client_list[client_h]->m_y, item_gold);

		m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index,
			m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, item_gold);

		m_game->calc_total_weight(client_h);

		ret = send_item_notify_msg(client_h, Notify::CannotCarryMoreItem, 0, 0);

		switch (ret) {
		case sock::Event::QueueFull:
		case sock::Event::SocketError:
		case sock::Event::CriticalError:
		case sock::Event::SocketClosed:
			m_game->delete_client(client_h, true, true);
			return;
		}
	}
}

void ItemManager::req_repair_item_handler(int client_h, char item_id, char repair_whom, const char* string)
{
	int32_t remain_life, price;
	double d1, d2, d3;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;
	if ((item_id < 0) || (item_id >= 50)) return;
	if (m_game->m_client_list[client_h]->m_item_list[item_id] == 0) return;

	if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_durability > 0) {

		if (repair_whom != 24) {
			m_game->send_notify_msg(0, client_h, Notify::CannotRepairItem, item_id, 2, 0, m_game->m_client_list[client_h]->m_item_list[item_id]->m_name);
			return;
		}

		remain_life = m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.cur_durability;
		if (remain_life == 0) {
			price = static_cast<short>(m_game->m_client_list[client_h]->m_item_list[item_id]->m_sell_price / 2);
		}
		else {
			d1 = (double)remain_life;
			if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_durability != 0)
				d2 = (double)m_game->m_client_list[client_h]->m_item_list[item_id]->m_durability;
			else d2 = 1.0f;
			d3 = (d1 / d2) * 0.5f;
			d2 = (double)m_game->m_client_list[client_h]->m_item_list[item_id]->m_sell_price;
			d3 = d3 * d2;

			price = static_cast<short>((m_game->m_client_list[client_h]->m_item_list[item_id]->m_sell_price / 2) - d3);
		}

		m_game->send_notify_msg(0, client_h, Notify::RepairItemPrice, item_id, remain_life, price, m_game->m_client_list[client_h]->m_item_list[item_id]->m_name);
	}
	else {
		m_game->send_notify_msg(0, client_h, Notify::CannotRepairItem, item_id, 1, 0, m_game->m_client_list[client_h]->m_item_list[item_id]->m_name);
	}
}

void ItemManager::req_repair_item_cofirm_handler(int client_h, char item_id, const char* string)
{
	int32_t  remain_life, price;
	double   d1, d2, d3;
	uint64_t gold_count;
	int      ret, gold_weight;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;

	if ((item_id < 0) || (item_id >= 50)) return;
	if (m_game->m_client_list[client_h]->m_item_list[item_id] == 0) return;

	// New 18/05/2004
	if (m_game->m_client_list[client_h]->m_is_processing_allowed == false) return;

	if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_durability > 0) {

		remain_life = m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.cur_durability;
		if (remain_life == 0) {
			price = static_cast<short>(m_game->m_client_list[client_h]->m_item_list[item_id]->m_sell_price / 2);
		}
		else {
			d1 = (double)abs(remain_life);
			if (m_game->m_client_list[client_h]->m_item_list[item_id]->m_durability != 0)
				d2 = (double)abs(m_game->m_client_list[client_h]->m_item_list[item_id]->m_durability);
			else d2 = 1.0f;
			d3 = (d1 / d2) * 0.5f;
			d2 = (double)m_game->m_client_list[client_h]->m_item_list[item_id]->m_sell_price;
			d3 = d3 * d2;

			price = static_cast<short>((m_game->m_client_list[client_h]->m_item_list[item_id]->m_sell_price / 2) - d3);
		}

		// price         .
		gold_count = get_item_count_by_id(client_h, hb::shared::item::ItemId::Gold);

		if (gold_count < static_cast<uint64_t>(price)) {
			// Gold     .   .
			{
				hb::net::PacketNotifyNotEnoughGold pkt{};
				pkt.header.msg_id = MsgId::Notify;
				pkt.header.msg_type = Notify::NotEnoughGold;
				pkt.item_index = static_cast<int8_t>(item_id);
				ret = m_game->m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
			}
			switch (ret) {
			case sock::Event::QueueFull:
			case sock::Event::SocketError:
			case sock::Event::CriticalError:
			case sock::Event::SocketClosed:
				m_game->delete_client(client_h, true, true);
				return;
			}
			return;
		}
		else {

			// . !BUG POINT  .      .
			m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.cur_durability = m_game->m_client_list[client_h]->m_item_list[item_id]->m_durability;
			m_game->send_notify_msg(0, client_h, Notify::ItemRepaired, item_id, m_game->m_client_list[client_h]->m_item_list[item_id]->m_instance.cur_durability, 0, 0);

			gold_weight = set_item_count_by_id(client_h, hb::shared::item::ItemId::Gold, gold_count - price);

			m_game->calc_total_weight(client_h);

			m_game->m_city_status[m_game->m_client_list[client_h]->m_side].funds += price;
		}
	}
	else {
	}
}

void ItemManager::calc_total_item_effect(int client_h, int equip_item_id, bool notify)
{
	short item_index;
	int arrow_index, prev_sa_type, temp;
	EquipPos equip_pos;
	double v1, v2, v3;
	uint32_t  swe_type, swe_value;

	if (m_game->m_client_list[client_h] == 0) return;

	if ((m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)] != -1) &&
		(m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)] != -1)) {

		if (m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)]] != 0) {
			m_game->m_client_list[client_h]->m_is_item_equipped[m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)]] = false;
			m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)] = -1;
		}
	}

	m_game->m_client_list[client_h]->m_angelic_str = 0; // By Snoopy81
	m_game->m_client_list[client_h]->m_angelic_int = 0; // By Snoopy81
	m_game->m_client_list[client_h]->m_angelic_dex = 0; // By Snoopy81
	m_game->m_client_list[client_h]->m_angelic_mag = 0; // By Snoopy81	
	m_game->m_status_effect_manager->set_angel_flag(client_h, hb::shared::owner_class::Player, 0, 0);

	m_game->m_client_list[client_h]->m_attack_dice_throw_sm = 0;
	m_game->m_client_list[client_h]->m_attack_dice_range_sm = 0;
	m_game->m_client_list[client_h]->m_attack_bonus_sm = 0;

	m_game->m_client_list[client_h]->m_attack_dice_throw_l = 0;
	m_game->m_client_list[client_h]->m_attack_dice_range_l = 0;
	m_game->m_client_list[client_h]->m_attack_bonus_l = 0;

	m_game->m_client_list[client_h]->m_hit_ratio = 0;
	m_game->m_client_list[client_h]->m_defense_ratio = m_game->m_client_list[client_h]->m_dex * 2;
	m_game->m_client_list[client_h]->m_damage_absorption_shield = 0;

	for(int i = 0; i < DEF_MAXITEMEQUIPPOS; i++) {
		m_game->m_client_list[client_h]->m_damage_absorption_armor[i] = 0;
	}

	m_game->m_client_list[client_h]->m_mana_save_ratio = 0;
	m_game->m_client_list[client_h]->m_add_resist_magic = 0;

	m_game->m_client_list[client_h]->m_add_physical_damage = 0;
	m_game->m_client_list[client_h]->m_add_magical_damage = 0;

	m_game->m_client_list[client_h]->m_is_lucky_effect = false;
	m_game->m_client_list[client_h]->m_magic_damage_save_item_index = -1;
	m_game->m_client_list[client_h]->m_side_effect_max_hp_down = 0;

	m_game->m_client_list[client_h]->m_add_abs_air = 0;
	m_game->m_client_list[client_h]->m_add_abs_earth = 0;
	m_game->m_client_list[client_h]->m_add_abs_fire = 0;
	m_game->m_client_list[client_h]->m_add_abs_water = 0;

	m_game->m_client_list[client_h]->m_custom_item_value_attack = 0;
	m_game->m_client_list[client_h]->m_custom_item_value_defense = 0;

	m_game->m_client_list[client_h]->m_min_attack_power_sm = 0;
	m_game->m_client_list[client_h]->m_min_attack_power_l = 0;

	m_game->m_client_list[client_h]->m_max_attack_power_sm = 0;
	m_game->m_client_list[client_h]->m_max_attack_power_l = 0;

	m_game->m_client_list[client_h]->m_special_weapon_effect_type = 0;	// : 0-None 1- 2- 3- 4-
	m_game->m_client_list[client_h]->m_special_weapon_effect_value = 0;

	m_game->m_client_list[client_h]->m_add_hp = m_game->m_client_list[client_h]->m_add_sp = m_game->m_client_list[client_h]->m_add_mp = 0;
	m_game->m_client_list[client_h]->m_add_attack_ratio = m_game->m_client_list[client_h]->m_add_poison_resistance = m_game->m_client_list[client_h]->m_add_defense_ratio = 0;
	m_game->m_client_list[client_h]->m_add_magic_resistance = m_game->m_client_list[client_h]->m_add_abs_physical_defense = m_game->m_client_list[client_h]->m_add_abs_magical_defense = 0;
	m_game->m_client_list[client_h]->m_add_combo_damage = m_game->m_client_list[client_h]->m_add_exp = m_game->m_client_list[client_h]->m_add_gold = 0;

	prev_sa_type = m_game->m_client_list[client_h]->m_special_ability_type;

	m_game->m_client_list[client_h]->m_special_ability_type = 0;
	m_game->m_client_list[client_h]->m_special_ability_last_sec = 0;
	m_game->m_client_list[client_h]->m_special_ability_equip_pos = 0;

	m_game->m_client_list[client_h]->m_add_trans_mana = 0;
	m_game->m_client_list[client_h]->m_add_charge_critical = 0;

	m_game->m_client_list[client_h]->m_alter_item_drop_index = -1;
	for (item_index = 0; item_index < hb::shared::limits::MaxItems; item_index++)
	{
		if (m_game->m_client_list[client_h]->m_item_list[item_index] != 0) {
			switch (m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type()) {
			case ItemEffectType::AlterItemDrop:
				if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.cur_durability > 0) {
					m_game->m_client_list[client_h]->m_alter_item_drop_index = item_index;
				}
				break;
			}
		}
	}

	for (item_index = 0; item_index < hb::shared::limits::MaxItems; item_index++)
	{
		if ((m_game->m_client_list[client_h]->m_item_list[item_index] != 0) &&
			(m_game->m_client_list[client_h]->m_is_item_equipped[item_index])) {

			equip_pos = m_game->m_client_list[client_h]->m_item_list[item_index]->get_equip_pos();

			switch (m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type()) {

			case ItemEffectType::MagicDamageSave:
				m_game->m_client_list[client_h]->m_magic_damage_save_item_index = item_index;
				break;

			case ItemEffectType::AttackSpecAbility:
			case ItemEffectType::AttackDefense:
			case ItemEffectType::AttackManaSave:
			case ItemEffectType::AttackMaxHPDown:
			case ItemEffectType::Attack:
				m_game->m_client_list[client_h]->m_attack_dice_throw_sm = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1;
				m_game->m_client_list[client_h]->m_attack_dice_range_sm = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
				m_game->m_client_list[client_h]->m_attack_bonus_sm = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value3;
				m_game->m_client_list[client_h]->m_attack_dice_throw_l = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value4;
				m_game->m_client_list[client_h]->m_attack_dice_range_l = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value5;
				m_game->m_client_list[client_h]->m_attack_bonus_l = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value6;

				temp = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus;
				//testcode
				//std::snprintf(G_cTxt, sizeof(G_cTxt), "Add Damage: %d", temp);
				//PutLogList(G_cTxt);

				m_game->m_client_list[client_h]->m_add_physical_damage += temp;
				m_game->m_client_list[client_h]->m_add_magical_damage += temp;

				m_game->m_client_list[client_h]->m_hit_ratio += m_game->m_client_list[client_h]->m_skill_mastery[m_game->m_client_list[client_h]->m_item_list[item_index]->m_related_skill];

				//m_game->m_client_list[client_h]->m_iHitRatio_ItemEffect_SM += m_game->m_client_list[client_h]->m_item_list[item_index]->m_sSM_HitRatio;
				//m_game->m_client_list[client_h]->m_iHitRatio_ItemEffect_L  += m_game->m_client_list[client_h]->m_item_list[item_index]->m_sL_HitRatio;
				m_game->m_client_list[client_h]->m_using_weapon_skill = m_game->m_client_list[client_h]->m_item_list[item_index]->m_related_skill;

				if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.custom_made) {
					m_game->m_client_list[client_h]->m_custom_item_value_attack += m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2;
					if (m_game->m_client_list[client_h]->m_custom_item_value_attack > 100)
						m_game->m_client_list[client_h]->m_custom_item_value_attack = 100;

					if (m_game->m_client_list[client_h]->m_custom_item_value_attack < -100)
						m_game->m_client_list[client_h]->m_custom_item_value_attack = -100;

					if (m_game->m_client_list[client_h]->m_custom_item_value_attack > 0) {
						v2 = (double)m_game->m_client_list[client_h]->m_custom_item_value_attack;
						v1 = (v2 / 100.0f) * (5.0f);
						m_game->m_client_list[client_h]->m_min_attack_power_sm = m_game->m_client_list[client_h]->m_attack_dice_throw_sm +
							m_game->m_client_list[client_h]->m_attack_bonus_sm + (int)v1;

						m_game->m_client_list[client_h]->m_min_attack_power_l = m_game->m_client_list[client_h]->m_attack_dice_throw_l +
							m_game->m_client_list[client_h]->m_attack_bonus_l + (int)v1;

						if (m_game->m_client_list[client_h]->m_min_attack_power_sm < 1) m_game->m_client_list[client_h]->m_min_attack_power_sm = 1;
						if (m_game->m_client_list[client_h]->m_min_attack_power_l < 1)  m_game->m_client_list[client_h]->m_min_attack_power_l = 1;

						if (m_game->m_client_list[client_h]->m_min_attack_power_sm > (m_game->m_client_list[client_h]->m_attack_dice_throw_sm * m_game->m_client_list[client_h]->m_attack_dice_range_sm + m_game->m_client_list[client_h]->m_attack_bonus_sm))
							m_game->m_client_list[client_h]->m_min_attack_power_sm = (m_game->m_client_list[client_h]->m_attack_dice_throw_sm * m_game->m_client_list[client_h]->m_attack_dice_range_sm + m_game->m_client_list[client_h]->m_attack_bonus_sm);

						if (m_game->m_client_list[client_h]->m_min_attack_power_l > (m_game->m_client_list[client_h]->m_attack_dice_throw_l * m_game->m_client_list[client_h]->m_attack_dice_range_l + m_game->m_client_list[client_h]->m_attack_bonus_l))
							m_game->m_client_list[client_h]->m_min_attack_power_l = (m_game->m_client_list[client_h]->m_attack_dice_throw_l * m_game->m_client_list[client_h]->m_attack_dice_range_l + m_game->m_client_list[client_h]->m_attack_bonus_l);

						//testcode
						//std::snprintf(G_cTxt, sizeof(G_cTxt), "MinAP: %d %d +(%d)", m_game->m_client_list[client_h]->m_min_attack_power_sm, m_game->m_client_list[client_h]->m_min_attack_power_l, (int)v1);
						//PutLogList(G_cTxt);
					}
					else if (m_game->m_client_list[client_h]->m_custom_item_value_attack < 0) {
						v2 = (double)m_game->m_client_list[client_h]->m_custom_item_value_attack;
						v1 = (v2 / 100.0f) * (5.0f);
						m_game->m_client_list[client_h]->m_max_attack_power_sm = m_game->m_client_list[client_h]->m_attack_dice_throw_sm * m_game->m_client_list[client_h]->m_attack_dice_range_sm
							+ m_game->m_client_list[client_h]->m_attack_bonus_sm + (int)v1;

						m_game->m_client_list[client_h]->m_max_attack_power_l = m_game->m_client_list[client_h]->m_attack_dice_throw_l * m_game->m_client_list[client_h]->m_attack_dice_range_l
							+ m_game->m_client_list[client_h]->m_attack_bonus_l + (int)v1;

						if (m_game->m_client_list[client_h]->m_max_attack_power_sm < 1) m_game->m_client_list[client_h]->m_max_attack_power_sm = 1;
						if (m_game->m_client_list[client_h]->m_max_attack_power_l < 1)  m_game->m_client_list[client_h]->m_max_attack_power_l = 1;

						if (m_game->m_client_list[client_h]->m_max_attack_power_sm < (m_game->m_client_list[client_h]->m_attack_dice_throw_sm * m_game->m_client_list[client_h]->m_attack_dice_range_sm + m_game->m_client_list[client_h]->m_attack_bonus_sm))
							m_game->m_client_list[client_h]->m_max_attack_power_sm = (m_game->m_client_list[client_h]->m_attack_dice_throw_sm * m_game->m_client_list[client_h]->m_attack_dice_range_sm + m_game->m_client_list[client_h]->m_attack_bonus_sm);

						if (m_game->m_client_list[client_h]->m_max_attack_power_l < (m_game->m_client_list[client_h]->m_attack_dice_throw_l * m_game->m_client_list[client_h]->m_attack_dice_range_l + m_game->m_client_list[client_h]->m_attack_bonus_l))
							m_game->m_client_list[client_h]->m_max_attack_power_l = (m_game->m_client_list[client_h]->m_attack_dice_throw_l * m_game->m_client_list[client_h]->m_attack_dice_range_l + m_game->m_client_list[client_h]->m_attack_bonus_l);

						//testcode
						//std::snprintf(G_cTxt, sizeof(G_cTxt), "MaxAP: %d %d +(%d)", m_game->m_client_list[client_h]->m_max_attack_power_sm, m_game->m_client_list[client_h]->m_max_attack_power_l, (int)v1);
						//PutLogList(G_cTxt);
					}
				}

				if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.prefix_type != static_cast<uint8_t>(hb::shared::item::AttributePrefixType::None)) {
					swe_type = static_cast<int>(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.prefix_type);
					swe_value = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.prefix_value;

					// 0-None 1- 2- 3- 4-
					// 5- 6- 7- 8- 9- 10-
					m_game->m_client_list[client_h]->m_special_weapon_effect_type = (int)swe_type;
					m_game->m_client_list[client_h]->m_special_weapon_effect_value = (int)swe_value;

					switch (swe_type) {
					case 7:
						m_game->m_client_list[client_h]->m_attack_bonus_sm += (int)swe_value * m_game->m_prefix_multiplier[7];
						m_game->m_client_list[client_h]->m_attack_bonus_l += (int)swe_value * m_game->m_prefix_multiplier[7];
						break;

					case 9:
						m_game->m_client_list[client_h]->m_attack_bonus_sm += (int)swe_value * m_game->m_prefix_multiplier[9];
						m_game->m_client_list[client_h]->m_attack_bonus_l += (int)swe_value * m_game->m_prefix_multiplier[9];
						break;
					}
				}

				if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.secondary_type != static_cast<uint8_t>(hb::shared::item::SecondaryEffectType::None)) {
					swe_type = static_cast<int>(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.secondary_type);
					swe_value = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.secondary_value;

					// (1),  (2),  (3), HP  (4), SP  (5)
					// MP  (6),  (7),   (8),   (9)
					// (10),   (11),  Gold(12)

					switch (swe_type) {
					case 0:  break;
					case 1:  m_game->m_client_list[client_h]->m_add_poison_resistance += (int)swe_value * m_game->m_secondary_multiplier[1]; break;
					case 2:  m_game->m_client_list[client_h]->m_add_attack_ratio += (int)swe_value * m_game->m_secondary_multiplier[2]; break;
					case 3:  m_game->m_client_list[client_h]->m_add_defense_ratio += (int)swe_value * m_game->m_secondary_multiplier[3]; break;
					case 4:  m_game->m_client_list[client_h]->m_add_hp += (int)swe_value * m_game->m_secondary_multiplier[4]; break;
					case 5:  m_game->m_client_list[client_h]->m_add_sp += (int)swe_value * m_game->m_secondary_multiplier[5]; break;
					case 6:  m_game->m_client_list[client_h]->m_add_mp += (int)swe_value * m_game->m_secondary_multiplier[6]; break;
					case 7:  m_game->m_client_list[client_h]->m_add_magic_resistance += (int)swe_value * m_game->m_secondary_multiplier[7]; break;
					case 8:  m_game->m_client_list[client_h]->m_damage_absorption_armor[m_game->m_client_list[client_h]->m_item_list[item_index]->m_equip_pos] += (int)swe_value * m_game->m_secondary_multiplier[8]; break;
					case 9:  m_game->m_client_list[client_h]->m_add_abs_magical_defense += (int)swe_value * m_game->m_secondary_multiplier[9]; break;
					case 10: m_game->m_client_list[client_h]->m_add_combo_damage += (int)swe_value * m_game->m_secondary_multiplier[10]; break;
					case 11: m_game->m_client_list[client_h]->m_add_exp += (int)swe_value * m_game->m_secondary_multiplier[11]; break;
					case 12: m_game->m_client_list[client_h]->m_add_gold += (int)swe_value * m_game->m_secondary_multiplier[12]; break;
					}

					switch (swe_type) {
					case 9: if (m_game->m_client_list[client_h]->m_add_abs_magical_defense > 80) m_game->m_client_list[client_h]->m_add_abs_magical_defense = 80; break;
					}
				}

				switch (m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type()) {
				case ItemEffectType::AttackMaxHPDown:
					m_game->m_client_list[client_h]->m_side_effect_max_hp_down = m_game->m_client_list[client_h]->m_item_list[item_index]->m_special_effect;
					break;

				case ItemEffectType::AttackManaSave:
					// :    80%
					m_game->m_client_list[client_h]->m_mana_save_ratio += m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value4;
					if (m_game->m_client_list[client_h]->m_mana_save_ratio > 80) m_game->m_client_list[client_h]->m_mana_save_ratio = 80;
					break;

				case ItemEffectType::AttackDefense:
					m_game->m_client_list[client_h]->m_damage_absorption_armor[to_int(EquipPos::Body)] += m_game->m_client_list[client_h]->m_item_list[item_index]->m_special_effect;
					break;

				case ItemEffectType::AttackSpecAbility:
					m_game->m_client_list[client_h]->m_special_ability_type = m_game->m_client_list[client_h]->m_item_list[item_index]->m_special_effect;
					m_game->m_client_list[client_h]->m_special_ability_last_sec = m_game->m_client_list[client_h]->m_item_list[item_index]->m_special_effect_value1;
					m_game->m_client_list[client_h]->m_special_ability_equip_pos = to_int(equip_pos);

					if ((notify) && (equip_item_id == (int)item_index))
						m_game->send_notify_msg(0, client_h, Notify::SpecialAbilityStatus, 2, m_game->m_client_list[client_h]->m_special_ability_type, m_game->m_client_list[client_h]->m_special_ability_time, 0);
					break;
				}
				break;

			case ItemEffectType::add_effect:
				switch (m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1) {
				case 1:
					m_game->m_client_list[client_h]->m_add_resist_magic += m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
					break;

				case 2:
					m_game->m_client_list[client_h]->m_mana_save_ratio += m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
					if (m_game->m_client_list[client_h]->m_mana_save_ratio > 80) m_game->m_client_list[client_h]->m_mana_save_ratio = 80;
					break;

				case 3:
					m_game->m_client_list[client_h]->m_add_physical_damage += m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
					break;

				case 4:
					m_game->m_client_list[client_h]->m_defense_ratio += m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
					break;

				case 5:
					if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2 != 0)
						m_game->m_client_list[client_h]->m_is_lucky_effect = true;
					else m_game->m_client_list[client_h]->m_is_lucky_effect = false;
					break;

				case 6:
					m_game->m_client_list[client_h]->m_add_magical_damage += m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
					break;

				case 7:
					m_game->m_client_list[client_h]->m_add_abs_air += m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
					break;

				case 8:
					m_game->m_client_list[client_h]->m_add_abs_earth += m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
					break;

				case 9:
					m_game->m_client_list[client_h]->m_add_abs_fire += m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
					break;

				case 10:
					// . (2  )
					m_game->m_client_list[client_h]->m_add_abs_water += m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
					break;

				case 11:
					m_game->m_client_list[client_h]->m_add_poison_resistance += m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
					break;

				case 12:
					m_game->m_client_list[client_h]->m_hit_ratio += m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
					break;

				case 13: // Magin Ruby		Characters Hp recovery rate(% applied) added by the purity formula.
					m_game->m_client_list[client_h]->m_add_hp += (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2 / 5);
					break;

				case 14: // Magin Diamond	Attack probability(physical&magic) added by the purity formula.
					m_game->m_client_list[client_h]->m_add_attack_ratio += (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2 / 5);
					break;

				case 15: // Magin Emerald	Magical damage decreased(% applied) by the purity formula.	
					m_game->m_client_list[client_h]->m_add_abs_magical_defense += (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2 / 10);
					if (m_game->m_client_list[client_h]->m_add_abs_magical_defense > 80) m_game->m_client_list[client_h]->m_add_abs_magical_defense = 80;
					break;

				case 30: // Magin Sapphire	Phisical damage decreased(% applied) by the purity formula.	
					temp = (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2 / 10);
					m_game->m_client_list[client_h]->m_damage_absorption_armor[to_int(EquipPos::Head)] += temp;
					m_game->m_client_list[client_h]->m_damage_absorption_armor[to_int(EquipPos::Body)] += temp;
					m_game->m_client_list[client_h]->m_damage_absorption_armor[to_int(EquipPos::Arms)] += temp;
					m_game->m_client_list[client_h]->m_damage_absorption_armor[to_int(EquipPos::Leggings)] += temp;
					break;

					/*Functions rates confirm.
					Magic Diamond: Completion rate / 5 = Functions rate. ? Maximum 20. (not%)
					Magic Ruby: Completion rate / 5 = Functions rate.(%) ? Maximum 20%.
					Magic Emerald: Completion rate / 10 = Functions rate.(%) ? Maximum 10%.
					Magic Sapphire: Completion rate / 10 = Functions rate.(%) ? Maximum 10%.*/

					// ******* Angel Code - Begin ******* //			
				case 16: // Angel STR//AngelicPendant(STR)
					temp = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus;
					m_game->m_client_list[client_h]->m_angelic_str = temp + 1;
					m_game->m_status_effect_manager->set_angel_flag(client_h, hb::shared::owner_class::Player, 1, temp);
					m_game->send_notify_msg(0, client_h, Notify::SettingSuccess, 0, 0, 0, 0);
					break;
				case 17: // Angel DEX //AngelicPendant(DEX)
					temp = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus;
					m_game->m_client_list[client_h]->m_angelic_dex = temp + 1;
					m_game->m_status_effect_manager->set_angel_flag(client_h, hb::shared::owner_class::Player, 2, temp);
					m_game->send_notify_msg(0, client_h, Notify::SettingSuccess, 0, 0, 0, 0);
					break;
				case 18: // Angel INT//AngelicPendant(INT)
					temp = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus;
					m_game->m_client_list[client_h]->m_angelic_int = temp + 1;
					m_game->m_status_effect_manager->set_angel_flag(client_h, hb::shared::owner_class::Player, 3, temp);
					m_game->send_notify_msg(0, client_h, Notify::SettingSuccess, 0, 0, 0, 0);
					break;
				case 19: // Angel MAG//AngelicPendant(MAG)
					temp = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus;
					m_game->m_client_list[client_h]->m_angelic_mag = temp + 1;
					m_game->m_status_effect_manager->set_angel_flag(client_h, hb::shared::owner_class::Player, 4, temp);
					m_game->send_notify_msg(0, client_h, Notify::SettingSuccess, 0, 0, 0, 0);
					break;

				}
				break;

			case ItemEffectType::AttackArrow:
				if ((m_game->m_client_list[client_h]->m_arrow_index != -1) &&
					(m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_arrow_index] == 0)) {
					// ArrowIndex  . ( )
					m_game->m_client_list[client_h]->m_arrow_index = get_arrow_item_index(client_h);
				}
				else if (m_game->m_client_list[client_h]->m_arrow_index == -1)
					m_game->m_client_list[client_h]->m_arrow_index = get_arrow_item_index(client_h);

				if (m_game->m_client_list[client_h]->m_arrow_index == -1) {
					m_game->m_client_list[client_h]->m_attack_dice_throw_sm = 0;
					m_game->m_client_list[client_h]->m_attack_dice_range_sm = 0;
					m_game->m_client_list[client_h]->m_attack_bonus_sm = 0;
					m_game->m_client_list[client_h]->m_attack_dice_throw_l = 0;
					m_game->m_client_list[client_h]->m_attack_dice_range_l = 0;
					m_game->m_client_list[client_h]->m_attack_bonus_l = 0;
				}
				else {
					arrow_index = m_game->m_client_list[client_h]->m_arrow_index;
					m_game->m_client_list[client_h]->m_attack_dice_throw_sm = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1;
					m_game->m_client_list[client_h]->m_attack_dice_range_sm = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2;
					m_game->m_client_list[client_h]->m_attack_bonus_sm = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value3;
					m_game->m_client_list[client_h]->m_attack_dice_throw_l = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value4;
					m_game->m_client_list[client_h]->m_attack_dice_range_l = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value5;
					m_game->m_client_list[client_h]->m_attack_bonus_l = m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value6;
				}

				m_game->m_client_list[client_h]->m_hit_ratio += m_game->m_client_list[client_h]->m_skill_mastery[m_game->m_client_list[client_h]->m_item_list[item_index]->m_related_skill];
				break;

			case ItemEffectType::DefenseSpecAbility:
			case ItemEffectType::Defense:
				m_game->m_client_list[client_h]->m_defense_ratio += m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1;

				if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.custom_made) {
					m_game->m_client_list[client_h]->m_custom_item_value_defense += m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2;

					v2 = (double)m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2;
					v3 = (double)m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1;
					v1 = (double)(v2 / 100.0f) * v3;

					v1 = v1 / 2.0f;
					m_game->m_client_list[client_h]->m_defense_ratio += (int)v1;
					if (m_game->m_client_list[client_h]->m_defense_ratio <= 0) m_game->m_client_list[client_h]->m_defense_ratio = 1;

					//testcode
					//std::snprintf(G_cTxt, sizeof(G_cTxt), "Custom-Defense: %d", (int)v1);
					//PutLogList(G_cTxt);
				}

				if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.prefix_type != static_cast<uint8_t>(hb::shared::item::AttributePrefixType::None)) {
					swe_type = static_cast<int>(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.prefix_type);
					swe_value = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.prefix_value;

					// 0-None 1- 2- 3- 4-
					// 5- 6- 7- 8- 9- 10- 11- 12-

					switch (swe_type) {
					case 7:
						m_game->m_client_list[client_h]->m_attack_bonus_sm += (int)swe_value * m_game->m_prefix_multiplier[7];
						m_game->m_client_list[client_h]->m_attack_bonus_l += (int)swe_value * m_game->m_prefix_multiplier[7];
						break;

					case 9:
						m_game->m_client_list[client_h]->m_attack_bonus_sm += (int)swe_value * m_game->m_prefix_multiplier[9];
						m_game->m_client_list[client_h]->m_attack_bonus_l += (int)swe_value * m_game->m_prefix_multiplier[9];
						break;

					case 11:
						m_game->m_client_list[client_h]->m_add_trans_mana += (int)swe_value * m_game->m_prefix_multiplier[11];
						if (m_game->m_client_list[client_h]->m_add_trans_mana > 13) m_game->m_client_list[client_h]->m_add_trans_mana = 13;
						break;

					case 12:
						m_game->m_client_list[client_h]->m_add_charge_critical += (int)swe_value * m_game->m_prefix_multiplier[12];
						if (m_game->m_client_list[client_h]->m_add_charge_critical > 20) m_game->m_client_list[client_h]->m_add_charge_critical = 20;
						break;
					}
				}

				if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.secondary_type != static_cast<uint8_t>(hb::shared::item::SecondaryEffectType::None)) {
					swe_type = static_cast<int>(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.secondary_type);
					swe_value = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.secondary_value;

					// (1),  (2),  (3), HP  (4), SP  (5)
					// MP  (6),  (7),   (8),   (9)
					// (10),   (11),  Gold(12)

					switch (swe_type) {
					case 0:  break;
					case 1:  m_game->m_client_list[client_h]->m_add_poison_resistance += (int)swe_value * m_game->m_secondary_multiplier[1]; break;
					case 2:  m_game->m_client_list[client_h]->m_add_attack_ratio += (int)swe_value * m_game->m_secondary_multiplier[2]; break;
					case 3:  m_game->m_client_list[client_h]->m_add_defense_ratio += (int)swe_value * m_game->m_secondary_multiplier[3]; break;
					case 4:  m_game->m_client_list[client_h]->m_add_hp += (int)swe_value * m_game->m_secondary_multiplier[4]; break;
					case 5:  m_game->m_client_list[client_h]->m_add_sp += (int)swe_value * m_game->m_secondary_multiplier[5]; break;
					case 6:  m_game->m_client_list[client_h]->m_add_mp += (int)swe_value * m_game->m_secondary_multiplier[6]; break;
					case 7:  m_game->m_client_list[client_h]->m_add_magic_resistance += (int)swe_value * m_game->m_secondary_multiplier[7]; break;
					case 8:  m_game->m_client_list[client_h]->m_damage_absorption_armor[m_game->m_client_list[client_h]->m_item_list[item_index]->m_equip_pos] += (int)swe_value * m_game->m_secondary_multiplier[8]; break;
					case 9:  m_game->m_client_list[client_h]->m_add_abs_magical_defense += (int)swe_value * m_game->m_secondary_multiplier[9]; break;
					case 10: m_game->m_client_list[client_h]->m_add_combo_damage += (int)swe_value * m_game->m_secondary_multiplier[10]; break;
					case 11: m_game->m_client_list[client_h]->m_add_exp += (int)swe_value * m_game->m_secondary_multiplier[11]; break;
					case 12: m_game->m_client_list[client_h]->m_add_gold += (int)swe_value * m_game->m_secondary_multiplier[12]; break;
					}

					switch (swe_type) {
					case 9: if (m_game->m_client_list[client_h]->m_add_abs_magical_defense > 80) m_game->m_client_list[client_h]->m_add_abs_magical_defense = 80; break;
					}
				}

				switch (equip_pos) {
				case EquipPos::LeftHand:
					// .  70%
					m_game->m_client_list[client_h]->m_damage_absorption_shield = (m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1) - (m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1) / 3;
					break;
				default:
					// .  70%  <- v1.43 100% . V2!
					m_game->m_client_list[client_h]->m_damage_absorption_armor[m_game->m_client_list[client_h]->m_item_list[item_index]->m_equip_pos] += (m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2);
					break;
				}

				switch (m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type()) {
				case ItemEffectType::DefenseSpecAbility:
					m_game->m_client_list[client_h]->m_special_ability_type = m_game->m_client_list[client_h]->m_item_list[item_index]->m_special_effect;
					m_game->m_client_list[client_h]->m_special_ability_last_sec = m_game->m_client_list[client_h]->m_item_list[item_index]->m_special_effect_value1;
					m_game->m_client_list[client_h]->m_special_ability_equip_pos = to_int(equip_pos);

					if ((notify) && (equip_item_id == (int)item_index))
						m_game->send_notify_msg(0, client_h, Notify::SpecialAbilityStatus, 2, m_game->m_client_list[client_h]->m_special_ability_type, m_game->m_client_list[client_h]->m_special_ability_time, 0);
					break;
				}
				break;
			}
		}
	}

	// Snoopy: Bonus for Angels	
	m_game->m_client_list[client_h]->m_defense_ratio += m_game->m_client_list[client_h]->m_angelic_dex * 2;
	if (m_game->m_client_list[client_h]->m_hp > m_game->get_max_hp(client_h)) m_game->m_client_list[client_h]->m_hp = m_game->get_max_hp(client_h);
	if (m_game->m_client_list[client_h]->m_mp > m_game->get_max_mp(client_h)) m_game->m_client_list[client_h]->m_mp = m_game->get_max_mp(client_h);
	if (m_game->m_client_list[client_h]->m_sp > m_game->get_max_sp(client_h)) m_game->m_client_list[client_h]->m_sp = m_game->get_max_sp(client_h);
	m_game->send_notify_msg(0, client_h, Notify::Sp, 0, 0, 0, 0);

	//v1.432
	if ((prev_sa_type != 0) && (m_game->m_client_list[client_h]->m_special_ability_type == 0) && (notify)) {
		m_game->send_notify_msg(0, client_h, Notify::SpecialAbilityStatus, 4, 0, 0, 0);
		if (m_game->m_client_list[client_h]->m_is_special_ability_enabled) {
			m_game->m_client_list[client_h]->m_is_special_ability_enabled = false;
			m_game->m_client_list[client_h]->m_special_ability_time = SpecialAbilityTimeSec;
			m_game->m_client_list[client_h]->m_appearance.effect_type = 0;
			m_game->send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		}
	}

	if ((prev_sa_type != 0) && (m_game->m_client_list[client_h]->m_special_ability_type != 0) &&
		(prev_sa_type != m_game->m_client_list[client_h]->m_special_ability_type) && (notify)) {
		if (m_game->m_client_list[client_h]->m_is_special_ability_enabled) {
			m_game->send_notify_msg(0, client_h , Notify::SpecialAbilityStatus, 3, 0, 0, 0);
			m_game->m_client_list[client_h]->m_is_special_ability_enabled = false;
			m_game->m_client_list[client_h]->m_special_ability_time = SpecialAbilityTimeSec;
			m_game->m_client_list[client_h]->m_appearance.effect_type = 0;
			m_game->send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		}
	}
}

bool ItemManager::deplete_dest_type_item_use_effect(int client_h, int dX, int dY, short item_index, short dest_item_id)
{
	int ret;

	if (m_game->m_client_list[client_h] == 0) return false;
	if ((item_index < 0) || (item_index >= hb::shared::limits::MaxItems)) return false;
	if (m_game->m_client_list[client_h]->m_item_list[item_index] == 0) return false;

	switch (m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type()) {
	case ItemEffectType::OccupyFlag:
		ret = m_game->m_war_manager->set_occupy_flag(m_game->m_client_list[client_h]->m_map_index, dX, dY,
			m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1,
			0, client_h);
		if (ret) {
			m_game->get_exp(client_h, (m_game->dice(m_game->m_client_list[client_h]->m_level, 10)));
		}
		else {
			m_game->send_notify_msg(0, client_h, Notify::NotFlagSpot, 0, 0, 0, 0);
		}
		return ret;

		// crusade
	case ItemEffectType::ConstructionKit:
		// .   . m_item_effect_value1:  , m_item_effect_value2:
		ret = m_game->m_war_manager->set_construction_kit(m_game->m_client_list[client_h]->m_map_index, dX, dY,
			m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1,
			m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2,
			client_h);
		if (ret) {
		}
		else {
		}
		return ret;

	case ItemEffectType::Dye:
		if ((dest_item_id >= 0) && (dest_item_id < hb::shared::limits::MaxItems)) {
			if (m_game->m_client_list[client_h]->m_item_list[dest_item_id] != 0) {
				auto* dest = m_game->m_client_list[client_h]->m_item_list[dest_item_id];
				bool is_dye_removal = (m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1 == 0);
				if (dest->m_is_dyeable != 0
					&& (is_dye_removal || dest->m_armor_class == armor_class::clothing))
				{
					dest->m_instance.item_color =
						static_cast<char>(m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1);
					m_game->send_notify_msg(0, client_h, Notify::ItemColorChange, dest_item_id, dest->m_instance.item_color, 0, 0);
					return true;
				}
				else {
					m_game->send_notify_msg(0, client_h, Notify::ItemColorChange, dest_item_id, -1, 0, 0);
					return false;
				}
			}
		}
		break;

	case ItemEffectType::ArmorDye:
		if ((dest_item_id >= 0) && (dest_item_id < hb::shared::limits::MaxItems)) {
			if (m_game->m_client_list[client_h]->m_item_list[dest_item_id] != 0) {
				auto* dest = m_game->m_client_list[client_h]->m_item_list[dest_item_id];
				if (dest->m_is_dyeable != 0 && dest->m_armor_class == armor_class::armor) {
					dest->m_instance.item_color =
						static_cast<char>(m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1);
					m_game->send_notify_msg(0, client_h, Notify::ItemColorChange, dest_item_id, dest->m_instance.item_color, 0, 0);
					return true;
				}
				else {
					m_game->send_notify_msg(0, client_h, Notify::ItemColorChange, dest_item_id, -1, 0, 0);
					return false;
				}
			}
		}
		break;

	case ItemEffectType::WeaponDye:
		if ((dest_item_id >= 0) && (dest_item_id < hb::shared::limits::MaxItems)) {
			if (m_game->m_client_list[client_h]->m_item_list[dest_item_id] != 0) {
				auto* dest = m_game->m_client_list[client_h]->m_item_list[dest_item_id];
				if (dest->m_is_dyeable != 0 && is_weapon_slot(dest->get_equip_pos())) {
					dest->m_instance.item_color =
						static_cast<char>(m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1);
					m_game->send_notify_msg(0, client_h, Notify::ItemColorChange, dest_item_id, dest->m_instance.item_color, 0, 0);
					return true;
				}
				else {
					m_game->send_notify_msg(0, client_h, Notify::ItemColorChange, dest_item_id, -1, 0, 0);
					return false;
				}
			}
		}
		break;

	case ItemEffectType::Farming:
		ret = plant_seed_bag(m_game->m_client_list[client_h]->m_map_index, dX, dY,
			m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1,
			m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value2,
			client_h);
		return ret;

	default:
		break;
	}

	return true;
}

void ItemManager::get_hero_mantle_handler(int client_h, int item_id, const char* string)
{
	int   num, ret, erase_req;
	CItem* item;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_enemy_kill_count < 100) return;
	if (m_game->m_client_list[client_h]->m_side == 0) return;
	if (get_item_space_left(client_h) == 0) {
		send_item_notify_msg(client_h, Notify::CannotCarryMoreItem, 0, 0);
		return;
	}

	//Prevents a crash if item dosent exist
	if (m_game->m_item_config_list[item_id] == 0)  return;

	switch (item_id) {
		// Hero Cape
	case 400: //Aresden HeroCape
	case 401: //Elvine HeroCape
		if (m_game->m_client_list[client_h]->m_enemy_kill_count < 300) return;
		m_game->m_client_list[client_h]->m_enemy_kill_count -= 300;
		break;

		// Hero Helm
	case 403: //Aresden HeroHelm(M)
	case 404: //Aresden HeroHelm(W)
	case 405: //Elvine HeroHelm(M)
	case 406: //Elvine HeroHelm(W)
		if (m_game->m_client_list[client_h]->m_enemy_kill_count < 150) return;
		m_game->m_client_list[client_h]->m_enemy_kill_count -= 150;
		if (m_game->m_client_list[client_h]->m_contribution < 20) return;
		m_game->m_client_list[client_h]->m_contribution -= 20;
		break;

		// Hero Cap
	case 407: //Aresden HeroCap(M)
	case 408: //Aresden HeroCap(W)
	case 409: //Elvine HeroHelm(M)
	case 410: //Elvine HeroHelm(W)
		if (m_game->m_client_list[client_h]->m_enemy_kill_count < 100) return;
		m_game->m_client_list[client_h]->m_enemy_kill_count -= 100;
		if (m_game->m_client_list[client_h]->m_contribution < 20) return;
		m_game->m_client_list[client_h]->m_contribution -= 20;
		break;

		// Hero Armour
	case 411: //Aresden HeroArmour(M)
	case 412: //Aresden HeroArmour(W)
	case 413: //Elvine HeroArmour(M)
	case 414: //Elvine HeroArmour(W)
		if (m_game->m_client_list[client_h]->m_enemy_kill_count < 300) return;
		m_game->m_client_list[client_h]->m_enemy_kill_count -= 300;
		if (m_game->m_client_list[client_h]->m_contribution < 30) return;
		m_game->m_client_list[client_h]->m_contribution -= 30;
		break;

		// Hero Robe
	case 415: //Aresden HeroRobe(M)
	case 416: //Aresden HeroRobe(W)
	case 417: //Elvine HeroRobe(M)
	case 418: //Elvine HeroRobe(W)
		if (m_game->m_client_list[client_h]->m_enemy_kill_count < 200) return;
		m_game->m_client_list[client_h]->m_enemy_kill_count -= 200;
		if (m_game->m_client_list[client_h]->m_contribution < 20) return;
		m_game->m_client_list[client_h]->m_contribution -= 20;
		break;

		// Hero Hauberk
	case 419: //Aresden HeroHauberk(M)
	case 420: //Aresden HeroHauberk(W)
	case 421: //Elvine HeroHauberk(M)
	case 422: //Elvine HeroHauberk(W)
		if (m_game->m_client_list[client_h]->m_enemy_kill_count < 100) return;
		m_game->m_client_list[client_h]->m_enemy_kill_count -= 100;
		if (m_game->m_client_list[client_h]->m_contribution < 10) return;
		m_game->m_client_list[client_h]->m_contribution -= 10;
		break;

		// Hero Leggings
	case 423: //Aresden HeroLeggings(M)
	case 424: //Aresden HeroLeggings(W)
	case 425: //Elvine HeroLeggings(M)
	case 426: //Elvine HeroLeggings(W)
		if (m_game->m_client_list[client_h]->m_enemy_kill_count < 150) return;
		m_game->m_client_list[client_h]->m_enemy_kill_count -= 150;
		if (m_game->m_client_list[client_h]->m_contribution < 15) return;
		m_game->m_client_list[client_h]->m_contribution -= 15;
		break;

	default:
		return;
		break;
	}

	// Server-authoritative correction for all hero items.
	// Don't trust the client's item_id — derive the correct variant
	// from the server's own m_side and m_sex.
	bool is_elvine = (m_game->m_client_list[client_h]->m_side == 2);
	bool is_female = (m_game->m_client_list[client_h]->m_sex == 2);

	// Cape (400-401): [Aresden, Elvine] — no sex variant
	if (item_id == ItemId::AresdenHeroCape || item_id == ItemId::ElvineHeroCape)
	{
		item_id = is_elvine ? ItemId::ElvineHeroCape : ItemId::AresdenHeroCape;
	}
	// Gendered items (403-426): groups of 4 [Aresden M, Aresden W, Elvine M, Elvine W]
	else if (item_id >= ItemId::AresdenHeroHelmM && item_id <= ItemId::ElvineHeroLeggingsW)
	{
		constexpr int gendered_first = ItemId::AresdenHeroHelmM;
		constexpr int group_size = 4;
		int group_base = gendered_first
			+ ((item_id - gendered_first) / group_size) * group_size;
		int side_offset = is_elvine ? 2 : 0;
		int sex_offset = is_female ? 1 : 0;
		item_id = group_base + side_offset + sex_offset;
	}

	num = 1;
	for(int i = 1; i <= num; i++)
	{
		item = new CItem;
		if (init_item_attr(item, item_id) == false)
		{
			delete item;
		}
		else {

			if (add_client_item_list(client_h, item, &erase_req)) {
				if (m_game->m_client_list[client_h]->m_cur_weight_load < 0) m_game->m_client_list[client_h]->m_cur_weight_load = 0;

				hb::logger::log<log_channel::events>("get HeroItem : Char({}) Player-EK({}) Player-Contr({}) Hero Obtained({})", m_game->m_client_list[client_h]->m_char_name, m_game->m_client_list[client_h]->m_enemy_kill_count, m_game->m_client_list[client_h]->m_contribution, item->m_name);

				item->set_touch_effect_type(TouchEffectType::UniqueOwner);
				item->m_instance.touch_effect_value1 = m_game->m_client_list[client_h]->m_char_id_num1;
				item->m_instance.touch_effect_value2 = m_game->m_client_list[client_h]->m_char_id_num2;
				item->m_instance.touch_effect_value3 = m_game->m_client_list[client_h]->m_char_id_num3;

				ret = send_item_notify_msg(client_h, Notify::ItemObtained, item, 0);

				m_game->calc_total_weight(client_h);

				switch (ret) {
				case sock::Event::QueueFull:
				case sock::Event::SocketError:
				case sock::Event::CriticalError:
				case sock::Event::SocketClosed:
					m_game->delete_client(client_h, true, true);
					return;
				}

				m_game->send_notify_msg(0, client_h, Notify::EnemyKills, m_game->m_client_list[client_h]->m_enemy_kill_count, 0, 0, 0);
				m_game->send_notify_msg(0, client_h, Notify::Contribution, m_game->m_client_list[client_h]->m_contribution, 0, 0, 0);
			}
			else
			{
				delete item;

				m_game->calc_total_weight(client_h);

				ret = send_item_notify_msg(client_h, Notify::CannotCarryMoreItem, 0, 0);

				switch (ret) {
				case sock::Event::QueueFull:
				case sock::Event::SocketError:
				case sock::Event::CriticalError:
				case sock::Event::SocketClosed:

					m_game->delete_client(client_h, true, true);
					return;
				}
			}
		}
	}
}

void ItemManager::set_item_pos(int client_h, char* data)
{
	char item_index;
	short sX, sY;

	if (m_game->m_client_list[client_h] == 0) return;

	const auto* req = hb::net::PacketCast<hb::net::PacketRequestSetItemPos>(data, sizeof(hb::net::PacketRequestSetItemPos));
	if (!req) return;
	item_index = static_cast<char>(req->dir);
	sX = req->x;
	sY = req->y;

	if (sY < -10) sY = -10;

	if ((item_index < 0) || (item_index >= hb::shared::limits::MaxItems)) return;
	if (m_game->m_client_list[client_h]->m_item_list[item_index] != 0) {
		m_game->m_client_list[client_h]->m_item_pos_list[item_index].x = sX;
		m_game->m_client_list[client_h]->m_item_pos_list[item_index].y = sY;
	}
}

void ItemManager::check_unique_item_equipment(int client_h)
{
	int damage;

	if (m_game->m_client_list[client_h] == 0) return;

	for(int i = 0; i < hb::shared::limits::MaxItems; i++)
		if (m_game->m_client_list[client_h]->m_item_list[i] != 0) {
			if ((m_game->m_client_list[client_h]->m_item_list[i]->get_touch_effect_type() == TouchEffectType::UniqueOwner) &&
				(m_game->m_client_list[client_h]->m_is_item_equipped[i])) {
				// Touch Effect Type DEF_ITET_OWNER Touch Effect Value 1, 2, 3    .

				if ((m_game->m_client_list[client_h]->m_item_list[i]->m_instance.touch_effect_value1 == m_game->m_client_list[client_h]->m_char_id_num1) &&
					(m_game->m_client_list[client_h]->m_item_list[i]->m_instance.touch_effect_value2 == m_game->m_client_list[client_h]->m_char_id_num2) &&
					(m_game->m_client_list[client_h]->m_item_list[i]->m_instance.touch_effect_value3 == m_game->m_client_list[client_h]->m_char_id_num3)) {
				}
				else {
					m_game->send_notify_msg(0, client_h, Notify::ItemReleased, m_game->m_client_list[client_h]->m_item_list[i]->m_equip_pos, i, 0, 0);
					release_item_handler(client_h, i, true);
					damage = m_game->dice(10, 10);
					m_game->m_client_list[client_h]->m_hp -= damage;
					if (m_game->m_client_list[client_h]->m_hp <= 0) {
						m_game->m_combat_manager->client_killed_handler(client_h, 0, 0, damage);
					}
				}
			}
		}
}

void ItemManager::exchange_item_handler(int client_h, short item_index, int amount, short dX, short dY, uint16_t object_id, const char* item_name)
{
	short owner_h;
	char  owner_type;

	if (m_game->m_client_list[client_h] == 0) return;
	if ((item_index < 0) || (item_index >= hb::shared::limits::MaxItems)) return;
	if (m_game->m_client_list[client_h]->m_item_list[item_index] == 0) return;
	if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.count < static_cast<uint32_t>(amount)) return;
	if (m_game->m_client_list[client_h]->m_is_on_server_change) return;
	if (m_game->m_client_list[client_h]->m_is_exchange_mode) return;
	if (object_id >= MaxClients) return;

	// dX, dY     .
	m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);

	if ((owner_h != 0) && (owner_type == hb::shared::owner_class::Player)) {

		if (object_id != 0) {
			if (hb::shared::object_id::is_player_id(object_id)) {
				if (m_game->m_client_list[object_id] != 0) {
					if ((uint16_t)owner_h != object_id) owner_h = 0;
				}
			}
			else owner_h = 0;
		}

		if ((owner_h == 0) || (m_game->m_client_list[owner_h] == 0)) {
			clear_exchange_status(client_h);
		}
		else {
			if ((m_game->m_client_list[owner_h]->m_is_exchange_mode) || (m_game->m_client_list[owner_h]->m_appearance.is_walking) ||
				(m_game->m_map_list[m_game->m_client_list[owner_h]->m_map_index]->m_is_fight_zone)) {
				clear_exchange_status(client_h);
			}
			else {
				m_game->m_client_list[client_h]->m_is_exchange_mode = true;
				m_game->m_client_list[client_h]->m_exchange_h = owner_h;
				std::memset(m_game->m_client_list[client_h]->m_exchange_name, 0, sizeof(m_game->m_client_list[client_h]->m_exchange_name));
				strcpy(m_game->m_client_list[client_h]->m_exchange_name, m_game->m_client_list[owner_h]->m_char_name);

				//Clear items in the list
				m_game->m_client_list[client_h]->exchange_count = 0;
				m_game->m_client_list[owner_h]->exchange_count = 0;
				for(int i = 0; i < 4; i++) {
					//Clear the trader
					m_game->m_client_list[client_h]->m_exchange_item_id[i] = 0;
					m_game->m_client_list[client_h]->m_exchange_item_index[i] = -1;
					m_game->m_client_list[client_h]->m_exchange_item_amount[i] = 0;
					//Clear the guy we're trading with
					m_game->m_client_list[owner_h]->m_exchange_item_id[i] = 0;
					m_game->m_client_list[owner_h]->m_exchange_item_index[i] = -1;
					m_game->m_client_list[owner_h]->m_exchange_item_amount[i] = 0;
				}

				m_game->m_client_list[client_h]->m_exchange_item_index[m_game->m_client_list[client_h]->exchange_count] = (char)item_index;
				m_game->m_client_list[client_h]->m_exchange_item_amount[m_game->m_client_list[client_h]->exchange_count] = amount;

				m_game->m_client_list[client_h]->m_exchange_item_id[m_game->m_client_list[client_h]->exchange_count] = m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num;

				m_game->m_client_list[owner_h]->m_is_exchange_mode = true;
				m_game->m_client_list[owner_h]->m_exchange_h = client_h;
				std::memset(m_game->m_client_list[owner_h]->m_exchange_name, 0, sizeof(m_game->m_client_list[owner_h]->m_exchange_name));
				strcpy(m_game->m_client_list[owner_h]->m_exchange_name, m_game->m_client_list[client_h]->m_char_name);

				m_game->m_client_list[client_h]->exchange_count++;
				m_game->send_exchange_item_notify(client_h, client_h, Notify::OpenExchangeWindow, item_index + 1000,
					m_game->m_client_list[client_h]->m_item_list[item_index], amount);

				m_game->send_exchange_item_notify(client_h, owner_h, Notify::OpenExchangeWindow, item_index,
					m_game->m_client_list[client_h]->m_item_list[item_index], amount);
			}
		}
	}
	else {
		// NPC    .
		clear_exchange_status(client_h);

	}
}

void ItemManager::set_exchange_item(int client_h, int item_index, int amount)
{
	int ex_h;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_on_server_change) return;
	if (m_game->m_client_list[client_h]->exchange_count > 4) return;	//only 4 items trade

	if ((m_game->m_client_list[client_h]->m_is_exchange_mode) && (m_game->m_client_list[client_h]->m_exchange_h != 0)) {
		ex_h = m_game->m_client_list[client_h]->m_exchange_h;
		if ((m_game->m_client_list[ex_h] == 0) || (hb_strnicmp(m_game->m_client_list[client_h]->m_exchange_name, m_game->m_client_list[ex_h]->m_char_name, hb::shared::limits::CharNameLen - 1) != 0)) {

		}
		else {
			if ((item_index < 0) || (item_index >= hb::shared::limits::MaxItems)) return;
			if (m_game->m_client_list[client_h]->m_item_list[item_index] == 0) return;
			if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.count < static_cast<uint32_t>(amount)) return;

			//No Duplicate items
			for(int i = 0; i < m_game->m_client_list[client_h]->exchange_count; i++) {
				if (m_game->m_client_list[client_h]->m_exchange_item_index[i] == (char)item_index) {
					clear_exchange_status(ex_h);
					clear_exchange_status(client_h);
					return;
				}
			}

			m_game->m_client_list[client_h]->m_exchange_item_index[m_game->m_client_list[client_h]->exchange_count] = (char)item_index;
			m_game->m_client_list[client_h]->m_exchange_item_amount[m_game->m_client_list[client_h]->exchange_count] = amount;

			m_game->m_client_list[client_h]->m_exchange_item_id[m_game->m_client_list[client_h]->exchange_count] = m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num;

			m_game->m_client_list[client_h]->exchange_count++;
			m_game->send_exchange_item_notify(client_h, client_h, Notify::set_exchange_item, item_index + 1000,
				m_game->m_client_list[client_h]->m_item_list[item_index], amount);

			m_game->send_exchange_item_notify(client_h, ex_h, Notify::set_exchange_item, item_index,
				m_game->m_client_list[client_h]->m_item_list[item_index], amount);
		}
	}
	else {
	}
}

void ItemManager::confirm_exchange_item(int client_h)
{
	int ex_h;
	int item_weight_a, item_weight_b, weight_left_a, weight_left_b, amount_left;
	CItem* item_a[4], * item_b[4], * item_acopy[4], * item_bcopy[4];

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_on_server_change) return;

	if ((m_game->m_client_list[client_h]->m_is_exchange_mode) && (m_game->m_client_list[client_h]->m_exchange_h != 0)) {
		ex_h = m_game->m_client_list[client_h]->m_exchange_h;

		if (client_h == ex_h) return;

		if (m_game->m_client_list[ex_h] != 0) {
			if ((hb_strnicmp(m_game->m_client_list[client_h]->m_exchange_name, m_game->m_client_list[ex_h]->m_char_name, hb::shared::limits::CharNameLen - 1) != 0) ||
				(m_game->m_client_list[ex_h]->m_is_exchange_mode != true) ||
				(hb_strnicmp(m_game->m_client_list[ex_h]->m_exchange_name, m_game->m_client_list[client_h]->m_char_name, hb::shared::limits::CharNameLen - 1) != 0)) {
				clear_exchange_status(client_h);
				clear_exchange_status(ex_h);
				return;
			}
			else {
				m_game->m_client_list[client_h]->m_is_exchange_confirm = true;
				if (m_game->m_client_list[ex_h]->m_is_exchange_confirm) {

					//Check all items
					for(int i = 0; i < m_game->m_client_list[client_h]->exchange_count; i++) {
						if ((m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_exchange_item_index[i]] == 0) ||
							(m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_exchange_item_index[i]]->m_id_num != m_game->m_client_list[client_h]->m_exchange_item_id[i])) {
							clear_exchange_status(client_h);
							clear_exchange_status(ex_h);
							return;
						}
					}
					for(int i = 0; i < m_game->m_client_list[ex_h]->exchange_count; i++) {
						if ((m_game->m_client_list[ex_h]->m_item_list[m_game->m_client_list[ex_h]->m_exchange_item_index[i]] == 0) ||
							(m_game->m_client_list[ex_h]->m_item_list[m_game->m_client_list[ex_h]->m_exchange_item_index[i]]->m_id_num != m_game->m_client_list[ex_h]->m_exchange_item_id[i])) {
							clear_exchange_status(client_h);
							clear_exchange_status(ex_h);
							return;
						}
					}

					weight_left_a = m_game->calc_max_load(client_h) - m_game->calc_total_weight(client_h);
					weight_left_b = m_game->calc_max_load(ex_h) - m_game->calc_total_weight(ex_h);

					//Calculate weight for items
					item_weight_a = 0;
					for(int i = 0; i < m_game->m_client_list[client_h]->exchange_count; i++) {
						item_weight_a = get_item_weight(m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_exchange_item_index[i]],
							m_game->m_client_list[client_h]->m_exchange_item_amount[i]);
					}
					item_weight_b = 0;
					for(int i = 0; i < m_game->m_client_list[ex_h]->exchange_count; i++) {
						item_weight_b = get_item_weight(m_game->m_client_list[ex_h]->m_item_list[m_game->m_client_list[ex_h]->m_exchange_item_index[i]],
							m_game->m_client_list[ex_h]->m_exchange_item_amount[i]);
					}

					//See if the other person can take the item weightload
					if ((weight_left_a < item_weight_b) || (weight_left_b < item_weight_a)) {
						clear_exchange_status(client_h);
						clear_exchange_status(ex_h);
						return;
					}

					for(int i = 0; i < m_game->m_client_list[client_h]->exchange_count; i++) {
						if (m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_exchange_item_index[i]]->is_stackable()) {

							if (static_cast<uint32_t>(m_game->m_client_list[client_h]->m_exchange_item_amount[i]) >
								m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_exchange_item_index[i]]->m_instance.count) {
								clear_exchange_status(client_h);
								clear_exchange_status(ex_h);
								return;
							}
							item_a[i] = new CItem;
							init_item_attr(item_a[i], m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_exchange_item_index[i]]->m_name);
							item_a[i]->m_instance.count = m_game->m_client_list[client_h]->m_exchange_item_amount[i];

							item_acopy[i] = new CItem;
							init_item_attr(item_acopy[i], m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_exchange_item_index[i]]->m_name);
							copy_item_contents(item_acopy[i], item_a[i]);
							item_acopy[i]->m_instance.count = m_game->m_client_list[client_h]->m_exchange_item_amount[i];
						}
						else {
							item_a[i] = (CItem*)m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_exchange_item_index[i]];
							item_a[i]->m_instance.count = m_game->m_client_list[client_h]->m_exchange_item_amount[i];

							item_acopy[i] = new CItem;
							init_item_attr(item_acopy[i], m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_exchange_item_index[i]]->m_name);
							copy_item_contents(item_acopy[i], item_a[i]);
							item_acopy[i]->m_instance.count = m_game->m_client_list[client_h]->m_exchange_item_amount[i];
						}
					}

					for(int i = 0; i < m_game->m_client_list[ex_h]->exchange_count; i++) {
						if (m_game->m_client_list[ex_h]->m_item_list[m_game->m_client_list[ex_h]->m_exchange_item_index[i]]->is_stackable()) {

							if (static_cast<uint32_t>(m_game->m_client_list[ex_h]->m_exchange_item_amount[i]) >
								m_game->m_client_list[ex_h]->m_item_list[m_game->m_client_list[ex_h]->m_exchange_item_index[i]]->m_instance.count) {
								clear_exchange_status(client_h);
								clear_exchange_status(ex_h);
								return;
							}
							item_b[i] = new CItem;
							init_item_attr(item_b[i], m_game->m_client_list[ex_h]->m_item_list[m_game->m_client_list[ex_h]->m_exchange_item_index[i]]->m_name);
							item_b[i]->m_instance.count = m_game->m_client_list[ex_h]->m_exchange_item_amount[i];

							item_bcopy[i] = new CItem;
							init_item_attr(item_bcopy[i], m_game->m_client_list[ex_h]->m_item_list[m_game->m_client_list[ex_h]->m_exchange_item_index[i]]->m_name);
							copy_item_contents(item_bcopy[i], item_b[i]);
							item_bcopy[i]->m_instance.count = m_game->m_client_list[ex_h]->m_exchange_item_amount[i];
						}
						else {
							item_b[i] = (CItem*)m_game->m_client_list[ex_h]->m_item_list[m_game->m_client_list[ex_h]->m_exchange_item_index[i]];
							item_b[i]->m_instance.count = m_game->m_client_list[ex_h]->m_exchange_item_amount[i];

							item_bcopy[i] = new CItem;
							init_item_attr(item_bcopy[i], m_game->m_client_list[ex_h]->m_item_list[m_game->m_client_list[ex_h]->m_exchange_item_index[i]]->m_name);
							copy_item_contents(item_bcopy[i], item_b[i]);
							item_bcopy[i]->m_instance.count = m_game->m_client_list[ex_h]->m_exchange_item_amount[i];
						}
					}

					for(int i = 0; i < m_game->m_client_list[ex_h]->exchange_count; i++) {
						add_item(client_h, item_b[i], 0);
						item_log(ItemLogAction::Exchange, ex_h, client_h, item_bcopy[i]);
						delete item_bcopy[i];
						item_bcopy[i] = 0;
						if (m_game->m_client_list[ex_h]->m_item_list[m_game->m_client_list[ex_h]->m_exchange_item_index[i]]->is_stackable()) {
							amount_left = static_cast<int>(m_game->m_client_list[ex_h]->m_item_list[m_game->m_client_list[ex_h]->m_exchange_item_index[i]]->m_instance.count) - m_game->m_client_list[ex_h]->m_exchange_item_amount[i];
							if (amount_left < 0) amount_left = 0;
							// v1.41 !!!
							set_item_count(ex_h, m_game->m_client_list[ex_h]->m_exchange_item_index[i], amount_left);
							// m_game->m_client_list[ex_h]->m_item_list[m_game->m_client_list[ex_h]->m_exchange_item_index]->m_name, amount_left);
						}
						else {
							release_item_handler(ex_h, m_game->m_client_list[ex_h]->m_exchange_item_index[i], true);
							m_game->send_notify_msg(0, ex_h, Notify::GiveItemFinEraseItem, m_game->m_client_list[ex_h]->m_exchange_item_index[i], m_game->m_client_list[ex_h]->m_exchange_item_amount[i], 0, m_game->m_client_list[client_h]->m_char_name);
							m_game->m_client_list[ex_h]->m_item_list[m_game->m_client_list[ex_h]->m_exchange_item_index[i]] = 0;
						}
					}

					for(int i = 0; i < m_game->m_client_list[client_h]->exchange_count; i++) {
						add_item(ex_h, item_a[i], 0);
						item_log(ItemLogAction::Exchange, client_h, ex_h, item_acopy[i]);
						delete item_acopy[i];
						item_acopy[i] = 0;

						if (m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_exchange_item_index[i]]->is_stackable()) {
							amount_left = static_cast<int>(m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_exchange_item_index[i]]->m_instance.count) - m_game->m_client_list[client_h]->m_exchange_item_amount[i];
							if (amount_left < 0) amount_left = 0;
							// v1.41 !!!
							set_item_count(client_h, m_game->m_client_list[client_h]->m_exchange_item_index[i], amount_left);
							// m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_exchange_item_index]->m_name, amount_left);
						}
						else {
							release_item_handler(client_h, m_game->m_client_list[client_h]->m_exchange_item_index[i], true);
							m_game->send_notify_msg(0, client_h, Notify::GiveItemFinEraseItem, m_game->m_client_list[client_h]->m_exchange_item_index[i], m_game->m_client_list[client_h]->m_exchange_item_amount[i], 0, m_game->m_client_list[ex_h]->m_char_name);
							m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_exchange_item_index[i]] = 0;
						}
					}

					m_game->m_client_list[client_h]->m_is_exchange_mode = false;
					m_game->m_client_list[client_h]->m_is_exchange_confirm = false;
					std::memset(m_game->m_client_list[client_h]->m_exchange_name, 0, sizeof(m_game->m_client_list[client_h]->m_exchange_name));
					m_game->m_client_list[client_h]->m_exchange_h = 0;
					m_game->m_client_list[client_h]->exchange_count = 0;

					m_game->m_client_list[ex_h]->m_is_exchange_mode = false;
					m_game->m_client_list[ex_h]->m_is_exchange_confirm = false;
					std::memset(m_game->m_client_list[ex_h]->m_exchange_name, 0, sizeof(m_game->m_client_list[ex_h]->m_exchange_name));
					m_game->m_client_list[ex_h]->m_exchange_h = 0;
					m_game->m_client_list[ex_h]->exchange_count = 0;

					for(int i = 0; i < 4; i++) {
						m_game->m_client_list[client_h]->m_exchange_item_index[i] = -1;
						m_game->m_client_list[ex_h]->m_exchange_item_index[i] = -1;
					}

					m_game->send_notify_msg(0, client_h, Notify::ExchangeItemComplete, 0, 0, 0, 0);
					m_game->send_notify_msg(0, ex_h, Notify::ExchangeItemComplete, 0, 0, 0, 0);

					m_game->calc_total_weight(client_h);
					m_game->calc_total_weight(ex_h);
					return;
				}
			}
		}
		else {
			clear_exchange_status(client_h);
			return;
		}
	}
}

int ItemManager::get_item_space_left(int client_h)
{
	int total_item;

	total_item = 0;
	for(int i = 0; i < hb::shared::limits::MaxItems; i++)
		if (m_game->m_client_list[client_h]->m_item_list[i] != 0) total_item++;

	return (hb::shared::limits::MaxItems - total_item);
}

bool ItemManager::add_item(int client_h, CItem* item, char mode)
{
	int ret, erase_req;

	if (add_client_item_list(client_h, item, &erase_req)) {
		ret = send_item_notify_msg(client_h, Notify::ItemObtained, item, 0);

		return true;
	}
	else {
		m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(m_game->m_client_list[client_h]->m_x,
			m_game->m_client_list[client_h]->m_y,
			item);

		m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index,
			m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, item);

		ret = send_item_notify_msg(client_h, Notify::CannotCarryMoreItem, 0, 0);

		return true;
	}

	return false;
}

int ItemManager::send_item_notify_msg(int client_h, uint16_t msg_type, CItem* item, int v1)
{
	int ret = 0;

	if (m_game->m_client_list[client_h] == 0) return 0;

	switch (msg_type) {
	case Notify::ItemObtained:
	{
		hb::net::PacketNotifyItemObtained pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.is_new = 1;
		memcpy(pkt.name, item->m_name, sizeof(pkt.name));
		pkt.count = item->m_instance.count;
		pkt.item_type = item->m_item_type;
		pkt.equip_pos = item->m_equip_pos;
		pkt.is_equipped = 0;
		pkt.level_limit = item->m_level_requirement;
		pkt.gender_limit = item->m_gender_requirement;
		pkt.cur_durability = item->m_instance.cur_durability;
		pkt.weight = item->m_weight;
		pkt.item_color = item->m_instance.item_color;
		pkt.spec_value2 = static_cast<uint8_t>(item->m_instance.special_effect_value2);
		item->copy_attributes_to(pkt);
		pkt.item_id = item->m_id_num;
		pkt.max_durability = item->m_durability;
		ret = m_game->m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::ItemPurchased:
	{
		hb::net::PacketNotifyItemPurchased pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		pkt.is_new = 1;
		memcpy(pkt.name, item->m_name, sizeof(pkt.name));
		pkt.count = item->m_instance.count;
		pkt.item_type = item->m_item_type;
		pkt.equip_pos = item->m_equip_pos;
		pkt.is_equipped = 0;
		pkt.level_limit = item->m_level_requirement;
		pkt.gender_limit = item->m_gender_requirement;
		pkt.cur_durability = item->m_instance.cur_durability;
		pkt.weight = item->m_weight;
		pkt.item_color = item->m_instance.item_color;
		pkt.cost = static_cast<uint16_t>(v1);
		pkt.item_id = item->m_id_num;
		pkt.max_durability = item->m_durability;
		ret = m_game->m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;

	case Notify::CannotCarryMoreItem:
	{
		hb::net::PacketNotifyEmpty pkt{};
		pkt.header.msg_id = MsgId::Notify;
		pkt.header.msg_type = msg_type;
		ret = m_game->m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
	}
	break;
	}

	return ret;
}

bool ItemManager::check_item_receive_condition(int client_h, CItem* item)
{
	

	if (m_game->m_client_list[client_h] == 0) return false;

	if (m_game->m_client_list[client_h]->m_cur_weight_load + get_item_weight(item, static_cast<int>(item->m_instance.count)) > m_game->calc_max_load(client_h))
		return false;

	for(int i = 0; i < hb::shared::limits::MaxItems; i++)
		if (m_game->m_client_list[client_h]->m_item_list[i] == 0) return true;

	return false;
}

void ItemManager::build_item_handler(int client_h, char* data)
{
	char name[hb::shared::limits::ItemNameLen], element_item_id[6];
	int    x, z, match, count, player_skill_level, result, total_value, result_value, temp, item_count[hb::shared::limits::MaxItems];
	CItem* item;
	bool   flag, item_flag[6];
	double v1, v2, v3;
	uint32_t  dw_temp;
	uint16_t   w_temp;

	if (m_game->m_client_list[client_h] == 0) return;
	m_game->m_client_list[client_h]->m_skill_msg_recv_count++;

	const auto* pkt = hb::net::PacketCast<hb::net::PacketCommandCommonBuild>(
		data, sizeof(hb::net::PacketCommandCommonBuild));
	if (!pkt) return;
	std::memset(name, 0, sizeof(name));
	memcpy(name, pkt->name, sizeof(pkt->name));

	//testcode
	//PutLogList(name);

	std::memset(element_item_id, 0, sizeof(element_item_id));
	for(int i = 0; i < 6; i++) {
		element_item_id[i] = static_cast<char>(pkt->item_ids[i]);
	}

	flag = true;
	while (flag) {
		flag = false;
		for(int i = 0; i <= 4; i++)
			if ((element_item_id[i] == -1) && (element_item_id[i + 1] != -1)) {
				element_item_id[i] = element_item_id[i + 1];
				element_item_id[i + 1] = -1;
				flag = true;
			}
	}

	for(int i = 0; i < 6; i++) item_flag[i] = false;

	//testcode
	//std::snprintf(G_cTxt, sizeof(G_cTxt), "%d %d %d %d %d %d", element_item_id[0], element_item_id[1], element_item_id[2],
	//	     element_item_id[3], element_item_id[4], element_item_id[5]);
	//PutLogList(G_cTxt);

	player_skill_level = m_game->m_client_list[client_h]->m_skill_mastery[13];
	result = m_game->dice(1, 100);

	if (result > player_skill_level) {
		m_game->send_notify_msg(0, client_h, Notify::BuildItemFail, 0, 0, 0, 0);
		return;
	}

	for(int i = 0; i < 6; i++)
		if (element_item_id[i] != -1) {
			// Item ID.
			if ((element_item_id[i] < 0) || (element_item_id[i] > hb::shared::limits::MaxItems)) return;
			if (m_game->m_client_list[client_h]->m_item_list[element_item_id[i]] == 0) return;
		}

	for(int i = 0; i < hb::shared::limits::MaxBuildItems; i++)
		if (m_game->m_build_item_list[i] != 0) {
			if (memcmp(m_game->m_build_item_list[i]->m_name, name, hb::shared::limits::ItemNameLen - 1) == 0) {

				if (m_game->m_build_item_list[i]->m_skill_limit > m_game->m_client_list[client_h]->m_skill_mastery[13]) return;

				for (x = 0; x < hb::shared::limits::MaxItems; x++)
					if (m_game->m_client_list[client_h]->m_item_list[x] != 0)
						item_count[x] = static_cast<int>(m_game->m_client_list[client_h]->m_item_list[x]->m_instance.count);
					else item_count[x] = 0;

				match = 0;
				total_value = 0;

				for (x = 0; x < 6; x++) {
					if (m_game->m_build_item_list[i]->m_material_item_count[x] == 0) {
						match++;
					}
					else {
						for (z = 0; z < 6; z++)
							if ((element_item_id[z] != -1) && (item_flag[z] == false)) {

								if ((m_game->m_client_list[client_h]->m_item_list[element_item_id[z]]->m_id_num == m_game->m_build_item_list[i]->m_material_item_id[x]) &&
									(m_game->m_client_list[client_h]->m_item_list[element_item_id[z]]->m_instance.count >=
										static_cast<uint32_t>(m_game->m_build_item_list[i]->m_material_item_count[x])) &&
									(item_count[element_item_id[z]] > 0)) {
									dw_temp = m_game->m_client_list[client_h]->m_item_list[element_item_id[z]]->m_instance.special_effect_value2;
									if (dw_temp > m_game->m_client_list[client_h]->m_skill_mastery[13]) {
										dw_temp = dw_temp - (dw_temp - m_game->m_client_list[client_h]->m_skill_mastery[13]) / 2;
									}

									total_value += (dw_temp * m_game->m_build_item_list[i]->m_material_item_value[x]);
									item_count[element_item_id[z]] -= m_game->m_build_item_list[i]->m_material_item_count[x];
									match++;
									item_flag[z] = true;

									break;
								}
							}
					}
				}

				// match 6     .
				if (match != 6) {
					m_game->send_notify_msg(0, client_h, Notify::BuildItemFail, 0, 0, 0, 0);
					return;
				}

				v2 = (double)m_game->m_build_item_list[i]->m_max_value;
				if (total_value <= 0)
					v3 = 1.0f;
				else v3 = (double)total_value;
				v1 = (double)(v3 / v2) * 100.0f;

				total_value = (int)v1;

				item = new CItem;
				if (init_item_attr(item, m_game->m_build_item_list[i]->m_name) == false) {
					delete item;
					return;
				}

				// Custom-Made
				item->m_instance.custom_made = 1;

				if (item->get_item_type() == hb::shared::item::item_type::material) {
					temp = m_game->dice(1, (player_skill_level / 2) + 1) - 1;
					item->m_instance.special_effect_value2 = (player_skill_level / 2) + temp;
					item->set_touch_effect_type(TouchEffectType::ID);
					item->m_instance.touch_effect_value1 = static_cast<short>(m_game->dice(1, 100000));
					item->m_instance.touch_effect_value2 = static_cast<short>(m_game->dice(1, 100000));
					item->m_instance.touch_effect_value3 = static_cast<short>(GameClock::GetTimeMS());

				}
				else {
					// Copy prefix attributes from build item definition
					uint16_t build_attr = m_game->m_build_item_list[i]->m_attribute;
					item->m_instance.prefix_value = static_cast<uint8_t>(build_attr & 0x0F);
					item->m_instance.prefix_type = static_cast<uint8_t>((build_attr >> 4) & 0x0F);
					item->m_instance.enchant_bonus = static_cast<uint8_t>((build_attr >> 12) & 0x0F);

					result_value = (total_value - m_game->m_build_item_list[i]->m_average_value);
					// : SpecEffectValue1 , SpecEffectValue2

					// 1.   ()
					if (result_value > 0) {
						v2 = (double)result_value;
						v3 = (double)(100 - m_game->m_build_item_list[i]->m_average_value);
						v1 = (v2 / v3) * 100.0f;
						item->m_instance.special_effect_value2 = (int)v1;
					}
					else if (result_value < 0) {
						v2 = (double)(result_value);
						v3 = (double)(m_game->m_build_item_list[i]->m_average_value);
						v1 = (v2 / v3) * 100.0f;
						item->m_instance.special_effect_value2 = (int)v1;
					}
					else item->m_instance.special_effect_value2 = 0;

					v2 = (double)item->m_instance.special_effect_value2;
					v3 = (double)item->m_durability;
					v1 = (v2 / 100.0f) * v3;

					w_temp = (int)item->m_durability;
					w_temp += (int)v1;

					item->set_touch_effect_type(TouchEffectType::ID);
					item->m_instance.touch_effect_value1 = static_cast<short>(m_game->dice(1, 100000));
					item->m_instance.touch_effect_value2 = static_cast<short>(m_game->dice(1, 100000));
					item->m_instance.touch_effect_value3 = static_cast<short>(GameClock::GetTimeMS());

					if (w_temp <= 0)
						w_temp = 1;
					else w_temp = (uint16_t)w_temp;

					if (w_temp <= item->m_durability * 2) {
						item->m_durability = w_temp;
						item->m_instance.special_effect_value1 = (short)w_temp;
						item->m_instance.cur_durability = item->m_durability;
					}
					else item->m_instance.special_effect_value1 = (short)item->m_durability;

					// Custom-Item  2.
					item->m_instance.item_color = 2;
				}

				//testcode
				hb::logger::log("Custom-Item({}) Value({}) Life({}/{})", item->m_name, item->m_instance.special_effect_value2, item->m_instance.cur_durability, item->m_durability);

				add_item(client_h, item, 0);
				m_game->send_notify_msg(0, client_h, Notify::BuildItemSuccess, item->m_instance.special_effect_value2, item->m_item_type, 0, 0); // Integer

				for (x = 0; x < 6; x++)
					if (element_item_id[x] != -1) {
						if (m_game->m_client_list[client_h]->m_item_list[element_item_id[x]] == 0) {
							// ### BUG POINT!!!
							hb::logger::log<log_channel::events>("(?) Char({}) ElementItemID({})", m_game->m_client_list[client_h]->m_char_name, element_item_id[x]);
						}
						else {
							count = static_cast<int>(m_game->m_client_list[client_h]->m_item_list[element_item_id[x]]->m_instance.count) - m_game->m_build_item_list[i]->m_material_item_count[x];
							if (count < 0) count = 0;
							set_item_count(client_h, element_item_id[x], count);
						}
					}

				if (m_game->m_build_item_list[i]->m_max_skill > m_game->m_client_list[client_h]->m_skill_mastery[13])
					m_game->m_skill_manager->calculate_ssn_skill_index(client_h, 13, 1);

				m_game->get_exp(client_h, m_game->dice(1, (m_game->m_build_item_list[i]->m_skill_limit / 4))); //m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, (m_game->m_build_item_list[i]->m_skill_limit/4));

				return;
			}
		}

}

void ItemManager::adjust_rare_item_value(CItem* item)
{
	uint32_t swe_type, swe_value;
	double v1, v2, v3;

	if (item->m_instance.prefix_type != static_cast<uint8_t>(hb::shared::item::AttributePrefixType::None)) {
		swe_type = static_cast<int>(item->m_instance.prefix_type);
		swe_value = item->m_instance.prefix_value;
		// 0-None 1- 2- 3-
		// 5- 6- 7- 8- 9-
		switch (swe_type) {
		case 0: break;

		case 5:
			item->m_swing_speed--;
			if (item->m_swing_speed < 0) item->m_swing_speed = 0;
			break;

		case 6:
			v2 = (double)item->m_weight;
			v3 = (double)(swe_value * m_game->m_prefix_multiplier[6]);
			v1 = (v3 / 100.0f) * v2;
			item->m_weight -= (int)v1;

			if (item->m_weight < 1) item->m_weight = 1;
			break;

		case 8:
		case 9:
			v2 = (double)item->m_durability;
			v3 = (double)(swe_value * m_game->m_prefix_multiplier[swe_type]);
			v1 = (v3 / 100.0f) * v2;
			item->m_durability += (int)v1;
			break;
		}
	}
}

int ItemManager::roll_attribute_value(int min_val, int max_val)
{
	if (max_val <= 0) return 0;
	if (min_val >= max_val) return min_val;

	// Weighted roll biased toward lower values within [min_val, max_val]
	static const int weights[] = { 10000, 7400, 5000, 3000, 2000, 1000, 500, 400, 300, 200, 100, 70, 30 };

	int range = max_val - min_val + 1;
	int total = 0;
	for (int i = 0; i < range; i++)
		total += (i < 13) ? weights[i] : weights[12];

	int roll = rand() % total;
	int cumulative = 0;
	for (int i = 0; i < range; i++)
	{
		cumulative += (i < 13) ? weights[i] : weights[12];
		if (roll < cumulative) return min_val + i;
	}
	return min_val;
}

bool ItemManager::generate_item_attributes(CItem* item)
{
	if (item == nullptr) return false;

	AttributePrefixType primaryType = AttributePrefixType::None;
	int primaryValue = 0;
	SecondaryEffectType secondaryType = SecondaryEffectType::None;
	int secondaryValue = 0;
	int item_color = 0;

	if (item->get_item_effect_type() == ItemEffectType::Attack) {
		// Attack weapons - roll primary prefix
		int roll = rand() % 10000;
		int cumul = 0;

		struct { int weight; AttributePrefixType type; int color; } attackPrimary[] = {
			{ 299,  AttributePrefixType::Light,      16 },
			{ 700,  AttributePrefixType::Strong,     16 },
			{ 1500, AttributePrefixType::Critical,   18 },
			{ 2000, AttributePrefixType::Agile,      16 },
			{ 2000, AttributePrefixType::Righteous,  20 },
			{ 1600, AttributePrefixType::Poisoning,  17 },
			{ 1600, AttributePrefixType::Sharp,      19 },
			{ 301,  AttributePrefixType::Ancient,    21 },
		};

		for (auto& entry : attackPrimary) {
			cumul += entry.weight;
			if (roll < cumul) {
				primaryType = entry.type;
				item_color = entry.color;
				int idx = static_cast<int>(entry.type);
				primaryValue = roll_attribute_value(m_game->m_prefix_min_value[idx], m_game->m_prefix_max_value[idx]);
				break;
			}
		}

		// Secondary effect - 40% chance (original rate)
		if (rand() % 100 < 40) {
		int secRoll = rand() % 10000;
		int secCumul = 0;

		struct { int weight; SecondaryEffectType type; int minVal; int maxVal; int fixedVal; } attackSecondary[] = {
			{ 4999, SecondaryEffectType::HittingProb,       3, 13, 0 },
			{ 3500, SecondaryEffectType::ConsecutiveAttack,  1, 7,  0 },
			{ 1000, SecondaryEffectType::GoldBonus,          0, 0,  5 },
			{ 501,  SecondaryEffectType::ExperienceBonus,    0, 0,  2 },
		};

		for (auto& entry : attackSecondary) {
			secCumul += entry.weight;
			if (secRoll < secCumul) {
				secondaryType = entry.type;
				if (entry.fixedVal > 0) {
					secondaryValue = entry.fixedVal;
				} else {
					secondaryValue = roll_attribute_value(entry.minVal, entry.maxVal);
				}
				break;
			}
		}
		} // end 40% secondary chance
	}
	else if (item->get_item_effect_type() == ItemEffectType::Defense) {
		// Defense armor - roll primary prefix
		int roll = rand() % 10000;
		int cumul = 0;

		struct { int weight; AttributePrefixType type; bool halved; } defensePrimary[] = {
			{ 5999, AttributePrefixType::Strong,         false },
			{ 3000, AttributePrefixType::Light,          false },
			{ 555,  AttributePrefixType::ManaConverting,  true },
			{ 446,  AttributePrefixType::CritChance,      true },
		};

		for (auto& entry : defensePrimary) {
			cumul += entry.weight;
			if (roll < cumul) {
				primaryType = entry.type;
				item_color = 0;
				int idx = static_cast<int>(entry.type);
				primaryValue = roll_attribute_value(m_game->m_prefix_min_value[idx], m_game->m_prefix_max_value[idx]);
				if (entry.halved) primaryValue = primaryValue / 2;
				break;
			}
		}

		// Secondary effect - 40% chance (original rate)
		if (rand() % 100 < 40) {
		int secRoll = rand() % 10001;
		int secCumul = 0;

		struct { int weight; SecondaryEffectType type; int minVal; int maxVal; } defenseSecondary[] = {
			{ 1000, SecondaryEffectType::DefenseRatio,      3, 13 },
			{ 3000, SecondaryEffectType::PoisonResistance,  3, 13 },
			{ 1500, SecondaryEffectType::SPRecovery,        1, 13 },
			{ 1000, SecondaryEffectType::HPRecovery,        1, 13 },
			{ 1000, SecondaryEffectType::MPRecovery,        1, 13 },
			{ 1900, SecondaryEffectType::MagicResistance,   3, 13 },
			{ 400,  SecondaryEffectType::PhysicalAbsorb,    3, 13 },
			{ 201,  SecondaryEffectType::MagicAbsorb,       3, 13 },
		};

		for (auto& entry : defenseSecondary) {
			secCumul += entry.weight;
			if (secRoll < secCumul) {
				secondaryType = entry.type;
				secondaryValue = roll_attribute_value(entry.minVal, entry.maxVal);
				break;
			}
		}
		} // end 40% secondary chance
	}
	else if (item->get_item_effect_type() == ItemEffectType::AttackManaSave) {
		// AttackManaSave - always type Special
		primaryType = AttributePrefixType::Special;
		item_color = 5;
		{
			int idx = static_cast<int>(AttributePrefixType::Special);
			primaryValue = roll_attribute_value(m_game->m_prefix_min_value[idx], m_game->m_prefix_max_value[idx]);
		}

		// Secondary effect - 40% chance (original rate)
		// Same secondary pool as attack weapons
		if (rand() % 100 < 40) {
		int secRoll = rand() % 10000;
		int secCumul = 0;

		struct { int weight; SecondaryEffectType type; int minVal; int maxVal; int fixedVal; } manaSaveSecondary[] = {
			{ 4999, SecondaryEffectType::HittingProb,       3, 13, 0 },
			{ 3500, SecondaryEffectType::ConsecutiveAttack,  1, 7,  0 },
			{ 1000, SecondaryEffectType::GoldBonus,          0, 0,  5 },
			{ 501,  SecondaryEffectType::ExperienceBonus,    0, 0,  2 },
		};

		for (auto& entry : manaSaveSecondary) {
			secCumul += entry.weight;
			if (secRoll < secCumul) {
				secondaryType = entry.type;
				if (entry.fixedVal > 0) {
					secondaryValue = entry.fixedVal;
				} else {
					secondaryValue = roll_attribute_value(entry.minVal, entry.maxVal);
				}
				break;
			}
		}
		} // end 40% secondary chance
	}
	else {
		// Item has no applicable effect type
		return false;
	}

	// Clamp values to nibble range (0-15)
	if (primaryValue > 15) primaryValue = 15;
	if (secondaryValue > 15) secondaryValue = 15;

	item->m_instance.item_color = (char)item_color;
	item->m_instance.custom_made = 0;
	item->m_instance.prefix_type = static_cast<uint8_t>(primaryType);
	item->m_instance.prefix_value = static_cast<uint8_t>(primaryValue);
	item->m_instance.secondary_type = static_cast<uint8_t>(secondaryType);
	item->m_instance.secondary_value = static_cast<uint8_t>(secondaryValue);
	item->m_instance.enchant_bonus = 0;

	adjust_rare_item_value(item);
	// New item: current durability should equal max durability
	item->m_instance.cur_durability = item->m_durability;
	return true;
}

void ItemManager::request_sell_item_list_handler(int client_h, char* data)
{
	int amount;
	char index;

	if (m_game->m_client_list[client_h] == 0) return;

	const auto* req = hb::net::PacketCast<hb::net::PacketRequestSellItemList>(data, sizeof(hb::net::PacketRequestSellItemList));
	if (!req) return;

	for(int i = 0; i < 12; i++) {
		index = static_cast<char>(req->entries[i].index);
		amount = req->entries[i].amount;

		if ((index == -1) || (index < 0) || (index >= hb::shared::limits::MaxItems)) return;
		if (m_game->m_client_list[client_h]->m_item_list[index] == 0) return;

		// index   .
		req_sell_item_confirm_handler(client_h, index, amount, 0);
		if (m_game->m_client_list[client_h] == 0) return;
	}
}

int ItemManager::get_item_weight(CItem* item, int count)
{
	if (count < 0) count = 1;
	return CItem::calc_item_stack_weight(item->get_effective_weight(), count);
}

bool ItemManager::copy_item_contents(CItem* copy, CItem* original)
{
	if (original == 0) return false;
	if (copy == 0) return false;

	copy->m_id_num = original->m_id_num;
	copy->m_item_type = original->m_item_type;
	copy->m_equip_pos = original->m_equip_pos;
	copy->m_item_effect_type = original->m_item_effect_type;
	copy->m_item_effect_value1 = original->m_item_effect_value1;
	copy->m_item_effect_value2 = original->m_item_effect_value2;
	copy->m_item_effect_value3 = original->m_item_effect_value3;
	copy->m_item_effect_value4 = original->m_item_effect_value4;
	copy->m_item_effect_value5 = original->m_item_effect_value5;
	copy->m_item_effect_value6 = original->m_item_effect_value6;
	copy->m_durability = original->m_durability;
	copy->m_special_effect = original->m_special_effect;

	//short m_sSM_HitRatio, m_sL_HitRatio;
	copy->m_special_effect_value1 = original->m_special_effect_value1;
	copy->m_special_effect_value2 = original->m_special_effect_value2;

	copy->m_weapon_class = original->m_weapon_class;
	copy->m_swing_speed = original->m_swing_speed;

	copy->m_sell_price = original->m_sell_price;
	copy->m_weight = original->m_weight;
	copy->m_level_requirement = original->m_level_requirement;
	copy->m_gender_requirement = original->m_gender_requirement;

	copy->m_related_skill = original->m_related_skill;

	copy->m_item_sub_type = original->m_item_sub_type;
	copy->m_hide_armor = original->m_hide_armor;
	copy->m_is_skirt = original->m_is_skirt;
	copy->m_stackable = original->m_stackable;
	copy->m_is_dyeable = original->m_is_dyeable;
	copy->m_armor_class = original->m_armor_class;
	copy->m_set_id = original->m_set_id;

	copy->m_instance = original->m_instance;
	copy->m_display_id = original->m_display_id;

	return true;
}

bool ItemManager::item_log(int action, int give_h, int recv_h, CItem* item, bool force_item_log)
{
	if (item == 0) return false;
	if (m_game->m_client_list[give_h] == 0) return false;

	switch (action) {

	case ItemLogAction::Exchange:
		if (m_game->m_client_list[recv_h] == 0) return false;
		hb::logger::log<log_channel::trade>("{}{} IP({}) Exchange {} at {}({},{}) -> {}", is_item_suspicious(item) ? "[SUSPICIOUS] " : "", m_game->m_client_list[give_h]->m_char_name, m_game->m_client_list[give_h]->m_ip_address, format_item_info(item), m_game->m_client_list[give_h]->m_map_name, m_game->m_client_list[give_h]->m_x, m_game->m_client_list[give_h]->m_y, m_game->m_client_list[recv_h]->m_char_name);
		break;

	case ItemLogAction::Give:
		if (m_game->m_client_list[recv_h] == 0) return false;
		hb::logger::log<log_channel::trade>("{}{} IP({}) Give {} at {}({},{}) -> {}", is_item_suspicious(item) ? "[SUSPICIOUS] " : "", m_game->m_client_list[give_h]->m_char_name, m_game->m_client_list[give_h]->m_ip_address, format_item_info(item), m_game->m_client_list[give_h]->m_map_name, m_game->m_client_list[give_h]->m_x, m_game->m_client_list[give_h]->m_y, m_game->m_client_list[recv_h]->m_char_name);
		break;

	case ItemLogAction::Drop:
		hb::logger::log<log_channel::drops>("{} IP({}) Drop {} at {}({},{})", m_game->m_client_list[give_h]->m_char_name, m_game->m_client_list[give_h]->m_ip_address, format_item_info(item), m_game->m_client_list[give_h]->m_map_name, m_game->m_client_list[give_h]->m_x, m_game->m_client_list[give_h]->m_y);
		break;

	case ItemLogAction::get:
		hb::logger::log<log_channel::drops>("{} IP({}) get {} at {}({},{})", m_game->m_client_list[give_h]->m_char_name, m_game->m_client_list[give_h]->m_ip_address, format_item_info(item), m_game->m_client_list[give_h]->m_map_name, m_game->m_client_list[give_h]->m_x, m_game->m_client_list[give_h]->m_y);
		break;

	case ItemLogAction::Make:
		hb::logger::log<log_channel::crafting>("{} IP({}) Make {} at {}({},{})", m_game->m_client_list[give_h]->m_char_name, m_game->m_client_list[give_h]->m_ip_address, format_item_info(item), m_game->m_client_list[give_h]->m_map_name, m_game->m_client_list[give_h]->m_x, m_game->m_client_list[give_h]->m_y);
		break;

	case ItemLogAction::Deplete:
		hb::logger::log<log_channel::items_misc>("{} IP({}) {} {} at {}({},{})", m_game->m_client_list[give_h]->m_char_name, m_game->m_client_list[give_h]->m_ip_address, "Deplete", format_item_info(item), m_game->m_client_list[give_h]->m_map_name, m_game->m_client_list[give_h]->m_x, m_game->m_client_list[give_h]->m_y);
		break;

	case ItemLogAction::Buy:
		hb::logger::log<log_channel::shop>("{} IP({}) {} {} at {}({},{})", m_game->m_client_list[give_h]->m_char_name, m_game->m_client_list[give_h]->m_ip_address, "Buy", format_item_info(item), m_game->m_client_list[give_h]->m_map_name, m_game->m_client_list[give_h]->m_x, m_game->m_client_list[give_h]->m_y);
		break;

	case ItemLogAction::Sell:
		hb::logger::log<log_channel::shop>("{} IP({}) {} {} at {}({},{})", m_game->m_client_list[give_h]->m_char_name, m_game->m_client_list[give_h]->m_ip_address, "Sell", format_item_info(item), m_game->m_client_list[give_h]->m_map_name, m_game->m_client_list[give_h]->m_x, m_game->m_client_list[give_h]->m_y);
		break;

	case ItemLogAction::Retrieve:
		hb::logger::log<log_channel::bank>("{} IP({}) {} {} at {}({},{})", m_game->m_client_list[give_h]->m_char_name, m_game->m_client_list[give_h]->m_ip_address, "Retrieve", format_item_info(item), m_game->m_client_list[give_h]->m_map_name, m_game->m_client_list[give_h]->m_x, m_game->m_client_list[give_h]->m_y);
		break;

	case ItemLogAction::Deposit:
		hb::logger::log<log_channel::bank>("{} IP({}) {} {} at {}({},{})", m_game->m_client_list[give_h]->m_char_name, m_game->m_client_list[give_h]->m_ip_address, "Deposit", format_item_info(item), m_game->m_client_list[give_h]->m_map_name, m_game->m_client_list[give_h]->m_x, m_game->m_client_list[give_h]->m_y);
		break;

	case ItemLogAction::UpgradeFail:
		hb::logger::log<log_channel::upgrades>("{} IP({}) Upgrade {} {} at {}({},{})", m_game->m_client_list[give_h]->m_char_name, m_game->m_client_list[give_h]->m_ip_address, false ? "Success" : "Fail", format_item_info(item), m_game->m_client_list[give_h]->m_map_name, m_game->m_client_list[give_h]->m_x, m_game->m_client_list[give_h]->m_y);
		break;

	case ItemLogAction::UpgradeSuccess:
		hb::logger::log<log_channel::upgrades>("{} IP({}) Upgrade {} {} at {}({},{})", m_game->m_client_list[give_h]->m_char_name, m_game->m_client_list[give_h]->m_ip_address, true ? "Success" : "Fail", format_item_info(item), m_game->m_client_list[give_h]->m_map_name, m_game->m_client_list[give_h]->m_x, m_game->m_client_list[give_h]->m_y);
		break;

	default:
		return false;
	}
	return true;
}

bool ItemManager::item_log(int action, int client_h, char* name, CItem* item)
{
	if (item == 0) return false;
	if (check_good_item(item) == false) return false;
	if (action != ItemLogAction::NewGenDrop)
	{
		if (m_game->m_client_list[client_h] == 0) return false;
	}
	char temp1[120];
	std::memset(temp1, 0, sizeof(temp1));
	if (m_game->m_client_list[client_h] != 0) m_game->m_client_list[client_h]->m_socket->get_peer_address(temp1);

	switch (action) {

	case ItemLogAction::NewGenDrop:
		hb::logger::log<log_channel::items_misc>("{} IP({}) {} {} at {}({},{})", name ? name : "Unknown", "", "NpcDrop", format_item_info(item), "", 0, 0);
		break;

	case ItemLogAction::SkillLearn:
	case ItemLogAction::MagicLearn:
		if (name == 0) return false;
		if (m_game->m_client_list[client_h] == 0) return false;
		hb::logger::log<log_channel::items_misc>("{} IP({}) {} {} at {}({},{})", m_game->m_client_list[client_h]->m_char_name, temp1, "Learn", format_item_info(item), m_game->m_client_list[client_h]->m_map_name, m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y);
		break;

	case ItemLogAction::SummonMonster:
		if (name == 0) return false;
		if (m_game->m_client_list[client_h] == 0) return false;
		hb::logger::log<log_channel::items_misc>("{} IP({}) {} {} at {}({},{})", m_game->m_client_list[client_h]->m_char_name, temp1, "Summon", format_item_info(item), m_game->m_client_list[client_h]->m_map_name, m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y);
		break;

	case ItemLogAction::Poisoned:
		if (m_game->m_client_list[client_h] == 0) return false;
		hb::logger::log<log_channel::items_misc>("{} IP({}) {} {} at {}({},{})", m_game->m_client_list[client_h]->m_char_name, temp1, "Poisoned", format_item_info(item), m_game->m_client_list[client_h]->m_map_name, m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y);
		break;

	case ItemLogAction::Repair:
		if (name == 0) return false;
		if (m_game->m_client_list[client_h] == 0) return false;
		hb::logger::log<log_channel::items_misc>("{} IP({}) {} {} at {}({},{})", m_game->m_client_list[client_h]->m_char_name, temp1, "Repair", format_item_info(item), m_game->m_client_list[client_h]->m_map_name, m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y);
		break;

	default:
		return false;
	}
	return true;
}

bool ItemManager::check_good_item(CItem* item)
{
	if (item == 0) return false;

	if (item->m_id_num == 90)
	{
		if (item->m_instance.count > 10000) return true;  // Gold  10000   .
		else return false;
	}
	switch (item->m_id_num) {
		// case 90: // Gold
	case 259:
	case 290:
	case 291:
	case 292:
	case 300:
	case 305:
	case 308:
	case 311:
	case 334:
	case 335:
	case 336:
	case 338:
	case 380:
	case 381:
	case 382:
	case 391:
	case 400:
	case 401:
	case 490:
	case 491:
	case 492:
	case 508:
	case 581:
	case 610:
	case 611:
	case 612:
	case 613:
	case 614:
	case 616:
	case 618:

	case 620:
	case 621:
	case 622:

	case 630:
	case 631:

	case 632:
	case 633:
	case 634:
	case 635:
	case 636:
	case 637:
	case 638:
	case 639:
	case 640:
	case 641:

	case 642:
	case 643:

	case 644:
	case 645:
	case 646:
	case 647:

	case 650:
	case 656:
	case 657:

	case 700:
	case 701:
	case 702:
	case 703:
	case 704:
	case 705:
	case 706:
	case 707:
	case 708:
	case 709:
	case 710:
	case 711:
	case 712:
	case 713:
	case 714:
	case 715:

	case 720:
	case 721:
	case 722:
	case 723:

	case 724:
	case 725:
	case 726:
	case 727:
	case 728:
	case 729:
	case 730:
	case 731:
	case 732:
	case 733:

	case 734:
	case 735:

	case 736:
	case 737:
	case 738:
	case 924:

		return true;
		break;
	default:
		if (!(item->has_special_attributes())) return false;
		else if (item->m_id_num > 30) return true;
		else return false;
	}
}

bool ItemManager::check_and_convert_plus_weapon_item(int client_h, int item_index)
{
	if (m_game->m_client_list[client_h] == 0) return false;
	if (m_game->m_client_list[client_h]->m_item_list[item_index] == 0) return false;

	switch (m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num) {
	case 4:  // Dagger +1
	case 9:  // Short Sword +1
	case 13: // Main Gauge +1
	case 16: // Gradius +1
	case 18: // Long Sword +1
	case 19: // Long Sword +2
	case 21: // Excaliber +1
	case 24: // Sabre +1
	case 26: // Scimitar +1
	case 27: // Scimitar +2
	case 29: // Falchoin +1
	case 30: // Falchion +2
	case 32: // Esterk +1
	case 33: // Esterk +2
	case 35: // Rapier +1
	case 36: // Rapier +2
	case 39: // Broad Sword +1
	case 40: // Broad Sword +2
	case 43: // Bastad Sword +1
	case 44: // Bastad Sword +2
	case 47: // Claymore +1
	case 48: // Claymore +2
	case 51: // Great Sword +1
	case 52: // Great Sword +2
	case 55: // Flameberge +1
	case 56: // Flameberge +2
	case 60: // Light Axe +1
	case 61: // Light Axe +2
	case 63: // Tomahoc +1
	case 64: // Tomohoc +2
	case 66: // Sexon Axe +1
	case 67: // Sexon Axe +2
	case 69: // Double Axe +1
	case 70: // Double Axe +2
	case 72: // War Axe +1
	case 73: // War Axe +2

	case 580: // Battle Axe +1
	case 581: // Battle Axe +2
	case 582: // Sabre +2
		return true;
		break;
	}
	return false;
}

void ItemManager::req_create_slate_handler(int client_h, char* data)
{
	int ret;
	char item_id[4], ctr[4];
	char slate_colour;
	bool is_slate_present = false;
	CItem* item;
	int slate_type, erase_req;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_on_server_change) return;

	for(int i = 0; i < 4; i++) {
		item_id[i] = 0;
		ctr[i] = 0;
	}
	const auto* pkt = hb::net::PacketCast<hb::net::PacketCommandCommonItems>(
		data, sizeof(hb::net::PacketCommandCommonItems));
	if (!pkt) return;

	// 14% chance of creating slates
	if (m_game->dice(1, 100) < static_cast<uint32_t>(m_game->m_slate_success_rate)) is_slate_present = true;

	try {
		// make sure slates really exist
		for(int i = 0; i < 4; i++) {
			item_id[i] = static_cast<char>(pkt->item_ids[i]);

			if (m_game->m_client_list[client_h]->m_item_list[item_id[i]] == 0 || item_id[i] > hb::shared::limits::MaxItems) {
				is_slate_present = false;
				m_game->send_notify_msg(0, client_h, Notify::SlateCreateFail, 0, 0, 0, 0);
				return;
			}

			//No duping
			if (m_game->m_client_list[client_h]->m_item_list[item_id[i]]->m_id_num == 868)
				ctr[0] = 1;
			else if (m_game->m_client_list[client_h]->m_item_list[item_id[i]]->m_id_num == 869)
				ctr[1] = 1;
			else if (m_game->m_client_list[client_h]->m_item_list[item_id[i]]->m_id_num == 870)
				ctr[2] = 1;
			else if (m_game->m_client_list[client_h]->m_item_list[item_id[i]]->m_id_num == 871)
				ctr[3] = 1;
		}
	}
	catch (...) {
		//Crash Hacker Caught
		is_slate_present = false;
		m_game->send_notify_msg(0, client_h, Notify::SlateCreateFail, 0, 0, 0, 0);
		hb::logger::warn<log_channel::security>("Slate hack: IP={} player={}, creating slates without required item", m_game->m_client_list[client_h]->m_ip_address, m_game->m_client_list[client_h]->m_char_name);
		m_game->delete_client(client_h, true, true);
		return;
	}

	// Are all 4 slates present ??
	if (ctr[0] != 1 || ctr[1] != 1 || ctr[2] != 1 || ctr[3] != 1) {
		is_slate_present = false;
		return;
	}

	// if we failed, kill everything
	if (!is_slate_present) {
		for(int i = 0; i < 4; i++) {
			if (m_game->m_client_list[client_h]->m_item_list[item_id[i]] != 0) {
				item_deplete_handler(client_h, item_id[i], false);
			}
		}
		m_game->send_notify_msg(0, client_h, Notify::SlateCreateFail, 0, 0, 0, 0);
		return;
	}

	// make the slates
	for(int i = 0; i < 4; i++) {
		if (m_game->m_client_list[client_h]->m_item_list[item_id[i]] != 0) {
			item_deplete_handler(client_h, item_id[i], false);
		}
	}

	item = new CItem;

	int i = m_game->dice(1, 1000);

	if (i < 50) { // Hp slate
		slate_type = 1;
		slate_colour = 32;
	}
	else if (i < 250) { // Bezerk slate
		slate_type = 2;
		slate_colour = 3;
	}
	else if (i < 750) { // Exp slate
		slate_type = 4;
		slate_colour = 7;
	}
	else if (i < 950) { // Mana slate
		slate_type = 3;
		slate_colour = 37;
	}
	else if (i < 1001) { // Hp slate
		slate_type = 1;
		slate_colour = 32;
	}

	// Notify client
	m_game->send_notify_msg(0, client_h, Notify::SlateCreateSuccess, slate_type, 0, 0, 0);

	// Create slates
	if (init_item_attr(item, 867) == false) {
		delete item;
		return;
	}
	else {
		item->set_touch_effect_type(TouchEffectType::ID);
		item->m_instance.touch_effect_value1 = static_cast<short>(m_game->dice(1, 100000));
		item->m_instance.touch_effect_value2 = static_cast<short>(m_game->dice(1, 100000));
		item->m_instance.touch_effect_value3 = (short)GameClock::GetTimeMS();

		item_log(ItemLogAction::get, client_h, -1, item);

		item->m_instance.special_effect_value2 = slate_type;
		item->m_instance.item_color = slate_colour;
		if (add_client_item_list(client_h, item, &erase_req)) {
			ret = send_item_notify_msg(client_h, Notify::ItemObtained, item, 0);
			switch (ret) {
			case sock::Event::QueueFull:
			case sock::Event::SocketError:
			case sock::Event::CriticalError:
			case sock::Event::SocketClosed:
				m_game->delete_client(client_h, true, true);
				return;
			}
		}
		else {
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, item);
			m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index,
				m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, item);
			ret = send_item_notify_msg(client_h, Notify::CannotCarryMoreItem, 0, 0);

			switch (ret) {
			case sock::Event::QueueFull:
			case sock::Event::SocketError:
			case sock::Event::CriticalError:
			case sock::Event::SocketClosed:
				m_game->delete_client(client_h, true, true);
				break;
			}
		}
	}
	return;
}

void ItemManager::set_slate_flag(int client_h, short type, bool flag)
{
	if (m_game->m_client_list[client_h] == 0) return;

	if (type == SlateClearNotify) {
		m_game->m_client_list[client_h]->m_status.slate_invincible = false;
		m_game->m_client_list[client_h]->m_status.slate_mana = false;
		m_game->m_client_list[client_h]->m_status.slate_exp = false;
		return;
	}

	if (flag) {
		if (type == 1) { // Invincible slate
			m_game->m_client_list[client_h]->m_status.slate_invincible = true;
		}
		else if (type == 3) { // Mana slate
			m_game->m_client_list[client_h]->m_status.slate_mana = true;
		}
		else if (type == 4) { // Exp slate
			m_game->m_client_list[client_h]->m_status.slate_exp = true;
		}
	}
	else {
		if (m_game->m_client_list[client_h]->m_status.slate_invincible) {
			m_game->m_client_list[client_h]->m_status.slate_invincible = false;
		}
		else if (m_game->m_client_list[client_h]->m_status.slate_mana) {
			m_game->m_client_list[client_h]->m_status.slate_mana = false;
		}
		else if (m_game->m_client_list[client_h]->m_status.slate_exp) {
			m_game->m_client_list[client_h]->m_status.slate_exp = false;
		}
	}

	m_game->send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
}

void ItemManager::clear_exchange_status(int to_h)
{
	if ((to_h <= 0) || (to_h >= MaxClients)) return;
	if (m_game->m_client_list[to_h] == 0) return;

	if (m_game->m_client_list[to_h]->m_exchange_name)
		m_game->send_notify_msg(0, to_h, Notify::cancel_exchange_item, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0);

	// m_game->m_client_list[to_h]->m_exchange_name    = false;
	m_game->m_client_list[to_h]->m_initial_check_time = false;
	m_game->m_client_list[to_h]->m_alter_item_drop_index = 0;
	//m_game->m_client_list[to_h]->m_exchange_item_index = -1;
	m_game->m_client_list[to_h]->m_exchange_h = 0;

	m_game->m_client_list[to_h]->m_is_exchange_mode = false;

	std::memset(m_game->m_client_list[to_h]->m_exchange_name, 0, sizeof(m_game->m_client_list[to_h]->m_exchange_name));

}

void ItemManager::cancel_exchange_item(int client_h)
{
	int ex_h;

	ex_h = m_game->m_client_list[client_h]->m_exchange_h;
	clear_exchange_status(ex_h);
	clear_exchange_status(client_h);
}

bool ItemManager::check_is_item_upgrade_success(int client_h, int item_index, int som_h, bool bonus)
{
	int value, prob, result;

	if (m_game->m_client_list[client_h]->m_item_list[som_h] == 0) return false;

	value = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus;

	switch (value) {
	case 0: prob = 30; break;  // +1 :90%     +1~+2
	case 1: prob = 25; break;  // +2 :80%      +3
	case 2: prob = 20; break;  // +3 :48%      +4 
	case 3: prob = 15; break;  // +4 :24%      +5
	case 4: prob = 10; break;  // +5 :9.6%     +6
	case 5: prob = 10; break;  // +6 :2.8%     +7
	case 6: prob = 8; break;  // +7 :0.57%    +8
	case 7: prob = 8; break;  // +8 :0.05%    +9
	case 8: prob = 5; break;  // +9 :0.004%   +10
	case 9: prob = 3; break;  // +10:0.00016%
	default: prob = 1; break;
	}

	if ((m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.custom_made) && (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2 > 100)) {
		if (prob > 20)
			prob += (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2 / 10);
		else if (prob > 7)
			prob += (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2 / 20);
		else
			prob += (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2 / 40);
	}
	if (bonus) prob *= 2;

	prob *= 100;
	result = m_game->dice(1, 10000);

	if (prob >= result) {
		item_log(ItemLogAction::UpgradeSuccess, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_index]);
		return true;
	}

	item_log(ItemLogAction::UpgradeFail, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_index]);

	return false;
}

void ItemManager::reload_item_configs()
{
	sqlite3* configDb = nullptr;
	std::string configDbPath;
	bool configDbCreated = false;
	if (!EnsureGameConfigDatabase(&configDb, configDbPath, &configDbCreated) || configDbCreated)
	{
		hb::logger::log("Item config reload failed: gamedata.db unavailable");
		return;
	}

	for(int i = 0; i < MaxItemTypes; i++)
	{
		if (m_game->m_item_config_list[i] != 0)
		{
			delete m_game->m_item_config_list[i];
			m_game->m_item_config_list[i] = 0;
		}
	}

	if (!LoadItemConfigs(configDb, m_game->m_item_config_list, MaxItemTypes))
	{
		hb::logger::log("Item config reload failed");
		CloseGameConfigDatabase(configDb);
		return;
	}

	CloseGameConfigDatabase(configDb);
	m_game->build_magic_manual_index();
	m_game->compute_config_hashes();
	hb::logger::log("Item configs reloaded successfully");
}

void ItemManager::request_item_upgrade_handler(int client_h, int item_index)
{
	int item_x, item_y, so_m, so_x, som_h, sox_h, value; // v2.172
	double v1, v2, v3;
	short item_upgrade = 2;

	//hbest
	int bugint = 0;

	if (m_game->m_client_list[client_h] == 0) return;
	if ((item_index < 0) || (item_index >= hb::shared::limits::MaxItems)) return;
	if (m_game->m_client_list[client_h]->m_item_list[item_index] == 0) return;

	value = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus;
	if (value >= 15 || value < 0) {
		m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 1, 0, 0, 0);
		return;
	}

	// Route upgrade by equipment type (replaces old category switch)
	auto upgrade_ep = m_game->m_client_list[client_h]->m_item_list[item_index]->get_equip_pos();
	auto upgrade_wc = m_game->m_client_list[client_h]->m_item_list[item_index]->get_weapon_class();
	auto upgrade_ist = m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_sub_type();
	int upgrade_route;
	if (upgrade_ist == hb::shared::item::item_sub_type::accessory)
		upgrade_route = 46; // pendants/accessories
	else if (upgrade_wc == hb::shared::item::weapon_class::bow)
		upgrade_route = 3;  // bows
	else if (upgrade_wc == hb::shared::item::weapon_class::wand)
		upgrade_route = 8;  // wands
	else if (upgrade_ep == EquipPos::LeftHand)
		upgrade_route = 5;  // shields
	else if (upgrade_ist == hb::shared::item::item_sub_type::weapon)
		upgrade_route = 1;  // melee weapons
	else if (upgrade_ist == hb::shared::item::item_sub_type::armor)
		upgrade_route = 6;  // armor
	else if (upgrade_ep == EquipPos::Back)
		upgrade_route = 13; // capes
	else
		upgrade_route = 0;

	switch (upgrade_route) {
	case 46: // Pendants/accessories
		if (m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_type() != hb::shared::item::item_type::equipment)
		{
			m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 2, 0, 0, 0);
			return; // Pendants are type Equip
		}
		if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_equip_pos < 11)
		{
			m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 2, 0, 0, 0);
			return; // Pendants are left finger or more
		}
		if (m_game->m_client_list[client_h]->m_item_list[item_index]->get_item_effect_type() != ItemEffectType::add_effect)
		{
			m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 2, 0, 0, 0);
			return; // Pendants are EffectType add_effect
		}
		switch (m_game->m_client_list[client_h]->m_item_list[item_index]->m_item_effect_value1) {
		default: // Other items are not upgradable
			m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 2, 0, 0, 0);
			return; // Pendants are EffectType 14

		case 16: // AngelicPendant(STR)
		case 17: // AngelicPendant(DEX)
		case 18: // AngelicPendant(INT)
		case 19: // AngelicPendant(MAG)
			if (m_game->m_client_list[client_h]->m_gizon_item_upgrade_left <= 0)
			{
				m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 3, 0, 0, 0);
				return;
			}
			if (value >= 10)
			{
				m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 3, 0, 0, 0);
				return;
			}
			switch (value) {
			case 0:	item_upgrade = 10; break;
			case 1: item_upgrade = 11; break;
			case 2: item_upgrade = 13; break;
			case 3: item_upgrade = 16; break;
			case 4: item_upgrade = 20; break;
			case 5: item_upgrade = 25; break;
			case 6: item_upgrade = 31; break;
			case 7: item_upgrade = 38; break;
			case 8: item_upgrade = 46; break;
			case 9: item_upgrade = 55; break;
			default:
				m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 3, 0, 0, 0);
				return;
				break;
			}
			if ((m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value1 != m_game->m_client_list[client_h]->m_char_id_num1)
				|| (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value2 != m_game->m_client_list[client_h]->m_char_id_num2)
				|| (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value3 != m_game->m_client_list[client_h]->m_char_id_num3))
			{
				m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 2, 0, 0, 0);
				return;
			}
			if ((m_game->m_client_list[client_h]->m_gizon_item_upgrade_left - item_upgrade) < 0)
			{
				m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 3, 0, 0, 0);
				return;
			}
			int dice_pta = m_game->dice(1, 100);
			if (dice_pta <= 70)
			{
				m_game->m_client_list[client_h]->m_gizon_item_upgrade_left -= item_upgrade;
				m_game->send_notify_msg(0, client_h, Notify::GizonItemUpgradeLeft, m_game->m_client_list[client_h]->m_gizon_item_upgrade_left, 0, 0, 0);
				value++;
				if (value > 10) value = 10;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus = value;

				m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
				item_log(ItemLogAction::UpgradeSuccess, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_index]);
			}
			else
			{
				m_game->m_client_list[client_h]->m_gizon_item_upgrade_left--;
				m_game->send_notify_msg(0, client_h, Notify::GizonItemUpgradeLeft, m_game->m_client_list[client_h]->m_gizon_item_upgrade_left, 0, 0, 0);
				m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 3, 0, 0, 0);
			}
			return;
			break;
		}
		break;

	case 1: // weapons upgrade
		switch (m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num) {
		case 703:
		case 709: // DarkKnightFlameberge 
		case 718: // DarkKnightGreatSword
		case 727: // DarkKnightFlamebergW
		case 736:
		case 737: // DarkKnightAxe
		case 745: // DarkKnightHammer
			if (m_game->m_client_list[client_h]->m_gizon_item_upgrade_left <= 0)
			{
				m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 3, 0, 0, 0);
				return;
			}

			item_upgrade = (value * (value + 6) / 8) + 2;

			if ((m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value1 != m_game->m_client_list[client_h]->m_char_id_num1) ||
				(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value2 != m_game->m_client_list[client_h]->m_char_id_num2) ||
				(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value3 != m_game->m_client_list[client_h]->m_char_id_num3))
			{
				if (value != 0) {
					m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 2, 0, 0, 0);
					return;
				}
			}

			if ((m_game->m_client_list[client_h]->m_gizon_item_upgrade_left - item_upgrade) < 0)
			{
				m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 3, 0, 0, 0);
				return;
			}

			m_game->m_client_list[client_h]->m_gizon_item_upgrade_left -= item_upgrade;

			m_game->send_notify_msg(0, client_h, Notify::GizonItemUpgradeLeft, m_game->m_client_list[client_h]->m_gizon_item_upgrade_left, 0, 0, 0);

			if ((value == 0) && m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num == 703)
			{
				item_x = m_game->m_client_list[client_h]->m_item_pos_list[item_index].x;
				item_y = m_game->m_client_list[client_h]->m_item_pos_list[item_index].y;

				delete m_game->m_client_list[client_h]->m_item_list[item_index];
				m_game->m_client_list[client_h]->m_item_list[item_index] = 0;

				m_game->m_client_list[client_h]->m_item_list[item_index] = new CItem;
				m_game->m_client_list[client_h]->m_item_pos_list[item_index].x = item_x;
				m_game->m_client_list[client_h]->m_item_pos_list[item_index].y = item_y;

				if (init_item_attr(m_game->m_client_list[client_h]->m_item_list[item_index], 709) == false) {
					m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
					return;
				}

				m_game->m_client_list[client_h]->m_item_list[item_index]->set_touch_effect_type(TouchEffectType::UniqueOwner);
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value1 = m_game->m_client_list[client_h]->m_char_id_num1;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value2 = m_game->m_client_list[client_h]->m_char_id_num2;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value3 = m_game->m_client_list[client_h]->m_char_id_num3;

				value += 1;
				if (value > 15) value = 15;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus = value;


				m_game->send_gizon_item_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
				item_log(ItemLogAction::UpgradeSuccess, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_index]);
				break;

			}
			else if ((value == 0) && (m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num == 709))
			{

				item_x = m_game->m_client_list[client_h]->m_item_pos_list[item_index].x;
				item_y = m_game->m_client_list[client_h]->m_item_pos_list[item_index].y;

				delete m_game->m_client_list[client_h]->m_item_list[item_index];
				m_game->m_client_list[client_h]->m_item_list[item_index] = 0;

				m_game->m_client_list[client_h]->m_item_list[item_index] = new CItem;
				m_game->m_client_list[client_h]->m_item_pos_list[item_index].x = item_x;
				m_game->m_client_list[client_h]->m_item_pos_list[item_index].y = item_y;

				if (init_item_attr(m_game->m_client_list[client_h]->m_item_list[item_index], 709) == false) {
					m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
					return;
				}

				m_game->m_client_list[client_h]->m_item_list[item_index]->set_touch_effect_type(TouchEffectType::UniqueOwner);
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value1 = m_game->m_client_list[client_h]->m_char_id_num1;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value2 = m_game->m_client_list[client_h]->m_char_id_num2;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value3 = m_game->m_client_list[client_h]->m_char_id_num3;

				value += 1;
				if (value > 15) value = 15;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus = value;


				m_game->send_gizon_item_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);

				item_log(ItemLogAction::UpgradeSuccess, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_index]);
				break;
			}
			else if ((value == 0) && (m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num == 745))
			{

				item_x = m_game->m_client_list[client_h]->m_item_pos_list[item_index].x;
				item_y = m_game->m_client_list[client_h]->m_item_pos_list[item_index].y;

				delete m_game->m_client_list[client_h]->m_item_list[item_index];
				m_game->m_client_list[client_h]->m_item_list[item_index] = 0;

				m_game->m_client_list[client_h]->m_item_list[item_index] = new CItem;
				m_game->m_client_list[client_h]->m_item_pos_list[item_index].x = item_x;
				m_game->m_client_list[client_h]->m_item_pos_list[item_index].y = item_y;

				if (init_item_attr(m_game->m_client_list[client_h]->m_item_list[item_index], 745) == false) {
					m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
					return;
				}

				m_game->m_client_list[client_h]->m_item_list[item_index]->set_touch_effect_type(TouchEffectType::UniqueOwner);
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value1 = m_game->m_client_list[client_h]->m_char_id_num1;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value2 = m_game->m_client_list[client_h]->m_char_id_num2;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value3 = m_game->m_client_list[client_h]->m_char_id_num3;

				value += 1;
				if (value > 15) value = 15;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus = value;


				m_game->send_gizon_item_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);

				item_log(ItemLogAction::UpgradeSuccess, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_index]);
				break;
			}
			else if ((value == 0) && (m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num == 737))
			{

				item_x = m_game->m_client_list[client_h]->m_item_pos_list[item_index].x;
				item_y = m_game->m_client_list[client_h]->m_item_pos_list[item_index].y;

				delete m_game->m_client_list[client_h]->m_item_list[item_index];
				m_game->m_client_list[client_h]->m_item_list[item_index] = 0;

				m_game->m_client_list[client_h]->m_item_list[item_index] = new CItem;
				m_game->m_client_list[client_h]->m_item_pos_list[item_index].x = item_x;
				m_game->m_client_list[client_h]->m_item_pos_list[item_index].y = item_y;

				if (init_item_attr(m_game->m_client_list[client_h]->m_item_list[item_index], 737) == false) {
					m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
					return;
				}

				m_game->m_client_list[client_h]->m_item_list[item_index]->set_touch_effect_type(TouchEffectType::UniqueOwner);
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value1 = m_game->m_client_list[client_h]->m_char_id_num1;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value2 = m_game->m_client_list[client_h]->m_char_id_num2;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value3 = m_game->m_client_list[client_h]->m_char_id_num3;

				value += 1;
				if (value > 15) value = 15;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus = value;


				m_game->send_gizon_item_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);

				item_log(ItemLogAction::UpgradeSuccess, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_index]);
				break;
			}
			else
			{
				value += 1;
				if (value > 15) value = 15;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus = value;

				m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
				item_log(ItemLogAction::UpgradeSuccess, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_index]);
			}
			break;

		default:

			if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.prefix_type == static_cast<uint8_t>(hb::shared::item::AttributePrefixType::Ancient)) {
					m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 2, 0, 0, 0);
					return;
			}
			so_x = so_m = 0;
			for(int i = 0; i < hb::shared::limits::MaxItems; i++)
				if (m_game->m_client_list[client_h]->m_item_list[i] != 0) {
					switch (m_game->m_client_list[client_h]->m_item_list[i]->m_id_num) {
					case 656: so_x++; sox_h = i; break;
					case 657: so_m++; som_h = i; break;
					}
				}
			if (so_x > 0) {
				if (check_is_item_upgrade_success(client_h, item_index, sox_h) == false) {
					m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
					value = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus; // v2.172
					if (value >= 1) item_deplete_handler(client_h, item_index, false);
					item_deplete_handler(client_h, sox_h, false);
					return;
				}

				if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.custom_made) {
					value++;
					if (value > 10)
						value = 10;
					else {
						m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus = value;
		
						item_deplete_handler(client_h, sox_h, false);
					}
				}
				else {
					value++;
					if (value > 7)
						value = 7;
					else {
						m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus = value;
		
						item_deplete_handler(client_h, sox_h, false);
					}
				}
			}

			m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
			break;
		}
		break;

	case 3:
		m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
		break;

	case 5:
		if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.prefix_type == static_cast<uint8_t>(hb::shared::item::AttributePrefixType::Strong)) {
				m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 2, 0, 0, 0);
				return;
		}
		switch (m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num) {
		case 620:
			m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 2, 0, 0, 0);
			return;
		default: break;
		}

		so_x = so_m = 0;
		for(int i = 0; i < hb::shared::limits::MaxItems; i++)
			if (m_game->m_client_list[client_h]->m_item_list[i] != 0) {
				switch (m_game->m_client_list[client_h]->m_item_list[i]->m_id_num) {
				case 656: so_x++; sox_h = i; break;
				case 657: so_m++; som_h = i; break;
				}
			}

		if (so_m > 0) {
			if (check_is_item_upgrade_success(client_h, item_index, som_h, true) == false) {
				m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
				value = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus; // v2.172
				if (value >= 1) item_deplete_handler(client_h, item_index, false);
				item_deplete_handler(client_h, som_h, false);
				return;
			}

			value++;
			if (value > 10)
				value = 10;
			else {
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus = value;


				if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.custom_made) {
					// +20%
					v1 = (double)m_game->m_client_list[client_h]->m_item_list[item_index]->m_durability;
					v2 = 0.2f * v1;
					v3 = v1 + v2;
				}
				else {
					// +15%
					v1 = (double)m_game->m_client_list[client_h]->m_item_list[item_index]->m_durability;
					v2 = 0.15f * v1;
					v3 = v1 + v2;
				}
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1 = (short)v3;
				if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1 < 0)
					m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_durability;

				m_game->m_client_list[client_h]->m_item_list[item_index]->m_durability = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1;
				item_deplete_handler(client_h, som_h, false);
			}
		}
		m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index], m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1, m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2);
		break;

	case 6: // armors upgrade
		switch (m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num) {
		case 621:
		case 622:

		case 700:
		case 701:
		case 702:
		case 704:
		case 706:
		case 707:
		case 708:
		case 710:
		case 711:
		case 712:
		case 713:
		case 724:
		case 725:
		case 726:
		case 728:
		case 729:
		case 730:
		case 731:
			m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 2, 0, 0, 0);
			return;

		default:
			if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.prefix_type == static_cast<uint8_t>(hb::shared::item::AttributePrefixType::Strong)) {
					m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 2, 0, 0, 0);
					return;
			}
			so_x = so_m = 0;
			for(int i = 0; i < hb::shared::limits::MaxItems; i++)
				if (m_game->m_client_list[client_h]->m_item_list[i] != 0) {
					switch (m_game->m_client_list[client_h]->m_item_list[i]->m_id_num) {
					case 656: so_x++; sox_h = i; break;
					case 657: so_m++; som_h = i; break;
					}
				}
			if (so_m > 0) {
				if (check_is_item_upgrade_success(client_h, item_index, som_h, true) == false) {
					m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
					value = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus;
					if (value >= 1) item_deplete_handler(client_h, item_index, false);
					item_deplete_handler(client_h, som_h, false);
					return;
				}
				value++;
				if (value > 10)
					value = 10;
				else {
					m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus = value;
	

					if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.custom_made) {
						v1 = (double)m_game->m_client_list[client_h]->m_item_list[item_index]->m_durability;
						v2 = 0.2f * v1;
						v3 = v1 + v2;
					}
					else {
						v1 = (double)m_game->m_client_list[client_h]->m_item_list[item_index]->m_durability;
						v2 = 0.15f * v1;
						v3 = v1 + v2;
					}
					m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1 = (short)v3;
					if (m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1 < 0)
						m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1 = m_game->m_client_list[client_h]->m_item_list[item_index]->m_durability;

					m_game->m_client_list[client_h]->m_item_list[item_index]->m_durability = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1;
					item_deplete_handler(client_h, som_h, false);
				}
			}
			m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index], m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value1, m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.special_effect_value2);
			break;
		}
		break;

	case 8: // wands upgrade 
		switch (m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num) {
		case 291: // MagicWand(LLF)

		case 714:
		case 732:
		case 738:
		case 746:

			if ((m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value1 != m_game->m_client_list[client_h]->m_char_id_num1) ||
				(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value2 != m_game->m_client_list[client_h]->m_char_id_num2) ||
				(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value3 != m_game->m_client_list[client_h]->m_char_id_num3))
			{
				if (value != 0) {
					m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 2, 0, 0, 0);
					return;
				}
			}

			if (m_game->m_client_list[client_h]->m_gizon_item_upgrade_left <= 0)
			{
				m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 3, 0, 0, 0);
				return;
			}
			item_upgrade = (value * (value + 6) / 8) + 2;

			if ((m_game->m_client_list[client_h]->m_gizon_item_upgrade_left - item_upgrade) < 0)
			{
				m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 3, 0, 0, 0);
				return;
			}

			m_game->m_client_list[client_h]->m_gizon_item_upgrade_left -= item_upgrade;
			m_game->send_notify_msg(0, client_h, Notify::GizonItemUpgradeLeft, m_game->m_client_list[client_h]->m_gizon_item_upgrade_left, 0, 0, 0);

			if (value == 0) {
				m_game->m_client_list[client_h]->m_item_list[item_index]->set_touch_effect_type(TouchEffectType::UniqueOwner);
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value1 = m_game->m_client_list[client_h]->m_char_id_num1;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value2 = m_game->m_client_list[client_h]->m_char_id_num2;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value3 = m_game->m_client_list[client_h]->m_char_id_num3;
			}

			if ((value == 11) && ((m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num == 714) || (m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num == 738)))
			{
				item_x = m_game->m_client_list[client_h]->m_item_pos_list[item_index].x;
				item_y = m_game->m_client_list[client_h]->m_item_pos_list[item_index].y;

				delete m_game->m_client_list[client_h]->m_item_list[item_index];
				m_game->m_client_list[client_h]->m_item_list[item_index] = 0;

				m_game->m_client_list[client_h]->m_item_list[item_index] = new CItem;

				m_game->m_client_list[client_h]->m_item_pos_list[item_index].x = item_x;
				m_game->m_client_list[client_h]->m_item_pos_list[item_index].y = item_y;

				if (init_item_attr(m_game->m_client_list[client_h]->m_item_list[item_index], 738) == false) {
					m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
					return;
				}

				m_game->m_client_list[client_h]->m_item_list[item_index]->set_touch_effect_type(TouchEffectType::UniqueOwner);
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value1 = m_game->m_client_list[client_h]->m_char_id_num1;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value2 = m_game->m_client_list[client_h]->m_char_id_num2;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value3 = m_game->m_client_list[client_h]->m_char_id_num3;

				value += 1;
				if (value > 15) value = 15;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus = value;


				m_game->send_gizon_item_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
				item_log(ItemLogAction::UpgradeSuccess, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_index]);
				break;

			}
			else if ((value == 15) && (m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num == 738))
			{
				item_x = m_game->m_client_list[client_h]->m_item_pos_list[item_index].x;
				item_y = m_game->m_client_list[client_h]->m_item_pos_list[item_index].y;

				delete m_game->m_client_list[client_h]->m_item_list[item_index];
				m_game->m_client_list[client_h]->m_item_list[item_index] = 0;

				m_game->m_client_list[client_h]->m_item_list[item_index] = new CItem;

				m_game->m_client_list[client_h]->m_item_pos_list[item_index].x = item_x;
				m_game->m_client_list[client_h]->m_item_pos_list[item_index].y = item_y;

				if (init_item_attr(m_game->m_client_list[client_h]->m_item_list[item_index], 746) == false) {
					m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
					return;
				}

				m_game->m_client_list[client_h]->m_item_list[item_index]->set_touch_effect_type(TouchEffectType::UniqueOwner);
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value1 = m_game->m_client_list[client_h]->m_char_id_num1;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value2 = m_game->m_client_list[client_h]->m_char_id_num2;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value3 = m_game->m_client_list[client_h]->m_char_id_num3;

				value += 1;
				if (value > 15) value = 15;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus = value;


				m_game->send_gizon_item_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
				item_log(ItemLogAction::UpgradeSuccess, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_index]);
				break;

			}
			else if ((value == 15) && (m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num == 746))
			{
				item_x = m_game->m_client_list[client_h]->m_item_pos_list[item_index].x;
				item_y = m_game->m_client_list[client_h]->m_item_pos_list[item_index].y;

				delete m_game->m_client_list[client_h]->m_item_list[item_index];
				m_game->m_client_list[client_h]->m_item_list[item_index] = 0;

				m_game->m_client_list[client_h]->m_item_list[item_index] = new CItem;

				m_game->m_client_list[client_h]->m_item_pos_list[item_index].x = item_x;
				m_game->m_client_list[client_h]->m_item_pos_list[item_index].y = item_y;

				if (init_item_attr(m_game->m_client_list[client_h]->m_item_list[item_index], 892) == false) {
					m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
					return;
				}

				m_game->m_client_list[client_h]->m_item_list[item_index]->set_touch_effect_type(TouchEffectType::UniqueOwner);
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value1 = m_game->m_client_list[client_h]->m_char_id_num1;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value2 = m_game->m_client_list[client_h]->m_char_id_num2;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value3 = m_game->m_client_list[client_h]->m_char_id_num3;

				value += 1;
				if (value > 15) value = 15;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus = value;


				m_game->send_gizon_item_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
				item_log(ItemLogAction::UpgradeSuccess, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_index]);
				break;

			}
			else
			{
				value += 1;
				if (value > 15) value = 15;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus = value;

				m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
				item_log(ItemLogAction::UpgradeSuccess, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_index]);
				break;
			}

		default:
			so_x = so_m = 0;
			for(int i = 0; i < hb::shared::limits::MaxItems; i++)
				if (m_game->m_client_list[client_h]->m_item_list[i] != 0) {
					switch (m_game->m_client_list[client_h]->m_item_list[i]->m_id_num) {
					case 656: so_x++; sox_h = i; break;
					case 657: so_m++; som_h = i; break;
					}
				}
			if (so_x > 0) {
				if (check_is_item_upgrade_success(client_h, item_index, sox_h) == false) {
					m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
					value = m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus; // v2.172
					if (value >= 1) item_deplete_handler(client_h, item_index, false);
					item_deplete_handler(client_h, sox_h, false);
					return;
				}

				value++;
				if (value > 7)
					value = 7;
				else {
					m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.enchant_bonus = value;
	
					item_deplete_handler(client_h, sox_h, false);
				}
			}

			m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);

			break;
		}
		break;

		//hbest hero cape upgrade
	case 13:
		switch (m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num) {
		case 400:
		case 401:
			so_x = so_m = 0;
			for(int i = 0; i < hb::shared::limits::MaxItems; i++)
				if (m_game->m_client_list[client_h]->m_item_list[i] != 0) {
					switch (m_game->m_client_list[client_h]->m_item_list[i]->m_id_num) {
					case 656: so_x++; sox_h = i; break;
					case 657: so_m++; som_h = i; break;
					}
				}

			if (so_m < 1) {
				return;
			}

			bugint = m_game->m_client_list[client_h]->m_item_list[item_index]->m_id_num;
			if ((m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value1 != m_game->m_client_list[client_h]->m_char_id_num1) ||
				(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value2 != m_game->m_client_list[client_h]->m_char_id_num2) ||
				(m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value3 != m_game->m_client_list[client_h]->m_char_id_num3))
			{
				m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 2, 0, 0, 0);
				return;
			}

			if ((m_game->m_client_list[client_h]->m_contribution < 50) || (m_game->m_client_list[client_h]->m_enemy_kill_count < 50))
			{
				m_game->send_notify_msg(0, client_h, Notify::ItemUpgradeFail, 3, 0, 0, 0);
				return;
			}

			m_game->m_client_list[client_h]->m_contribution -= 50;
			m_game->m_client_list[client_h]->m_enemy_kill_count -= 50;
			m_game->send_notify_msg(0, client_h, Notify::EnemyKills, m_game->m_client_list[client_h]->m_enemy_kill_count, 0, 0, 0);

			if (value == 0)
			{
				item_x = m_game->m_client_list[client_h]->m_item_pos_list[item_index].x;
				item_y = m_game->m_client_list[client_h]->m_item_pos_list[item_index].y;

				delete m_game->m_client_list[client_h]->m_item_list[item_index];
				m_game->m_client_list[client_h]->m_item_list[item_index] = 0;

				m_game->m_client_list[client_h]->m_item_list[item_index] = new CItem;

				m_game->m_client_list[client_h]->m_item_pos_list[item_index].x = item_x;
				m_game->m_client_list[client_h]->m_item_pos_list[item_index].y = item_y;

				if (bugint == 400) {
					if (init_item_attr(m_game->m_client_list[client_h]->m_item_list[item_index], 427) == false) {
						m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
						return;
					}
				}
				else {
					if (init_item_attr(m_game->m_client_list[client_h]->m_item_list[item_index], 428) == false) {
						m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
						return;
					}
				}

				m_game->m_client_list[client_h]->m_item_list[item_index]->set_touch_effect_type(TouchEffectType::UniqueOwner);
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value1 = m_game->m_client_list[client_h]->m_char_id_num1;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value2 = m_game->m_client_list[client_h]->m_char_id_num2;
				m_game->m_client_list[client_h]->m_item_list[item_index]->m_instance.touch_effect_value3 = m_game->m_client_list[client_h]->m_char_id_num3;

				item_deplete_handler(client_h, som_h, false);

				m_game->send_gizon_item_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
				item_log(ItemLogAction::UpgradeSuccess, client_h, (int)-1, m_game->m_client_list[client_h]->m_item_list[item_index]);
				break;

			}

		default: break;
		}
		break;

	default:
		m_game->send_item_attribute_change(client_h, item_index, m_game->m_client_list[client_h]->m_item_list[item_index]);
		break;
	}
}

char ItemManager::check_hero_item_equipped(int client_h)
{
	short hero_leggings, hero_hauberk, hero_armor, hero_helm;

	if (m_game->m_client_list[client_h] == 0) return 0;

	hero_helm = m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::Head)];
	hero_armor = m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::Body)];
	hero_hauberk = m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::Arms)];
	hero_leggings = m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::Leggings)];

	if ((hero_helm < 0) || (hero_leggings < 0) || (hero_armor < 0) || (hero_hauberk < 0)) return 0;

	if (m_game->m_client_list[client_h]->m_item_list[hero_helm] == 0) return 0;
	if (m_game->m_client_list[client_h]->m_item_list[hero_leggings] == 0) return 0;
	if (m_game->m_client_list[client_h]->m_item_list[hero_armor] == 0) return 0;
	if (m_game->m_client_list[client_h]->m_item_list[hero_hauberk] == 0) return 0;

	if ((m_game->m_client_list[client_h]->m_item_list[hero_helm]->m_id_num == 403) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_armor]->m_id_num == 411) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_hauberk]->m_id_num == 419) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_leggings]->m_id_num == 423)) return 1;

	if ((m_game->m_client_list[client_h]->m_item_list[hero_helm]->m_id_num == 407) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_armor]->m_id_num == 415) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_hauberk]->m_id_num == 419) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_leggings]->m_id_num == 423)) return 2;

	if ((m_game->m_client_list[client_h]->m_item_list[hero_helm]->m_id_num == 404) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_armor]->m_id_num == 412) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_hauberk]->m_id_num == 420) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_leggings]->m_id_num == 424)) return 1;

	if ((m_game->m_client_list[client_h]->m_item_list[hero_helm]->m_id_num == 408) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_armor]->m_id_num == 416) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_hauberk]->m_id_num == 420) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_leggings]->m_id_num == 424)) return 2;

	if ((m_game->m_client_list[client_h]->m_item_list[hero_helm]->m_id_num == 405) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_armor]->m_id_num == 413) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_hauberk]->m_id_num == 421) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_leggings]->m_id_num == 425)) return 1;

	if ((m_game->m_client_list[client_h]->m_item_list[hero_helm]->m_id_num == 409) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_armor]->m_id_num == 417) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_hauberk]->m_id_num == 421) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_leggings]->m_id_num == 425)) return 2;

	if ((m_game->m_client_list[client_h]->m_item_list[hero_helm]->m_id_num == 406) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_armor]->m_id_num == 414) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_hauberk]->m_id_num == 422) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_leggings]->m_id_num == 426)) return 1;

	if ((m_game->m_client_list[client_h]->m_item_list[hero_helm]->m_id_num == 410) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_armor]->m_id_num == 418) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_hauberk]->m_id_num == 422) &&
		(m_game->m_client_list[client_h]->m_item_list[hero_leggings]->m_id_num == 426)) return 2;

	return 0;
}

bool ItemManager::plant_seed_bag(int map_index, int dX, int dY, int item_effect_value1, int item_effect_value2, int client_h)
{
	int naming_value, tX, tY;
	short owner_h;
	char owner_type, npc_name[hb::shared::limits::NpcNameLen], name[hb::shared::limits::NpcNameLen], npc_waypoint_index[11];
	int ret;

	if (m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_total_agriculture >= 200) {
		m_game->send_notify_msg(0, client_h, Notify::NoMoreAgriculture, 0, 0, 0, 0);
		return false;
	}

	if (item_effect_value2 > m_game->m_client_list[client_h]->m_skill_mastery[2]) {
		m_game->send_notify_msg(0, client_h, Notify::AgricultureSkillLimit, 0, 0, 0, 0);
		return false;
	}

	naming_value = m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_empty_naming_value();

	if (naming_value == -1) {
	}
	else {
		m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
		if (owner_h != 0 && owner_h == hb::shared::owner_class::Npc && m_game->m_npc_list[owner_h]->m_action_limit == 5) {
			m_game->send_notify_msg(0, client_h, Notify::AgricultureNoArea, 0, 0, 0, 0);
			return false;
		}
		else {
			if (m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_is_farm(dX, dY) == false) {
				m_game->send_notify_msg(0, client_h, Notify::AgricultureNoArea, 0, 0, 0, 0);
				return false;
			}

			int npc_config_id = m_game->get_npc_config_id_by_name("Crops");
			std::memset(name, 0, sizeof(name));
			std::snprintf(name, sizeof(name), "XX%d", naming_value);
			name[0] = '_';
			name[1] = map_index + 65;

			std::memset(npc_waypoint_index, 0, sizeof(npc_waypoint_index));
			tX = dX;
			tY = dY;

			ret = m_game->create_new_npc(npc_config_id, name, m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_name, 0, 0, MoveType::Random, &tX, &tY, npc_waypoint_index, 0, 0, 0, false, true);
			if (ret == false) {
				m_game->m_map_list[map_index]->set_naming_value_empty(naming_value);
			}
			else {
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX, tY);
				if (m_game->m_npc_list[owner_h] == 0) return 0;
				m_game->m_npc_list[owner_h]->m_crop_type = item_effect_value1;
				switch (item_effect_value1) {
				case 1: m_game->m_npc_list[owner_h]->m_crop_skill = item_effect_value2; break;
				case 2: m_game->m_npc_list[owner_h]->m_crop_skill = item_effect_value2; break;
				case 3: m_game->m_npc_list[owner_h]->m_crop_skill = item_effect_value2; break;
				case 4: m_game->m_npc_list[owner_h]->m_crop_skill = item_effect_value2; break;
				case 5: m_game->m_npc_list[owner_h]->m_crop_skill = item_effect_value2; break;
				case 6: m_game->m_npc_list[owner_h]->m_crop_skill = item_effect_value2; break;
				case 7: m_game->m_npc_list[owner_h]->m_crop_skill = item_effect_value2; break;
				case 8: m_game->m_npc_list[owner_h]->m_crop_skill = item_effect_value2; break;
				case 9: m_game->m_npc_list[owner_h]->m_crop_skill = item_effect_value2; break;
				case 10: m_game->m_npc_list[owner_h]->m_crop_skill = item_effect_value2; break;
				case 11: m_game->m_npc_list[owner_h]->m_crop_skill = item_effect_value2; break;
				case 12: m_game->m_npc_list[owner_h]->m_crop_skill = item_effect_value2; break;
				case 13: m_game->m_npc_list[owner_h]->m_crop_skill = item_effect_value2; break;
				default: m_game->m_npc_list[owner_h]->m_crop_skill = 100; break;
				}
				m_game->m_npc_list[owner_h]->m_appearance.special_frame = 1;
				m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Npc, MsgId::EventLog, MsgType::Confirm, 0, 0, 0);
				hb::logger::log("Agriculture: skill={} type={} plant={} at ({},{}) total={}", m_game->m_npc_list[owner_h]->m_crop_skill, m_game->m_npc_list[owner_h]->m_crop_type, npc_name, tX, tY, m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_total_agriculture);
				return true;
			}
		}
	}
	return false;
}

void ItemManager::request_repair_all_items_handler(int client_h)
{
	int price;
	double d1, d2, d3;
	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;

	m_game->m_client_list[client_h]->total_item_repair = 0;

	for(int i = 0; i < hb::shared::limits::MaxItems; i++) {
		if (m_game->m_client_list[client_h]->m_item_list[i] != 0) {

			if (m_game->m_client_list[client_h]->m_item_list[i]->m_durability > 0)
			{
				if (m_game->m_client_list[client_h]->m_item_list[i]->m_instance.cur_durability == m_game->m_client_list[client_h]->m_item_list[i]->m_durability)
					continue;
				if (m_game->m_client_list[client_h]->m_item_list[i]->m_instance.cur_durability <= 0)
					price = (m_game->m_client_list[client_h]->m_item_list[i]->m_sell_price / 2);
				else
				{
					d1 = (double)(m_game->m_client_list[client_h]->m_item_list[i]->m_instance.cur_durability);
					if (m_game->m_client_list[client_h]->m_item_list[i]->m_durability != 0)
						d2 = (double)(m_game->m_client_list[client_h]->m_item_list[i]->m_durability);
					else
						d2 = (double)1.0f;
					d3 = (double)((d1 / d2) * 0.5f);
					d2 = (double)(m_game->m_client_list[client_h]->m_item_list[i]->m_sell_price);
					d3 = (d3 * d2);
					price = ((m_game->m_client_list[client_h]->m_item_list[i]->m_sell_price / 2) - static_cast<int32_t>(d3));
				}
				m_game->m_client_list[client_h]->m_repair_all[m_game->m_client_list[client_h]->total_item_repair].index = i;
				m_game->m_client_list[client_h]->m_repair_all[m_game->m_client_list[client_h]->total_item_repair].price = price;
				m_game->m_client_list[client_h]->total_item_repair++;
			}
		}
	}
	m_game->send_notify_msg(0, client_h, Notify::RepairAllPrices, 0, 0, 0, 0);
}

void ItemManager::request_repair_all_items_delete_handler(int client_h, int index)
{
	
	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;

	for(int i = index; i < m_game->m_client_list[client_h]->total_item_repair; i++) {
		m_game->m_client_list[client_h]->m_repair_all[i] = m_game->m_client_list[client_h]->m_repair_all[i + 1];
	}
	m_game->m_client_list[client_h]->total_item_repair--;
	m_game->send_notify_msg(0, client_h, Notify::RepairAllPrices, 0, 0, 0, 0);
}

void ItemManager::request_repair_all_items_confirm_handler(int client_h)
{
	int      ret, totalPrice = 0;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;
	if (m_game->m_client_list[client_h]->m_is_processing_allowed == false) return;

	for(int i = 0; i < m_game->m_client_list[client_h]->total_item_repair; i++) {
		totalPrice += m_game->m_client_list[client_h]->m_repair_all[i].price;
	}

	if (get_item_count_by_id(client_h, hb::shared::item::ItemId::Gold) < static_cast<uint64_t>(totalPrice))
	{
		{
			hb::net::PacketNotifyNotEnoughGold pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = Notify::NotEnoughGold;
			pkt.item_index = 0;
			ret = m_game->m_client_list[client_h]->m_socket->send_msg(reinterpret_cast<char*>(&pkt), sizeof(pkt));
		}
		switch (ret) {
		case sock::Event::QueueFull:
		case sock::Event::SocketError:
		case sock::Event::CriticalError:
		case sock::Event::SocketClosed:
			m_game->delete_client(client_h, true, true);
			break;
		}

	}
	else
	{
		for(int i = 0; i < m_game->m_client_list[client_h]->total_item_repair; i++)
		{
			if (m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_repair_all[i].index] != 0) {
				m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_repair_all[i].index]->m_instance.cur_durability = m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_repair_all[i].index]->m_durability;
				m_game->send_notify_msg(0, client_h, Notify::ItemRepaired, m_game->m_client_list[client_h]->m_repair_all[i].index, m_game->m_client_list[client_h]->m_item_list[m_game->m_client_list[client_h]->m_repair_all[i].index]->m_instance.cur_durability, 0, 0);
			}
		}
		m_game->calc_total_weight(set_item_count_by_id(client_h, hb::shared::item::ItemId::Gold, get_item_count_by_id(client_h, hb::shared::item::ItemId::Gold) - totalPrice));
	}
}
