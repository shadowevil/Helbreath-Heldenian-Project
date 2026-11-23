#include "InventoryManager.h"
#include "Game.h"
#include "TeleportManager.h"
#include "ItemNameFormatter.h"
#include "lan_eng.h"
#include "NetMessages.h"
#include "PacketSendHelpers.h"

#include <algorithm>
#include <climits>
#include <format>
#include "Screen_OnGame.h"

using namespace hb::shared::net;
using hb::shared::item::ItemType;
using hb::shared::item::EquipPos;
using hb::shared::item::to_int;

inventory_manager& inventory_manager::get()
{
	static inventory_manager instance;
	return instance;
}

void inventory_manager::set_game(CGame* game)
{
	m_game = game;
}

void inventory_manager::set_item_order(int where, int item_id)
{
	if (item_id < 0 || item_id >= hb::shared::limits::MaxItems) return;
	int i;

	switch (where) {
	case 0:
		for (i = 0; i < hb::shared::limits::MaxItems; i++)
			if (m_game->m_item_order[i] == item_id)
				m_game->m_item_order[i] = -1;

		for (i = 1; i < hb::shared::limits::MaxItems; i++)
			if ((m_game->m_item_order[i - 1] == -1) && (m_game->m_item_order[i] != -1)) {
				m_game->m_item_order[i - 1] = m_game->m_item_order[i];
				m_game->m_item_order[i] = -1;
			}

		for (i = 0; i < hb::shared::limits::MaxItems; i++)
			if (m_game->m_item_order[i] == -1) {
				m_game->m_item_order[i] = item_id;
				return;
			}
		break;
	}
}

int inventory_manager::calc_total_weight()
{
	int i, cnt;
	int64_t weight = 0, temp;
	cnt = 0;
	for (i = 0; i < hb::shared::limits::MaxItems; i++)
		if (m_game->m_player->m_item_list[i] != 0)
		{
			CItem* cfg = m_game->get_item_config(m_game->m_player->m_item_list[i]->m_id_num);
			if (cfg && ((cfg->get_item_type() == ItemType::Consume)
				|| (cfg->get_item_type() == ItemType::Arrow)))
			{
				int lp = m_game->m_player->m_item_list[i]->get_light_percent();
				int item_w = (lp > 0) ? cfg->m_weight * (100 - lp) / 100 : cfg->m_weight;
				temp = static_cast<int64_t>(item_w) * static_cast<int64_t>(m_game->m_player->m_item_list[i]->m_count);
				if (m_game->m_player->m_item_list[i]->m_id_num == hb::shared::item::ItemId::Gold) temp = temp / 20;
				weight += temp;
			}
			else if (cfg)
			{
				int lp = m_game->m_player->m_item_list[i]->get_light_percent();
				weight += (lp > 0) ? cfg->m_weight * (100 - lp) / 100 : cfg->m_weight;
			}
			cnt++;
		}

	return static_cast<int>(std::min<int64_t>(weight, INT_MAX));
}

int inventory_manager::get_total_item_count()
{
	int i, cnt;
	cnt = 0;
	for (i = 0; i < hb::shared::limits::MaxItems; i++)
		if (m_game->m_player->m_item_list[i] != 0) cnt++;
	return cnt;
}

int inventory_manager::get_bank_item_count()
{
	int i, cnt;

	cnt = 0;
	for (i = 0; i < hb::shared::limits::MaxBankItems; i++)
		if (m_game->m_player->m_bank_list[i] != 0) cnt++;

	return cnt;
}

void inventory_manager::erase_item(int item_id)
{
	if (item_id < 0 || item_id >= hb::shared::limits::MaxItems) return;
	int i;
	for (i = 0; i < 6; i++)
	{
		if (m_game->m_short_cut[i] == item_id)
		{
			std::string G_cTxt;
			auto itemInfo = item_name_formatter::get().format(m_game->m_player->m_item_list[item_id].get());
			auto effect = itemInfo.effect_text();
			auto extra = itemInfo.extra_text();
			if (i < 3) G_cTxt = std::format(ERASE_ITEM, itemInfo.name.c_str(), effect.c_str(), extra.c_str(), i + 1);
			else G_cTxt = std::format(ERASE_ITEM, itemInfo.name.c_str(), effect.c_str(), extra.c_str(), i + 7);
			m_game->add_event_list(G_cTxt.c_str(), 10);
			m_game->m_short_cut[i] = -1;
		}
	}

	if (item_id == m_game->m_recent_short_cut)
		m_game->m_recent_short_cut = -1;
	// ItemOrder
	for (i = 0; i < hb::shared::limits::MaxItems; i++)
		if (m_game->m_item_order[i] == item_id)
			m_game->m_item_order[i] = -1;
	for (i = 1; i < hb::shared::limits::MaxItems; i++)
		if ((m_game->m_item_order[i - 1] == -1) && (m_game->m_item_order[i] != -1))
		{
			m_game->m_item_order[i - 1] = m_game->m_item_order[i];
			m_game->m_item_order[i] = -1;
		}
	// ItemList
	m_game->m_player->m_item_list[item_id].reset();
	m_game->m_is_item_equipped[item_id] = false;
	unlock_item(item_id);
}

