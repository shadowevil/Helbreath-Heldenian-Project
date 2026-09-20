// CraftingManager.cpp: Manages potion brewing and crafting recipe processing.
// Extracted from CGame (Phase B2).

#include "CraftingManager.h"
#include "Game.h"
#include "SkillManager.h"
#include "ItemManager.h"
#include "Portion.h"
#include "Item.h"
#include "Packet/SharedPackets.h"
#include "Log.h"
#include "ServerLogChannels.h"
#include "TimeUtils.h"

extern char G_cTxt[512];

using hb::log_channel;

using namespace hb::shared::net;
using namespace hb::server::config;
using namespace hb::shared::item;
namespace sock = hb::shared::net::socket;

CraftingManager::CraftingManager()
{
	for (int i = 0; i < MaxPortionTypes; i++) {
		m_portion_config_list[i] = 0;
		m_crafting_config_list[i] = 0;
	}
}

CraftingManager::~CraftingManager()
{
	cleanup_arrays();
}

void CraftingManager::init_arrays()
{
	for (int i = 0; i < MaxPortionTypes; i++) {
		m_portion_config_list[i] = 0;
		m_crafting_config_list[i] = 0;
	}
}

void CraftingManager::cleanup_arrays()
{
	for (int i = 0; i < MaxPortionTypes; i++) {
		if (m_portion_config_list[i] != 0) delete m_portion_config_list[i];
		if (m_crafting_config_list[i] != 0) delete m_crafting_config_list[i];
	}
}

