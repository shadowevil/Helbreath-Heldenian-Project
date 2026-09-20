#include "StatusEffectManager.h"
#include "Game.h"
#include "Item.h"
#include "ItemManager.h"
#include "Packet/SharedPackets.h"
#include "ObjectIDRange.h"

using namespace hb::shared::net;
using namespace hb::shared::action;
using namespace hb::server::net;
using namespace hb::server::config;
namespace sock = hb::shared::net::socket;

extern char G_cTxt[512];

void StatusEffectManager::set_hero_flag(short owner_h, char owner_type, bool status)
{
	switch (owner_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[owner_h] == 0) return;
		if (status)
			m_game->m_client_list[owner_h]->m_status.hero = true;
		else m_game->m_client_list[owner_h]->m_status.hero = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[owner_h] == 0) return;
		if (status)
			m_game->m_npc_list[owner_h]->m_status.hero = true;
		else m_game->m_npc_list[owner_h]->m_status.hero = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;
	}
}

void StatusEffectManager::set_berserk_flag(short owner_h, char owner_type, bool status)
{
	switch (owner_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[owner_h] == 0) return;
		if (status)
			m_game->m_client_list[owner_h]->m_status.berserk = true;
		else m_game->m_client_list[owner_h]->m_status.berserk = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[owner_h] == 0) return;
		if (status)
			m_game->m_npc_list[owner_h]->m_status.berserk = true;
		else m_game->m_npc_list[owner_h]->m_status.berserk = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;
	}
}

void StatusEffectManager::set_haste_flag(short owner_h, char owner_type, bool status)
{
	switch (owner_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[owner_h] == 0) return;
		if (status)
			m_game->m_client_list[owner_h]->m_status.haste = true;
		else m_game->m_client_list[owner_h]->m_status.haste = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;

	case hb::shared::owner_class::Npc:
		break;
	}
}

void StatusEffectManager::set_poison_flag(short owner_h, char owner_type, bool status)
{
	switch (owner_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[owner_h] == 0) return;
		if (status)
			m_game->m_client_list[owner_h]->m_status.poisoned = true;
		else m_game->m_client_list[owner_h]->m_status.poisoned = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[owner_h] == 0) return;
		if (status)
			m_game->m_npc_list[owner_h]->m_status.poisoned = true;
		else m_game->m_npc_list[owner_h]->m_status.poisoned = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;
	}
}

void StatusEffectManager::set_defense_shield_flag(short owner_h, char owner_type, bool status)
{
	switch (owner_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[owner_h] == 0) return;
		if (status)
			m_game->m_client_list[owner_h]->m_status.defense_shield = true;
		else m_game->m_client_list[owner_h]->m_status.defense_shield = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[owner_h] == 0) return;
		if (status)
			m_game->m_npc_list[owner_h]->m_status.defense_shield = true;
		else m_game->m_npc_list[owner_h]->m_status.defense_shield = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;
	}
}

void StatusEffectManager::set_magic_protection_flag(short owner_h, char owner_type, bool status)
{
	switch (owner_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[owner_h] == 0) return;
		if (status)
			m_game->m_client_list[owner_h]->m_status.magic_protection = true;
		else m_game->m_client_list[owner_h]->m_status.magic_protection = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[owner_h] == 0) return;
		if (status)
			m_game->m_npc_list[owner_h]->m_status.magic_protection = true;
		else m_game->m_npc_list[owner_h]->m_status.magic_protection = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;
	}
}

void StatusEffectManager::set_protection_from_arrow_flag(short owner_h, char owner_type, bool status)
{
	switch (owner_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[owner_h] == 0) return;
		if (status)
			m_game->m_client_list[owner_h]->m_status.protection_from_arrow = true;
		else m_game->m_client_list[owner_h]->m_status.protection_from_arrow = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[owner_h] == 0) return;
		if (status)
			m_game->m_npc_list[owner_h]->m_status.protection_from_arrow = true;
		else m_game->m_npc_list[owner_h]->m_status.protection_from_arrow = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;
	}
}

void StatusEffectManager::set_illusion_movement_flag(short owner_h, char owner_type, bool status)
{
	switch (owner_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[owner_h] == 0) return;
		if (status)
			m_game->m_client_list[owner_h]->m_status.illusion_movement = true;
		else m_game->m_client_list[owner_h]->m_status.illusion_movement = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;
	}
}

void StatusEffectManager::set_illusion_flag(short owner_h, char owner_type, bool status)
{
	switch (owner_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[owner_h] == 0) return;
		if (status)
			m_game->m_client_list[owner_h]->m_status.illusion = true;
		else m_game->m_client_list[owner_h]->m_status.illusion = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[owner_h] == 0) return;
		if (status)
			m_game->m_npc_list[owner_h]->m_status.illusion = true;
		else m_game->m_npc_list[owner_h]->m_status.illusion = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;
	}
}

void StatusEffectManager::set_ice_flag(short owner_h, char owner_type, bool status)
{
	switch (owner_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[owner_h] == 0) return;
		if (status)
			m_game->m_client_list[owner_h]->m_status.frozen = true;
		else m_game->m_client_list[owner_h]->m_status.frozen = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[owner_h] == 0) return;
		if (status)
			m_game->m_npc_list[owner_h]->m_status.frozen = true;
		else m_game->m_npc_list[owner_h]->m_status.frozen = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;
	}
}