bool inventory_manager::check_item_operation_enabled(int item_id)
{
	if (item_id < 0 || item_id >= hb::shared::limits::MaxItems) return false;
	if (m_game->m_player->m_item_list[item_id] == 0) return false;
	if (m_game->m_player->m_Controller.get_command() < 0) return false;
	if (teleport_manager::get().is_requested()) return false;
	if (is_locked(item_id)) return false;

	if ((m_game->m_player->m_item_list[item_id]->m_id_num == 867) && (m_game->on_game()->m_using_slate == true)) // Ancient Tablet
	{
		if ((m_game->m_map_index == 35) || (m_game->m_map_index == 36) || (m_game->m_map_index == 37))
		{
			m_game->add_event_list(DEF_MSG_NOTIFY_SLATE_WRONG_MAP, 10);
			return false;
		}
		m_game->add_event_list(DEF_MSG_NOTIFY_SLATE_ALREADYUSING, 10);
		return false;
	}

	if (m_game->get_dialog_box_manager().is_blocking_operation_active())
	{
		m_game->add_event_list(BCHECK_ITEM_OPERATION_ENABLE1, 10);
		return false;
	}

	return true;
}

void inventory_manager::unequip_slot(int equip_pos)
{
	if (equip_pos < 0 || equip_pos >= hb::shared::item::DEF_MAXITEMEQUIPPOS) return;
	std::string G_cTxt;
	if (m_game->m_item_equipment_status[equip_pos] < 0) return;
	// Remove Angelic Stats
	CItem* cfg_eq = m_game->get_item_config(m_game->m_player->m_item_list[m_game->m_item_equipment_status[equip_pos]]->m_id_num);
	if ((equip_pos >= 11)
		&& (cfg_eq && cfg_eq->get_item_type() == ItemType::Equip))
	{
		short item_id = m_game->m_item_equipment_status[equip_pos];
		if (m_game->m_player->m_item_list[item_id]->m_id_num == hb::shared::item::ItemId::AngelicPandentSTR)
			m_game->m_player->m_angelic_str = 0;
		else if (m_game->m_player->m_item_list[item_id]->m_id_num == hb::shared::item::ItemId::AngelicPandentDEX)
			m_game->m_player->m_angelic_dex = 0;
		else if (m_game->m_player->m_item_list[item_id]->m_id_num == hb::shared::item::ItemId::AngelicPandentINT)
			m_game->m_player->m_angelic_int = 0;
		else if (m_game->m_player->m_item_list[item_id]->m_id_num == hb::shared::item::ItemId::AngelicPandentMAG)
			m_game->m_player->m_angelic_mag = 0;
	}

	auto itemInfo2 = item_name_formatter::get().format(m_game->m_player->m_item_list[m_game->m_item_equipment_status[equip_pos]].get());
	G_cTxt = std::format(ITEM_EQUIPMENT_RELEASED, itemInfo2.name.c_str());
	m_game->add_event_list(G_cTxt.c_str(), 10);
	m_game->m_is_item_equipped[m_game->m_item_equipment_status[equip_pos]] = false;
	m_game->m_item_equipment_status[equip_pos] = -1;
}