void CraftingManager::req_create_portion_handler(int client_h, char* data)
{
	char cI[6], portion_name[hb::shared::limits::ItemNameLen];
	int    ret, j, erase_req, skill_limit, skill_level, result, difficulty;
	short item_index[6], temp;
	short  item_number[6], item_array[12];
	bool   dup, flag;
	CItem* item;

	if (m_game->m_client_list[client_h] == 0) return;
	m_game->m_client_list[client_h]->m_skill_msg_recv_count++;

	for(int i = 0; i < 6; i++) {
		cI[i] = -1;
		item_index[i] = -1;
		item_number[i] = 0;
	}

	const auto* pkt = hb::net::PacketCast<hb::net::PacketCommandCommonItems>(
		data, sizeof(hb::net::PacketCommandCommonItems));
	if (!pkt) return;
	for(int i = 0; i < 6; i++) {
		cI[i] = static_cast<char>(pkt->item_ids[i]);
	}

	for(int i = 0; i < 6; i++) {
		if (cI[i] >= hb::shared::limits::MaxItems) return;
		if ((cI[i] >= 0) && (m_game->m_client_list[client_h]->m_item_list[cI[i]] == 0)) return;
	}

	for(int i = 0; i < 6; i++)
		if (cI[i] >= 0) {
			dup = false;
			for (j = 0; j < 6; j++)
				if (item_index[j] == cI[i]) {
					item_number[j]++;
					dup = true;
				}
			if (dup == false) {
				for (j = 0; j < 6; j++)
					if (item_index[j] == -1) {
						item_index[j] = cI[i];
						item_number[j]++;
						break;
					}
			}
		}

	for(int i = 0; i < 6; i++)
		if (item_index[i] != -1) {
			if (item_index[i] < 0) return;
			if ((item_index[i] >= 0) && (item_index[i] >= hb::shared::limits::MaxItems)) return;
			if (m_game->m_client_list[client_h]->m_item_list[item_index[i]] == 0) return;
			if (m_game->m_client_list[client_h]->m_item_list[item_index[i]]->m_instance.count < static_cast<uint64_t>(item_number[i])) return;
		}

	// . Bubble Sort
	flag = true;
	while (flag) {
		flag = false;
		for(int i = 0; i < 5; i++)
			if ((item_index[i] != -1) && (item_index[i + 1] != -1)) {
				if ((m_game->m_client_list[client_h]->m_item_list[item_index[i]]->m_id_num) <
					(m_game->m_client_list[client_h]->m_item_list[item_index[i + 1]]->m_id_num)) {
					temp = item_index[i + 1];
					item_index[i + 1] = item_index[i];
					item_index[i] = temp;
					temp = item_number[i + 1];
					item_number[i + 1] = item_number[i];
					item_number[i] = temp;
					flag = true;
				}
			}
	}

	j = 0;
	for(int i = 0; i < 6; i++) {
		if (item_index[i] != -1)
			item_array[j] = m_game->m_client_list[client_h]->m_item_list[item_index[i]]->m_id_num;
		else item_array[j] = item_index[i];
		item_array[j + 1] = item_number[i];
		j += 2;
	}

	std::memset(portion_name, 0, sizeof(portion_name));

	for(int i = 0; i < MaxPortionTypes; i++)
		if (m_portion_config_list[i] != 0) {
			flag = false;
			for (j = 0; j < 12; j++)
				if (m_portion_config_list[i]->m_array[j] != item_array[j]) flag = true;

			if (flag == false) {
				std::memset(portion_name, 0, sizeof(portion_name));
				memcpy(portion_name, m_portion_config_list[i]->m_name, hb::shared::limits::ItemNameLen - 1);
				skill_limit = m_portion_config_list[i]->m_skill_limit;
				difficulty = m_portion_config_list[i]->m_difficulty;
			}
		}

	if (strlen(portion_name) == 0) {
		m_game->send_notify_msg(0, client_h, Notify::NoMatchingPortion, 0, 0, 0, 0);
		return;
	}

	skill_level = m_game->m_client_list[client_h]->m_skill_mastery[12];
	if (skill_limit > skill_level) {
		m_game->send_notify_msg(0, client_h, Notify::LowPortionSkill, 0, 0, 0, portion_name);
		return;
	}

	skill_level -= difficulty;
	if (skill_level <= 0) skill_level = 1;

	result = m_game->dice(1, 100);
	if (result > skill_level) {
		m_game->send_notify_msg(0, client_h, Notify::PortionFail, 0, 0, 0, portion_name);
		return;
	}

	m_game->m_skill_manager->calculate_ssn_skill_index(client_h, 12, 1);

	if (strlen(portion_name) != 0) {
		item = 0;
		item = new CItem;
		if (item == 0) return;

		for(int i = 0; i < 6; i++)
			if (item_index[i] != -1) {
				if (m_game->m_client_list[client_h]->m_item_list[item_index[i]]->is_stackable())
					m_game->m_item_manager->set_item_count(client_h, item_index[i],
						m_game->m_client_list[client_h]->m_item_list[item_index[i]]->m_instance.count - item_number[i]);
				else m_game->m_item_manager->item_deplete_handler(client_h, item_index[i], false);
			}

		m_game->send_notify_msg(0, client_h, Notify::PortionSuccess, 0, 0, 0, portion_name);
		m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(1, (difficulty / 3));

		if ((m_game->m_item_manager->init_item_attr(item, portion_name))) {
			if (m_game->m_item_manager->add_client_item_list(client_h, item, &erase_req)) {
				ret = m_game->m_item_manager->send_item_notify_msg(client_h, Notify::ItemObtained, item, 0);
				switch (ret) {
				case sock::Event::QueueFull:
				case sock::Event::SocketError:
				case sock::Event::CriticalError:
				case sock::Event::SocketClosed:
					m_game->delete_client(client_h, true, true);
					break;
				}

				//if ((item->m_sell_price * item->m_instance.count) > 1000)
				//	SendMsgToLS(ServerMsgId::RequestSavePlayerData, client_h);
			}
			else {
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(m_game->m_client_list[client_h]->m_x,
					m_game->m_client_list[client_h]->m_y, item);

				m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index,
					m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, item);

				ret = m_game->m_item_manager->send_item_notify_msg(client_h, Notify::CannotCarryMoreItem, 0, 0);

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
		else {
			delete item;
			item = 0;
		}
	}
}

///		Snoopy: Added Crafting to the same file than potions
void CraftingManager::req_create_crafting_handler(int client_h, char* data)
{
	char cI[6], crafting_name[hb::shared::limits::ItemNameLen];
	int    ret, j, erase_req, risk_level, difficulty, needed_contrib = 0;
	short temp;
	short  item_index[6], item_purity[6], item_number[6], item_array[12];
	bool   dup, flag, need_log;
	CItem* item;

	if (m_game->m_client_list[client_h] == 0) return;
	m_game->m_client_list[client_h]->m_skill_msg_recv_count++;

	for(int i = 0; i < 6; i++)
	{
		cI[i] = -1;
		item_index[i] = -1;
		item_number[i] = 0;
		item_purity[i] = -1;
	}
	const auto* pkt = hb::net::PacketCast<hb::net::PacketCommandCommonBuild>(
		data, sizeof(hb::net::PacketCommandCommonBuild));
	if (!pkt) return;
	for(int i = 0; i < 6; i++) {
		cI[i] = static_cast<char>(pkt->item_ids[i]);
	}

	for(int i = 0; i < 6; i++)
	{
		if (cI[i] >= hb::shared::limits::MaxItems) return;
		if ((cI[i] >= 0) && (m_game->m_client_list[client_h]->m_item_list[cI[i]] == 0)) return;
	}

	for(int i = 0; i < 6; i++)
		if (cI[i] >= 0)
		{
			dup = false;
			for (j = 0; j < 6; j++)
				if (item_index[j] == cI[i])
				{
					item_number[j]++;
					dup = true;
				}
			if (dup == false)
			{
				for (j = 0; j < 6; j++)
					if (item_index[j] == -1)
					{
						item_index[j] = cI[i];
						item_number[j]++;
						break;
					}
			}
		}

	for(int i = 0; i < 6; i++)
		if (item_index[i] != -1)
		{
			if (item_index[i] < 0) return;
			if ((item_index[i] >= 0) && (item_index[i] >= hb::shared::limits::MaxItems)) return;
			if (m_game->m_client_list[client_h]->m_item_list[item_index[i]] == 0) return;
			if (m_game->m_client_list[client_h]->m_item_list[item_index[i]]->m_instance.count < static_cast<uint64_t>(item_number[i])) return;
			item_purity[i] = m_game->m_client_list[client_h]->m_item_list[item_index[i]]->m_instance.special_effect_value2;
			if (m_game->m_client_list[client_h]->m_item_list[item_index[i]]->m_id_num == 657) // Stone of Merien
			{
				item_purity[i] = 100; // Merien stones considered 100% purity.
			}
			if (m_game->m_client_list[client_h]->m_item_list[item_index[i]]->is_stackable())
			{
				item_purity[i] = -1; // Diamonds / Emeralds.etc.. never have purity
			}
			if (item_number[i] > 1) // No purity for stacked items
			{
				item_purity[i] = -1;
			}
			/*std::snprintf(G_cTxt, sizeof(G_cTxt), "Crafting: %d x %s (%d)"
				, item_number[i]
				, m_game->m_client_list[client_h]->m_item_list[item_index[i]]->m_name
				, m_game->m_client_list[client_h]->m_item_list[item_index[i]]->m_id_num);
			PutLogList(G_cTxt);*/

			if ((m_game->m_client_list[client_h]->m_item_list[item_index[i]]->get_item_type() == hb::shared::item::item_type::equipment)
				&& (m_game->m_client_list[client_h]->m_item_list[item_index[i]]->get_equip_pos() == EquipPos::Neck))
			{
				needed_contrib = 10; // Necks Crafting requires 10 contrib
			}
		}

	// Bubble Sort
	flag = true;
	while (flag)
	{
		flag = false;
		for(int i = 0; i < 5; i++)
			if ((item_index[i] != -1) && (item_index[i + 1] != -1))
			{
				if ((m_game->m_client_list[client_h]->m_item_list[item_index[i]]->m_id_num) < (m_game->m_client_list[client_h]->m_item_list[item_index[i + 1]]->m_id_num))
				{
					temp = item_index[i + 1];
					item_index[i + 1] = item_index[i];
					item_index[i] = temp;
					temp = item_purity[i + 1];
					item_purity[i + 1] = item_purity[i];
					item_purity[i] = temp;
					temp = item_number[i + 1];
					item_number[i + 1] = item_number[i];
					item_number[i] = temp;
					flag = true;
				}
			}
	}
	j = 0;
	for(int i = 0; i < 6; i++)
	{
		if (item_index[i] != -1)
			item_array[j] = m_game->m_client_list[client_h]->m_item_list[item_index[i]]->m_id_num;
		else item_array[j] = item_index[i];
		item_array[j + 1] = item_number[i];
		j += 2;
	}

	// Search Crafting you wanna build
	std::memset(crafting_name, 0, sizeof(crafting_name));
	for(int i = 0; i < MaxPortionTypes; i++)
		if (m_crafting_config_list[i] != 0)
		{
			flag = false;
			for (j = 0; j < 12; j++)
			{
				if (m_crafting_config_list[i]->m_array[j] != item_array[j]) flag = true; // one item mismatch
			}
			if (flag == false) // good Crafting receipe
			{
				std::memset(crafting_name, 0, sizeof(crafting_name));
				memcpy(crafting_name, m_crafting_config_list[i]->m_name, hb::shared::limits::ItemNameLen - 1);
				risk_level = m_crafting_config_list[i]->m_skill_limit;			// % to loose item if crafting fails
				difficulty = m_crafting_config_list[i]->m_difficulty;
			}
		}

	// Check if recipe is OK
	if (strlen(crafting_name) == 0)
	{
		m_game->send_notify_msg(0, client_h, Notify::CraftingFail, 1, 0, 0, 0); // "There is not enough material"
		return;
	}
	// Check for Contribution
	if (m_game->m_client_list[client_h]->m_contribution < needed_contrib)
	{
		m_game->send_notify_msg(0, client_h, Notify::CraftingFail, 2, 0, 0, 0); // "There is not enough Contribution Point"
		return;
	}
	// Check possible Failure
	if (m_game->dice(1, 100) > static_cast<uint32_t>(difficulty))
	{
		m_game->send_notify_msg(0, client_h, Notify::CraftingFail, 3, 0, 0, 0); // "Crafting failed"
		// Remove parts...
		item = 0;
		item = new CItem;
		if (item == 0) return;
		for(int i = 0; i < 6; i++)
			if (item_index[i] != -1)
			{	// Deplete any Merien Stone
				if (m_game->m_client_list[client_h]->m_item_list[item_index[i]]->m_id_num == 657) // Stone of Merien
				{
					m_game->m_item_manager->item_deplete_handler(client_h, item_index[i], false);
				}
				else
					// Risk to deplete any other items (not stackable ones) // DEF_ITEMTYPE_CONSUME
					if ((m_game->m_client_list[client_h]->m_item_list[item_index[i]]->get_item_type() == hb::shared::item::item_type::equipment)
						|| (m_game->m_client_list[client_h]->m_item_list[item_index[i]]->get_item_type() == hb::shared::item::item_type::material))
					{
						if (m_game->dice(1, 100) < static_cast<uint32_t>(risk_level))
						{
							m_game->m_item_manager->item_deplete_handler(client_h, item_index[i], false);
						}
					}
			}
		return;
	}

	// Purity
	int purity, tot = 0, count = 0;
	for(int i = 0; i < 6; i++)
	{
		if (item_index[i] != -1)
		{
			if (item_purity[i] != -1)
			{
				tot += item_purity[i];
				count++;
			}
		}
	}
	if (count == 0)
	{
		purity = 20 + m_game->dice(1, 80);			// Wares have random purity (20%..100%)
		need_log = false;
	}
	else
	{
		purity = tot / count;
		tot = (purity * 4) / 5;
		count = purity - tot;
		purity = tot + m_game->dice(1, count);	// Jewel completion depends off Wares purity
		need_log = true;
	}
	if (needed_contrib != 0)
	{
		purity = 0;						// Necks require contribution but no purity/completion
		need_log = true;
	}
	m_game->m_skill_manager->calculate_ssn_skill_index(client_h, 18, 1);

	if (strlen(crafting_name) != 0)
	{
		item = 0;
		item = new CItem;
		if (item == 0) return;
		for(int i = 0; i < 6; i++)
		{
			if (item_index[i] != -1)
			{
				if (m_game->m_client_list[client_h]->m_item_list[item_index[i]]->is_stackable())
				{
					m_game->m_item_manager->set_item_count(client_h, item_index[i],
						m_game->m_client_list[client_h]->m_item_list[item_index[i]]->m_instance.count - item_number[i]);
				}
				else // Non-stackable items get depleted
				{
					m_game->m_item_manager->item_deplete_handler(client_h, item_index[i], false);
				}
			}
		}
		if (needed_contrib != 0)
		{
			m_game->m_client_list[client_h]->m_contribution -= needed_contrib;
			// No known msg to send info to client, so client will compute shown Contrib himself.
		}

		m_game->send_notify_msg(0, client_h, Notify::CraftingSuccess, 0, 0, 0, 0);

		m_game->m_client_list[client_h]->m_exp_stock += m_game->dice(2, 100);

		if ((m_game->m_item_manager->init_item_attr(item, crafting_name)))
		{	// // Snoopy: Added Purity to Oils/Elixirs
			if (purity != 0)
			{
				item->m_instance.special_effect_value2 = purity;
				item->m_instance.custom_made = 1;
			}
			item->set_touch_effect_type(TouchEffectType::ID);
			item->m_instance.touch_effect_value1 = static_cast<short>(m_game->dice(1, 100000));
			item->m_instance.touch_effect_value2 = static_cast<short>(m_game->dice(1, 100000));
			// item->m_instance.touch_effect_value3 = GameClock::GetTimeMS();
			hb::time::local_time SysTime{};
			char temp[256];
			SysTime = hb::time::local_time::now();
			std::memset(temp, 0, sizeof(temp));
			std::snprintf(temp, sizeof(temp), "%d%2d", (short)SysTime.month, (short)SysTime.day);
			item->m_instance.touch_effect_value3 = atoi(temp);

			// SNOOPY log anything above WAREs
			if (need_log)
			{
				hb::logger::log<log_channel::events>("Player '{}' crafting '{}' purity={}", m_game->m_client_list[client_h]->m_char_name, item->m_name, item->m_instance.special_effect_value2);
			}
			if (m_game->m_item_manager->add_client_item_list(client_h, item, &erase_req))
			{
				ret = m_game->m_item_manager->send_item_notify_msg(client_h, Notify::ItemObtained, item, 0);
				switch (ret) {
				case sock::Event::QueueFull:
				case sock::Event::SocketError:
				case sock::Event::CriticalError:
				case sock::Event::SocketClosed:
					m_game->delete_client(client_h, true, true);
					break;
				}
				//if ((item->m_sell_price * item->m_instance.count) > 1000)
				//	SendMsgToLS(ServerMsgId::RequestSavePlayerData, client_h);
			}
			else
			{
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(m_game->m_client_list[client_h]->m_x,
					m_game->m_client_list[client_h]->m_y, item);
				m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index,
					m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, item);

				ret = m_game->m_item_manager->send_item_notify_msg(client_h, Notify::CannotCarryMoreItem, 0, 0);

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
		else
		{
			delete item;
			item = 0;
		}
	}
}