void StatusEffectManager::set_invisibility_flag(short owner_h, char owner_type, bool status)
{
	switch (owner_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[owner_h] == 0) return;
		if (status)
			m_game->m_client_list[owner_h]->m_status.invisibility = true;
		else m_game->m_client_list[owner_h]->m_status.invisibility = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[owner_h] == 0) return;
		if (status)
			m_game->m_npc_list[owner_h]->m_status.invisibility = true;
		else m_game->m_npc_list[owner_h]->m_status.invisibility = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;
	}
}

void StatusEffectManager::set_inhibition_casting_flag(short owner_h, char owner_type, bool status)
{
	switch (owner_type) {
	case hb::shared::owner_class::Player:
		if (m_game->m_client_list[owner_h] == 0) return;
		if (status)
			m_game->m_client_list[owner_h]->m_status.inhibition_casting = true;
		else m_game->m_client_list[owner_h]->m_status.inhibition_casting = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;

	case hb::shared::owner_class::Npc:
		if (m_game->m_npc_list[owner_h] == 0) return;
		if (status)
			m_game->m_npc_list[owner_h]->m_status.inhibition_casting = true;
		else m_game->m_npc_list[owner_h]->m_status.inhibition_casting = false;
		m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
		break;
	}
}

void StatusEffectManager::set_angel_flag(short owner_h, char owner_type, int status, int temp)
{
	if (owner_type != hb::shared::owner_class::Player) return;
	if (m_game->m_client_list[owner_h] == 0) return;
	switch (status) {
	case 1: // STR Angel
		m_game->m_client_list[owner_h]->m_status.angel_str = true;
		break;
	case 2: // DEX Angel
		m_game->m_client_list[owner_h]->m_status.angel_dex = true;
		break;
	case 3: // INT Angel
		m_game->m_client_list[owner_h]->m_status.angel_int = true;
		break;
	case 4: // MAG Angel
		m_game->m_client_list[owner_h]->m_status.angel_mag = true;
		break;
	default:
	case 0: // Remove all Angels
		m_game->m_client_list[owner_h]->m_status.angel_percent = 0;
		m_game->m_client_list[owner_h]->m_status.angel_str = false;
		m_game->m_client_list[owner_h]->m_status.angel_dex = false;
		m_game->m_client_list[owner_h]->m_status.angel_int = false;
		m_game->m_client_list[owner_h]->m_status.angel_mag = false;
		break;
	}
	if (temp > 4)
	{
		int angelic_stars = (temp / 3) * (temp / 5);
		m_game->m_client_list[owner_h]->m_status.angel_percent = static_cast<uint8_t>(angelic_stars);
	}
	m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
}

void StatusEffectManager::check_farming_action(short attacker_h, short target_h, bool type)
{
	char crop_type;
	int item_id;
	CItem* item;

	item_id = 0;
	crop_type = 0;

	crop_type = m_game->m_npc_list[target_h]->m_crop_type;
	switch (crop_type) {
	case 1: m_game->get_exp(attacker_h, m_game->dice(3, 10)); item_id = 820; break; // WaterMelon
	case 2: m_game->get_exp(attacker_h, m_game->dice(3, 10)); item_id = 821; break; // Pumpkin
	case 3: m_game->get_exp(attacker_h, m_game->dice(4, 10)); item_id = 822; break; // Garlic
	case 4: m_game->get_exp(attacker_h, m_game->dice(4, 10)); item_id = 823; break; // Barley
	case 5: m_game->get_exp(attacker_h, m_game->dice(5, 10)); item_id = 824; break; // Carrot
	case 6: m_game->get_exp(attacker_h, m_game->dice(5, 10)); item_id = 825; break; // Radish
	case 7: m_game->get_exp(attacker_h, m_game->dice(6, 10)); item_id = 826; break; // Corn
	case 8: m_game->get_exp(attacker_h, m_game->dice(6, 10)); item_id = 827; break; // ChineseBellflower
	case 9: m_game->get_exp(attacker_h, m_game->dice(7, 10)); item_id = 828; break; // Melone
	case 10: m_game->get_exp(attacker_h, m_game->dice(7, 10)); item_id = 829; break; // Tommato
	case 11: m_game->get_exp(attacker_h, m_game->dice(8, 10)); item_id = 830; break; // Grapes
	case 12: m_game->get_exp(attacker_h, m_game->dice(8, 10)); item_id = 831; break; // BlueGrapes
	case 13: m_game->get_exp(attacker_h, m_game->dice(9, 10)); item_id = 832; break; // Mushroom
	default: m_game->get_exp(attacker_h, m_game->dice(10, 10)); item_id = 721; break; // Ginseng

	}

	item = new CItem;
	if (m_game->m_item_manager->init_item_attr(item, item_id) == false) {
		delete item;
	}
	if (type == 0) {
		m_game->m_map_list[m_game->m_client_list[attacker_h]->m_map_index]->set_item(m_game->m_client_list[attacker_h]->m_x, m_game->m_client_list[attacker_h]->m_y, item);
		m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[attacker_h]->m_map_index,
			m_game->m_client_list[attacker_h]->m_x, m_game->m_client_list[attacker_h]->m_y, item);
	}
	else if (type == 1) {
		m_game->m_map_list[m_game->m_npc_list[target_h]->m_map_index]->set_item(m_game->m_npc_list[target_h]->m_x, m_game->m_npc_list[target_h]->m_y, item);
		m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_npc_list[target_h]->m_map_index,
			m_game->m_npc_list[target_h]->m_x, m_game->m_npc_list[target_h]->m_y, item);
	}

}