void inventory_manager::equip_item(int item_id)
{
	if (item_id < 0 || item_id >= hb::shared::limits::MaxItems) return;
	std::string G_cTxt;
	if (check_item_operation_enabled(item_id) == false) return;
	if (m_game->m_is_item_equipped[item_id] == true) return;
	CItem* cfg = m_game->get_item_config(m_game->m_player->m_item_list[item_id]->m_id_num);
	if (!cfg) return;
	if (cfg->get_equip_pos() == EquipPos::None)
	{
		m_game->add_event_list(BITEMDROP_CHARACTER3, 10);
		return;
	}
	if (m_game->m_player->m_item_list[item_id]->m_cur_life_span == 0)
	{
		m_game->add_event_list(BITEMDROP_CHARACTER1, 10);
		return;
	}
	int light_pct = m_game->m_player->m_item_list[item_id]->get_light_percent();
	int equip_weight = (light_pct > 0) ? cfg->m_weight * (100 - light_pct) / 100 : cfg->m_weight;
	if (equip_weight / 100 > m_game->m_player->m_str + m_game->m_player->m_angelic_str)
	{
		m_game->add_event_list(BITEMDROP_CHARACTER2, 10);
		return;
	}
	if (((m_game->m_player->m_item_list[item_id]->m_attribute & 0x00000001) == 0) && (cfg->m_level_limit > m_game->m_player->m_level))
	{
		m_game->add_event_list(BITEMDROP_CHARACTER4, 10);
		return;
	}
	if (m_game->on_game()->m_skill_using_status == true)
	{
		m_game->add_event_list(BITEMDROP_CHARACTER5, 10);
		return;
	}
	if (cfg->m_gender_limit != 0)
	{
		switch (m_game->m_player->m_player_type) {
		case 1:
		case 2:
		case 3:
			if (cfg->m_gender_limit != 1)
			{
				m_game->add_event_list(BITEMDROP_CHARACTER6, 10);
				return;
			}
			break;
		case 4:
		case 5:
		case 6:
			if (cfg->m_gender_limit != 2)
			{
				m_game->add_event_list(BITEMDROP_CHARACTER7, 10);
				return;
			}
			break;
		}
	}

	{
		auto pkt = hb::net::make_common_command(CommonType::equip_item, m_game->m_player->m_player_x, m_game->m_player->m_player_y);
		pkt.v1 = item_id;
		m_game->send_game_packet(pkt);
	}
	m_game->m_recent_short_cut = item_id;
	unequip_slot(cfg->m_equip_pos);
	switch (cfg->get_equip_pos()) {
	case EquipPos::Head:
	case EquipPos::Body:
	case EquipPos::Arms:
	case EquipPos::Pants:
	case EquipPos::Leggings:
	case EquipPos::Back:
		unequip_slot(to_int(EquipPos::FullBody));
		break;
	case EquipPos::FullBody:
		unequip_slot(to_int(EquipPos::Head));
		unequip_slot(to_int(EquipPos::Body));
		unequip_slot(to_int(EquipPos::Arms));
		unequip_slot(to_int(EquipPos::Pants));
		unequip_slot(to_int(EquipPos::Leggings));
		unequip_slot(to_int(EquipPos::Back));
		break;
	case EquipPos::LeftHand:
	case EquipPos::RightHand:
		unequip_slot(to_int(EquipPos::TwoHand));
		break;
	case EquipPos::TwoHand:
		unequip_slot(to_int(EquipPos::RightHand));
		unequip_slot(to_int(EquipPos::LeftHand));
		break;
	}

	m_game->m_item_equipment_status[cfg->m_equip_pos] = item_id;
	m_game->m_is_item_equipped[item_id] = true;

	// Add Angelic Stats
	if ((cfg->get_item_type() == ItemType::Equip)
		&& (cfg->m_equip_pos >= 11))
	{
		int angel_value = (m_game->m_player->m_item_list[item_id]->m_attribute & 0xF0000000) >> 28;
		if (m_game->m_player->m_item_list[item_id]->m_id_num == hb::shared::item::ItemId::AngelicPandentSTR)
			m_game->m_player->m_angelic_str = 1 + angel_value;
		else if (m_game->m_player->m_item_list[item_id]->m_id_num == hb::shared::item::ItemId::AngelicPandentDEX)
			m_game->m_player->m_angelic_dex = 1 + angel_value;
		else if (m_game->m_player->m_item_list[item_id]->m_id_num == hb::shared::item::ItemId::AngelicPandentINT)
			m_game->m_player->m_angelic_int = 1 + angel_value;
		else if (m_game->m_player->m_item_list[item_id]->m_id_num == hb::shared::item::ItemId::AngelicPandentMAG)
			m_game->m_player->m_angelic_mag = 1 + angel_value;
	}

	auto itemInfo3 = item_name_formatter::get().format(m_game->m_player->m_item_list[item_id].get());
	G_cTxt = std::format(BITEMDROP_CHARACTER9, itemInfo3.name.c_str());
	m_game->add_event_list(G_cTxt.c_str(), 10);
	{
		short id = m_game->m_player->m_item_list[item_id]->m_id_num;
		if (id == hb::shared::item::ItemId::AngelicPandentSTR || id == hb::shared::item::ItemId::AngelicPandentDEX ||
			id == hb::shared::item::ItemId::AngelicPandentINT || id == hb::shared::item::ItemId::AngelicPandentMAG)
			m_game->play_game_sound('E', 52, 0);
		else
			m_game->play_game_sound('E', 28, 0);
	}
}

void inventory_manager::lock_item(int slot)
{
	if (slot >= 0 && slot < hb::shared::limits::MaxItems)
		m_game->m_is_item_disabled[slot] = true;
}

void inventory_manager::unlock_item(int slot)
{
	if (slot >= 0 && slot < hb::shared::limits::MaxItems)
		m_game->m_is_item_disabled[slot] = false;
}

bool inventory_manager::is_locked(int slot) const
{
	if (slot >= 0 && slot < hb::shared::limits::MaxItems)
		return m_game->m_is_item_disabled[slot];
	return false;
}

void inventory_manager::unlock_all()
{
	m_game->m_is_item_disabled.fill(false);
}
