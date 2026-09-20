#include "MagicManager.h"
#include "Game.h"
#include <algorithm>
#include <format>
#include "StatusEffectManager.h"
#include "Item.h"
#include "CombatManager.h"
#include "EntityManager.h"
#include "DynamicObjectManager.h"
#include "DelayEventManager.h"
#include "ItemManager.h"
#include "SkillManager.h"
#include "Packet/SharedPackets.h"
#include "ObjectIDRange.h"
#include "Skill.h"
#include "GameConfigSqliteStore.h"
#include "Log.h"
#include "ServerLogChannels.h"

using namespace hb::shared::net;

using hb::log_channel;
using namespace hb::shared::action;
using namespace hb::server::net;
using namespace hb::server::config;
using namespace hb::shared::item;
using namespace hb::shared::direction;
namespace sock = hb::shared::net::socket;
namespace dynamic_object = hb::shared::dynamic_object;
namespace smap = hb::server::map;
namespace sdelay = hb::server::delay_event;
using namespace hb::server::npc;
using namespace hb::server::skill;

extern char G_cTxt[512];
extern char G_cData50000[50000];

static int _tmp_iMCProb[] = { 0, 300, 250, 200, 150, 100, 80, 70, 60, 50, 40 };
static int _tmp_iMLevelPenalty[] = { 0,   5,   5,   8,   8,  10, 14, 28, 32, 36, 40 };

bool MagicManager::send_client_magic_configs(int client_h)
{
	if (m_game->m_client_list[client_h] == 0) {
		return false;
	}

	constexpr size_t maxPacketSize = 7000;
	constexpr size_t headerSize = sizeof(hb::net::PacketMagicConfigHeader);
	constexpr size_t entrySize = sizeof(hb::net::PacketMagicConfigEntry);
	constexpr size_t maxEntriesPerPacket = (maxPacketSize - headerSize) / entrySize;

	// Count total magics
	int totalMagics = 0;
	for(int i = 0; i < hb::shared::limits::MaxMagicType; i++) {
		if (m_game->m_magic_config_list[i] != 0) {
			totalMagics++;
		}
	}

	// Send magics in packets
	int magicsSent = 0;
	int packetIndex = 0;

	while (magicsSent < totalMagics) {
		std::memset(G_cData50000, 0, sizeof(G_cData50000));

		auto* pktHeader = reinterpret_cast<hb::net::PacketMagicConfigHeader*>(G_cData50000);
		pktHeader->header.msg_id = MsgId::MagicConfigContents;
		pktHeader->header.msg_type = MsgType::Confirm;
		pktHeader->totalMagics = static_cast<uint16_t>(totalMagics);
		pktHeader->packetIndex = static_cast<uint16_t>(packetIndex);

		auto* entries = reinterpret_cast<hb::net::PacketMagicConfigEntry*>(G_cData50000 + headerSize);

		uint16_t entriesInPacket = 0;
		int skipped = 0;

		for(int i = 0; i < hb::shared::limits::MaxMagicType && entriesInPacket < maxEntriesPerPacket; i++) {
			if (m_game->m_magic_config_list[i] == 0) {
				continue;
			}

			if (skipped < magicsSent) {
				skipped++;
				continue;
			}

			const CMagic* magic = m_game->m_magic_config_list[i];
			auto& entry = entries[entriesInPacket];

			entry.magicId = static_cast<int16_t>(i);
			std::memset(entry.name, 0, sizeof(entry.name));
			std::snprintf(entry.name, sizeof(entry.name), "%s", magic->m_name);
			entry.manaCost = magic->m_value_1;
			entry.intLimit = magic->m_intelligence_limit;
			entry.goldCost = magic->m_gold_cost;
			entry.visible = (magic->m_gold_cost >= 0) ? 1 : 0;
			entry.magicType = magic->m_type;
			entry.aoeRadiusX = magic->m_value_2;
			entry.aoeRadiusY = magic->m_value_3;
			entry.dynamicPattern = magic->m_value_11;
			entry.dynamicRadius = magic->m_value_12;

			entriesInPacket++;
		}

		pktHeader->magicCount = entriesInPacket;
		size_t packetSize = headerSize + (entriesInPacket * entrySize);

		int ret = m_game->m_client_list[client_h]->m_socket->send_msg(G_cData50000, static_cast<int>(packetSize));
		switch (ret) {
		case sock::Event::QueueFull:
		case sock::Event::SocketError:
		case sock::Event::CriticalError:
		case sock::Event::SocketClosed:
			hb::logger::log("Failed to send magic configs: Client({}) Packet({})", client_h, packetIndex);
			m_game->delete_client(client_h, true, true);
			delete m_game->m_client_list[client_h];
			m_game->m_client_list[client_h] = 0;
			return false;
		}

		magicsSent += entriesInPacket;
		packetIndex++;
	}

	return true;
}

int MagicManager::client_motion_magic_handler(int client_h, short sX, short sY, direction dir)
{
	int     ret;

	if (m_game->m_client_list[client_h] == 0) return 0;
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

	if (m_game->m_client_list[client_h]->m_status.invisibility) {
		m_game->m_status_effect_manager->set_invisibility_flag(client_h, hb::shared::owner_class::Player, false);
		m_game->m_delay_event_manager->remove_from_delay_event_list(client_h, hb::shared::owner_class::Player, hb::shared::magic::Invisibility);
		m_game->m_client_list[client_h]->m_magic_effect_status[hb::shared::magic::Invisibility] = 0;
	}

	m_game->m_client_list[client_h]->m_dir = dir;

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

void MagicManager::player_magic_handler(int client_h, int dX, int dY, short type, bool item_effect, int v1, uint16_t targetObjectID)
{
	short sX, sY, owner_h, magic_circle, rx, ry, level_magic;
	direction dir;
	char owner_type, name[hb::shared::limits::CharNameLen], item_name[hb::shared::limits::ItemNameLen], npc_waypoint[11], cName_Master[hb::shared::limits::CharNameLen], scan_message[256];
	double dv1, dv2, dv3, dv4;
	int err, ret, result, dice_res, naming_value, followers_num, erase_req, whether_bonus;
	int tX, tY, mana_cost, magic_attr;
	CItem* item;
	uint32_t time;
	short eq_status;
	int map_side = 0;

	time = GameClock::GetTimeMS();
	m_game->m_client_list[client_h]->m_magic_confirm = true;
	m_game->m_client_list[client_h]->m_magic_pause_time = false;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;

	int casterX = m_game->m_client_list[client_h]->m_x;
	int casterY = m_game->m_client_list[client_h]->m_y;

	// Viewport clamp: clamp target to the caster's visible area instead of cancelling
	// Y is asymmetric: CenterY = RangeY + 1, so top extends 1 tile further than RangeY
	dX = std::clamp(dX, casterX - MaxCastRangeX, casterX + MaxCastRangeX);
	dY = std::clamp(dY, casterY - MaxCastRangeY - 1, casterY + MaxCastRangeY);

	// Auto-aim: snap to target's current server-side position to compensate for latency
	if (targetObjectID != 0)
	{
		int targetMapIndex = m_game->m_client_list[client_h]->m_map_index;
		int targetX = -1, targetY = -1;

		if (hb::shared::object_id::is_player_id(targetObjectID))
		{
			if ((targetObjectID > 0) && (targetObjectID < MaxClients) && m_game->m_client_list[targetObjectID] != nullptr)
			{
				if (m_game->m_client_list[targetObjectID]->m_map_index == targetMapIndex)
				{
					targetX = m_game->m_client_list[targetObjectID]->m_x;
					targetY = m_game->m_client_list[targetObjectID]->m_y;
				}
			}
		}
		else
		{
			int npcIndex = hb::shared::object_id::ToNpcIndex(targetObjectID);
			if ((npcIndex > 0) && (npcIndex < MaxNpcs) && m_game->m_npc_list[npcIndex] != nullptr)
			{
				if (m_game->m_npc_list[npcIndex]->m_map_index == targetMapIndex)
				{
					targetX = m_game->m_npc_list[npcIndex]->m_x;
					targetY = m_game->m_npc_list[npcIndex]->m_y;
				}
			}
		}

		if (targetX >= 0 && targetY >= 0)
		{
			int distX = abs(targetX - dX);
			int distY = abs(targetY - dY);
			// Only snap if target is within auto-aim range AND still inside the caster's viewport
			if (distX <= MaxAutoAimRange && distY <= MaxAutoAimRange &&
				abs(targetX - casterX) <= MaxCastRangeX &&
				targetY >= casterY - MaxCastRangeY - 1 && targetY <= casterY + MaxCastRangeY)
			{
				dX = targetX;
				dY = targetY;
			}
		}
	}

	if ((dX < 0) || (dX >= m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_size_x) ||
		(dY < 0) || (dY >= m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_size_y)) return;

	if (((time - m_game->m_client_list[client_h]->m_recent_attack_time) < 1000) && (item_effect == 0)) {
		hb::logger::warn<log_channel::security>("Fast cast detection: IP={} player={}, magic casting too fast", m_game->m_client_list[client_h]->m_ip_address, m_game->m_client_list[client_h]->m_char_name);
		m_game->m_client_list[client_h]->m_magic_confirm = false;
		return;
	}
	m_game->m_client_list[client_h]->m_recent_attack_time = time;
	m_game->m_client_list[client_h]->m_last_action_time = time;

	if (m_game->m_client_list[client_h]->m_map_index < 0) return;
	if (m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index] == 0) return;

	if ((type < 0) || (type >= 100))     return;
	if (m_game->m_magic_config_list[type] == 0) return;

	if ((item_effect == false) && (m_game->m_client_list[client_h]->m_magic_mastery[type] != 1)) return;

	// Direct INT check at cast time — reject if player no longer meets the spell's INT requirement
	if (item_effect == false && m_game->m_magic_config_list[type]->m_intelligence_limit >
		(m_game->m_client_list[client_h]->m_int + m_game->m_client_list[client_h]->m_angelic_int))
	{
		m_game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0,
			"Insufficient Intelligence to cast this spell.");
		return;
	}

	// Only block offensive magic in no-attack zones; allow friendly spells (healing, buffs, teleport, create, etc.)
	if ((m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_is_attack_enabled == false)
		&& type != 14)
	{

		switch (m_game->m_magic_config_list[type]->m_type)
		{
		case hb::shared::magic::DamageSpot:
		case hb::shared::magic::DamageArea:
		case hb::shared::magic::SpDownSpot:
		case hb::shared::magic::SpDownArea:
		case hb::shared::magic::HoldObject:
		case hb::shared::magic::Possession:
		case hb::shared::magic::Confuse:
		case hb::shared::magic::Poison:
		case hb::shared::magic::DamageLinear:
		case hb::shared::magic::DamageAreaNoSpot:
		case hb::shared::magic::Tremor:
		case hb::shared::magic::Ice:
		case hb::shared::magic::DamageAreaNoSpotSpDown:
		case hb::shared::magic::IceLinear:
		case hb::shared::magic::DamageAreaArmorBreak:
		case hb::shared::magic::DamageLinearSpDown:
		case hb::shared::magic::Inhibition:
			return;
		}
	}
	//if ((var_874 ) && (m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_is_heldenian_map ) && (m_game->m_magic_config_list[type]->m_type != 8)) return;

	if ((m_game->m_client_list[client_h]->m_status.inhibition_casting) && (item_effect != true)) {
		m_game->send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Damage, Sentinel::MagicFailed, -1, 0);
		return;
	}

	if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)] != -1) {
		auto weapon_class = m_game->m_client_list[client_h]->get_equipped_weapon_class();
		if (weapon_class != hb::shared::item::weapon_class::wand) return;
	}

	if ((m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::LeftHand)] != -1) ||
		(m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)] != -1)) return;

	// Reject spell if the cast was interrupted by damage (sentinel value -1)
	if ((item_effect == false) && (m_game->m_client_list[client_h]->m_spell_count == -1)) {
		m_game->m_client_list[client_h]->m_spell_count = 0;
		return;
	}

	if ((m_game->m_client_list[client_h]->m_spell_count > 1) && (item_effect == false)) {
		hb::logger::warn<log_channel::security>("Spell hack: IP={} player={}, casting without precast", m_game->m_client_list[client_h]->m_ip_address, m_game->m_client_list[client_h]->m_char_name);
		m_game->m_client_list[client_h]->m_spell_count = 0;
		m_game->m_client_list[client_h]->m_magic_confirm = false;
		return;
	}

	if (m_game->m_client_list[client_h]->m_inhibition) {
		m_game->send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Damage, Sentinel::MagicFailed, -1, 0);
		return;
	}

	/*if (((m_game->m_client_list[client_h]->m_iUninteruptibleCheck - (m_game->get_max_hp(client_h)/10)) > (m_game->m_client_list[client_h]->m_hp)) && (m_game->m_client_list[client_h]->m_magic_item == false)) {
		m_game->send_event_to_near_client_type_b(MsgId::EventCommon, CommonType::Magic, 0,
			0, 0, 0, 0, 0, 0);
		return;
	}*/

	if (m_game->m_magic_config_list[type]->m_type == 32) { // Invisiblity
		eq_status = m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)];
		if ((eq_status != -1) && (m_game->m_client_list[client_h]->m_item_list[eq_status] != 0)) {
			if ((m_game->m_client_list[client_h]->m_item_list[eq_status]->m_id_num == 865) || (m_game->m_client_list[client_h]->m_item_list[eq_status]->m_id_num == 866)) {
				item_effect = true;
			}
		}
	}

	sX = m_game->m_client_list[client_h]->m_x;
	sY = m_game->m_client_list[client_h]->m_y;

	magic_circle = (type / 10) + 1;
	if (m_game->m_client_list[client_h]->m_skill_mastery[4] == 0)
		dv1 = 1.0f;
	else dv1 = (double)m_game->m_client_list[client_h]->m_skill_mastery[4];

	if (item_effect) dv1 = (double)100.0f;
	dv2 = (double)(dv1 / 100.0f);
	dv3 = (double)_tmp_iMCProb[magic_circle];
	dv1 = dv2 * dv3;
	result = (int)dv1;

	if ((m_game->m_client_list[client_h]->m_int + m_game->m_client_list[client_h]->m_angelic_int) > 50)
		result += ((m_game->m_client_list[client_h]->m_int + m_game->m_client_list[client_h]->m_angelic_int) - 50) / 2;

	level_magic = (m_game->m_client_list[client_h]->m_level / 10);
	if (magic_circle != level_magic) {
		if (magic_circle > level_magic) {
			dv1 = (double)(m_game->m_client_list[client_h]->m_level - level_magic * 10);
			dv2 = (double)abs(magic_circle - level_magic) * _tmp_iMLevelPenalty[magic_circle];
			dv3 = (double)abs(magic_circle - level_magic) * 10;
			dv4 = (dv1 / dv3) * dv2;
			result -= abs(abs(magic_circle - level_magic) * _tmp_iMLevelPenalty[magic_circle] - (int)dv4);
		}
		else {
			result += 5 * abs(magic_circle - level_magic);
		}
	}

	switch (m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_weather_status) {
	case 0: break;
	case 1: result = result - (result / 24); break;
	case 2:	result = result - (result / 12); break;
	case 3: result = result - (result / 5);  break;
	}

	if (m_game->m_client_list[client_h]->m_special_weapon_effect_type == 10) {
		dv1 = (double)result;
		dv2 = (double)(m_game->m_client_list[client_h]->m_special_weapon_effect_value * m_game->m_prefix_multiplier[10]);
		dv3 = dv1 + dv2;
		result = (int)dv3;
	}

	if (result <= 0) result = 1;

	whether_bonus = get_weather_magic_bonus_effect(type, m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_weather_status);

	mana_cost = m_game->m_magic_config_list[type]->m_value_1;
	if ((m_game->m_client_list[client_h]->m_is_safe_attack_mode) &&
		(m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_is_fight_zone == false)) {
		mana_cost += (mana_cost / 2) - (mana_cost / 10);
	}

	if (m_game->m_client_list[client_h]->m_mana_save_ratio > 0) {
		dv1 = (double)m_game->m_client_list[client_h]->m_mana_save_ratio;
		dv2 = (double)(dv1 / 100.0f);
		dv3 = (double)mana_cost;
		dv1 = dv2 * dv3;
		dv2 = dv3 - dv1;
		mana_cost = (int)dv2;

		if (mana_cost <= 0) mana_cost = 1;
	}

	// Cleanup lambda: deduct mana, notify client, send magic event.
	// Replaces the former MAGIC_NOEFFECT goto label.
	auto magic_noeffect = [&]() {
		if (m_game->m_client_list[client_h] == 0) return;

		//Mana Slate
		if (m_game->m_client_list[client_h]->m_status.slate_mana) {
			mana_cost = 0;
		}

		// Mana  .
		m_game->m_client_list[client_h]->m_mp -= mana_cost; // value1 Mana Cost
		if (m_game->m_client_list[client_h]->m_mp < 0)
			m_game->m_client_list[client_h]->m_mp = 0;

		m_game->m_skill_manager->calculate_ssn_skill_index(client_h, 4, 1);

		m_game->send_notify_msg(0, client_h, Notify::Mp, 0, 0, 0, 0);

		// .  + 100
		m_game->send_event_to_near_client_type_b(MsgId::EventCommon, CommonType::Magic, m_game->m_client_list[client_h]->m_map_index,
			m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, dX, dY, (type + 100), m_game->m_client_list[client_h]->m_type);
	};

	if (result < 100) {
		dice_res = m_game->dice(1, 100);
		if (result < dice_res) {
			m_game->send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Damage, Sentinel::MagicFailed, -1, 0);
			return;
		}
	}

	if (((m_game->m_client_list[client_h]->m_hunger_status <= 10) || (m_game->m_client_list[client_h]->m_sp <= 0)) && (m_game->dice(1, 1000) <= 100)) {
		m_game->send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Damage, Sentinel::MagicFailed, -1, 0);
		return;
	}

	if (m_game->m_client_list[client_h]->m_mp < mana_cost) {
		return;
	}

	result = m_game->m_client_list[client_h]->m_skill_mastery[4];
	if ((m_game->m_client_list[client_h]->m_mag + m_game->m_client_list[client_h]->m_angelic_mag) > 50) result += ((m_game->m_client_list[client_h]->m_mag + m_game->m_client_list[client_h]->m_angelic_mag) - 50);

	level_magic = (m_game->m_client_list[client_h]->m_level / 10);
	if (magic_circle != level_magic) {
		if (magic_circle > level_magic) {
			dv1 = (double)(m_game->m_client_list[client_h]->m_level - level_magic * 10);
			dv2 = (double)abs(magic_circle - level_magic) * _tmp_iMLevelPenalty[magic_circle];
			dv3 = (double)abs(magic_circle - level_magic) * 10;
			dv4 = (dv1 / dv3) * dv2;

			result -= abs(abs(magic_circle - level_magic) * _tmp_iMLevelPenalty[magic_circle] - (int)dv4);
		}
		else {
			result += 5 * abs(magic_circle - level_magic);
		}
	}

	result += m_game->m_client_list[client_h]->m_add_attack_ratio;
	if (result <= 0) result = 1;

	if (type >= 80) result += 10000;

	if (m_game->m_magic_config_list[type]->m_type == 28) {
		result += 10000;
	}

	if (m_game->m_magic_config_list[type]->m_category == 1) {
		if (m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_attribute(sX, sY, 0x00000005) != 0) return;
	}

	magic_attr = m_game->m_magic_config_list[type]->m_attribute;
	if (m_game->m_client_list[client_h]->m_status.invisibility) {
		m_game->m_status_effect_manager->set_invisibility_flag(client_h, hb::shared::owner_class::Player, false);
		m_game->m_delay_event_manager->remove_from_delay_event_list(client_h, hb::shared::owner_class::Player, hb::shared::magic::Invisibility);
		m_game->m_client_list[client_h]->m_magic_effect_status[hb::shared::magic::Invisibility] = 0;
	}

	m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
	if ((m_game->m_is_crusade_mode == false) && (owner_type == hb::shared::owner_class::Player)) {
		if ((m_game->m_client_list[client_h]->m_is_player_civil != true) && (m_game->m_client_list[owner_h]->m_is_player_civil)) {
			if (m_game->m_client_list[client_h]->m_side != m_game->m_client_list[owner_h]->m_side) return;
		}
		else if ((m_game->m_client_list[client_h]->m_is_player_civil) && (m_game->m_client_list[owner_h]->m_is_player_civil == false)) {
			switch (m_game->m_magic_config_list[type]->m_type) {
			case 1:  // hb::shared::magic::DamageSpot
			case 4:  // hb::shared::magic::SpDownSpot 4
			case 8:  // hb::shared::magic::Teleport 8
			case 10: // hb::shared::magic::Create 10
			case 11: // hb::shared::magic::Protect 11
			case 12: // hb::shared::magic::HoldObject 12
			case 16: // hb::shared::magic::Confuse
			case 17: // hb::shared::magic::Poison
			case 32: // hb::shared::magic::Resurrection
			case hb::shared::magic::Haste:
				return;
			}
		}
	}

	if (m_game->m_magic_config_list[type]->m_delay_time == 0) {
		switch (m_game->m_magic_config_list[type]->m_type) {
		case hb::shared::magic::Haste:
			switch (m_game->m_magic_config_list[type]->m_value_4) {
			case 1:
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);

				switch (owner_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
					if (owner_h == client_h) { magic_noeffect(); return; }
					if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Haste] != 0) { magic_noeffect(); return; }
					m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Haste] = (char)m_game->m_magic_config_list[type]->m_value_4;
					m_game->m_status_effect_manager->set_haste_flag(owner_h, owner_type, true);
					break;

				case hb::shared::owner_class::Npc:
					{ magic_noeffect(); return; }
					break;
				}
				m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Haste, time + (m_game->m_magic_config_list[type]->m_last_time * 1000),
					owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

				if (owner_type == hb::shared::owner_class::Player)
					m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Haste, m_game->m_magic_config_list[type]->m_value_4, 0, 0);
				break;
			}
			break;
		case hb::shared::magic::DamageSpot:
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
				m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);

			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, dX, dY);
			if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) && (m_game->m_client_list[owner_h]->m_hp > 0)) {
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
			}
			break;

		case hb::shared::magic::HpUpSpot:
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			m_game->m_combat_manager->effect_hp_up_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6);
			break;

		case hb::shared::magic::DamageArea:
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
				m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);

			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, dX, dY);
			if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) && (m_game->m_client_list[owner_h]->m_hp > 0)) {
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
			}

			for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
				for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, ix, iy);
					if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) && (m_game->m_client_list[owner_h]->m_hp > 0)) {
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
							m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					}
				}
			break;

		case hb::shared::magic::SpDownSpot:
			break;

		case hb::shared::magic::SpDownArea:
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
				m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6);
			for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
				for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
				}
			break;

		case hb::shared::magic::Polymorph:
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (1) { // m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
				switch (owner_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
					if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Polymorph] != 0) { magic_noeffect(); return; }
					m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Polymorph] = (char)m_game->m_magic_config_list[type]->m_value_4;
					m_game->m_client_list[owner_h]->m_original_type = m_game->m_client_list[owner_h]->m_type;
					m_game->m_client_list[owner_h]->m_type = 18;
					m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
					break;

				case hb::shared::owner_class::Npc:
					if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
					if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Polymorph] != 0) { magic_noeffect(); return; }
					m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Polymorph] = (char)m_game->m_magic_config_list[type]->m_value_4;
					m_game->m_npc_list[owner_h]->m_original_type = m_game->m_npc_list[owner_h]->m_type;
					m_game->m_npc_list[owner_h]->m_type = 18;
					m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Npc, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
					break;
				}

				m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Polymorph, time + (m_game->m_magic_config_list[type]->m_last_time * 1000),
					owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

				if (owner_type == hb::shared::owner_class::Player)
					m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Polymorph, m_game->m_magic_config_list[type]->m_value_4, 0, 0);
			}
			break;

			// 05/20/2004 - Hypnotoad - Cancellation
		case hb::shared::magic::Cancellation:
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) && (m_game->m_client_list[owner_h]->m_hp > 0)) {

				// Removes Invisibility Flag 0x0010
				m_game->m_status_effect_manager->set_invisibility_flag(owner_h, owner_type, false);

				// Removes Illusion Flag 0x01000000
				m_game->m_status_effect_manager->set_illusion_flag(owner_h, owner_type, false);

				// Removes Defense Shield Flag 0x02000000
				// Removes Great Defense Shield Flag 0x02000000
				m_game->m_status_effect_manager->set_defense_shield_flag(owner_h, owner_type, false);

				// Removes Absolute Magic Protection Flag 0x04000000	
				// Removes Protection From Magic Flag 0x04000000
				m_game->m_status_effect_manager->set_magic_protection_flag(owner_h, owner_type, false);

				// Removes Protection From Arrow Flag 0x08000000
				m_game->m_status_effect_manager->set_protection_from_arrow_flag(owner_h, owner_type, false);

				// Removes Illusion Movement Flag 0x00200000
				m_game->m_status_effect_manager->set_illusion_movement_flag(owner_h, owner_type, false);

				// Removes Berserk Flag 0x0020
				m_game->m_status_effect_manager->set_berserk_flag(owner_h, owner_type, false);

				//Removes ice-added 
				m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, false);

				//Remove paralyse

				m_game->m_delay_event_manager->remove_from_delay_event_list(owner_h, hb::shared::owner_class::Player, hb::shared::magic::Ice);
				m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_last_time),
					owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

				m_game->m_delay_event_manager->remove_from_delay_event_list(owner_h, hb::shared::owner_class::Player, hb::shared::magic::HoldObject);
				m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::HoldObject, time + (m_game->m_magic_config_list[type]->m_last_time),
					owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

				m_game->m_delay_event_manager->remove_from_delay_event_list(owner_h, hb::shared::owner_class::Player, hb::shared::magic::Inhibition);
				m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Inhibition, time + (m_game->m_magic_config_list[type]->m_last_time),
					owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

				m_game->m_delay_event_manager->remove_from_delay_event_list(owner_h, hb::shared::owner_class::Player, hb::shared::magic::Invisibility);
				m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Invisibility, time + (m_game->m_magic_config_list[type]->m_last_time),
					owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

				m_game->m_delay_event_manager->remove_from_delay_event_list(owner_h, hb::shared::owner_class::Player, hb::shared::magic::Berserk);
				m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Berserk, time + (m_game->m_magic_config_list[type]->m_last_time),
					owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

				m_game->m_delay_event_manager->remove_from_delay_event_list(owner_h, hb::shared::owner_class::Player, hb::shared::magic::Protect);
				m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Protect, time + (m_game->m_magic_config_list[type]->m_last_time),
					owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

				m_game->m_delay_event_manager->remove_from_delay_event_list(owner_h, hb::shared::owner_class::Player, hb::shared::magic::Confuse);
				m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Confuse, time + (m_game->m_magic_config_list[type]->m_last_time),
					owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

				// Update Client
				m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
			}
			break;

		case hb::shared::magic::DamageAreaNoSpotSpDown:
			for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
				for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, false, magic_attr);
						m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
					}

					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, ix, iy);
					if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
						(m_game->m_client_list[owner_h]->m_hp > 0)) {
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
							m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, false, magic_attr);
							m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
						}
					}
				}
			break;

		case hb::shared::magic::DamageLinear:
			sX = m_game->m_client_list[client_h]->m_x;
			sY = m_game->m_client_list[client_h]->m_y;

			for(int i = 2; i < 10; i++) {
				err = 0;
				CMisc::GetPoint2(sX, sY, dX, dY, &tX, &tY, &err, i);

				// tx, ty
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX, tY);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, sX, sY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX, tY);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
				}

				// tx-1, ty
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX - 1, tY);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, sX, sY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX - 1, tY);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
				}

				// tx+1, ty
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX + 1, tY);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, sX, sY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX + 1, tY);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
				}

				// tx, ty-1
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX, tY - 1);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, sX, sY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX, tY - 1);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
				}

				// tx, ty+1
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX, tY + 1);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, sX, sY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX, tY + 1);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
				}

				if ((abs(tX - dX) <= 1) && (abs(tY - dY) <= 1)) break;
			}

			for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
				for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, ix, iy);
					if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
						(m_game->m_client_list[owner_h]->m_hp > 0)) {
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
							m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					}
				}

			// dX, dY
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
				m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr); // v1.41 false

			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, dX, dY);
			if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
				(m_game->m_client_list[owner_h]->m_hp > 0)) {
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr); // v1.41 false
			}
			break;

		case hb::shared::magic::IceLinear:
			sX = m_game->m_client_list[client_h]->m_x;
			sY = m_game->m_client_list[client_h]->m_y;

			for(int i = 2; i < 10; i++) {
				err = 0;
				CMisc::GetPoint2(sX, sY, dX, dY, &tX, &tY, &err, i);

				// tx, ty
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX, tY);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
					m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, sX, sY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					switch (owner_type) {
					case hb::shared::owner_class::Player:
						if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
						if (m_game->m_client_list[owner_h]->m_hp < 0) { magic_noeffect(); return; }
						if ((m_game->m_client_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
							if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
								m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
								m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
								m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
									owner_h, owner_type, 0, 0, 0, 1, 0, 0);
								m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
							}
						}
						break;

					case hb::shared::owner_class::Npc:
						if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
						if ((m_game->m_npc_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
							if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
								m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
								m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
								m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
									owner_h, owner_type, 0, 0, 0, 1, 0, 0);
							}
						}
						break;
					}
				}

				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX, tY);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
						switch (owner_type) {
						case hb::shared::owner_class::Player:
							if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_client_list[owner_h]->m_is_gm_mode) break;
							if ((m_game->m_client_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
								if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
									m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
									m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
									m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
										owner_h, owner_type, 0, 0, 0, 1, 0, 0);
									m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
								}
							}
							break;

						case hb::shared::owner_class::Npc:
							if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
							if ((m_game->m_npc_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
								if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
									m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
									m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
									m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
										owner_h, owner_type, 0, 0, 0, 1, 0, 0);
								}
							}
							break;
						}
					}
				}

				// tx-1, ty
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX - 1, tY);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
					m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, sX, sY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					switch (owner_type) {
					case hb::shared::owner_class::Player:
						if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
						if ((m_game->m_client_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
							if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
								m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
								m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
								m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
									owner_h, owner_type, 0, 0, 0, 1, 0, 0);
								m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
							}
						}
						break;

					case hb::shared::owner_class::Npc:
						if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
						if ((m_game->m_npc_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
							if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
								m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
								m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
								m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
									owner_h, owner_type, 0, 0, 0, 1, 0, 0);
							}
						}
						break;
					}
				}

				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX - 1, tY);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
						switch (owner_type) {
						case hb::shared::owner_class::Player:
							if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_client_list[owner_h]->m_is_gm_mode) break;
							if ((m_game->m_client_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
								if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
									m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
									m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
									m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
										owner_h, owner_type, 0, 0, 0, 1, 0, 0);
									m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
								}
							}
							break;

						case hb::shared::owner_class::Npc:
							if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
							if ((m_game->m_npc_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
								if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
									m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
									m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
									m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
										owner_h, owner_type, 0, 0, 0, 1, 0, 0);
								}
							}
							break;
						}
					}
				}

				// tx+1, ty
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX + 1, tY);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
					m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, sX, sY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					switch (owner_type) {
					case hb::shared::owner_class::Player:
						if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
						if ((m_game->m_client_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
							if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
								m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
								m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
								m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
									owner_h, owner_type, 0, 0, 0, 1, 0, 0);
								m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
							}
						}
						break;

					case hb::shared::owner_class::Npc:
						if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
						if ((m_game->m_npc_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
							if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
								m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
								m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
								m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
									owner_h, owner_type, 0, 0, 0, 1, 0, 0);
							}
						}
						break;
					}
				}

				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX + 1, tY);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
						switch (owner_type) {
						case hb::shared::owner_class::Player:
							if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_client_list[owner_h]->m_is_gm_mode) break;
							if ((m_game->m_client_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
								if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
									m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
									m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
									m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
										owner_h, owner_type, 0, 0, 0, 1, 0, 0);
									m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
								}
							}
							break;

						case hb::shared::owner_class::Npc:
							if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
							if ((m_game->m_npc_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
								if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
									m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
									m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
									m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
										owner_h, owner_type, 0, 0, 0, 1, 0, 0);
								}
							}
							break;
						}
					}
				}

				// tx, ty-1
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX, tY - 1);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
					m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, sX, sY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					switch (owner_type) {
					case hb::shared::owner_class::Player:
						if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
						if ((m_game->m_client_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
							if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
								m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
								m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
								m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
									owner_h, owner_type, 0, 0, 0, 1, 0, 0);
								m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
							}
						}
						break;

					case hb::shared::owner_class::Npc:
						if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
						if ((m_game->m_npc_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
							if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
								m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
								m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
								m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
									owner_h, owner_type, 0, 0, 0, 1, 0, 0);
							}
						}
						break;
					}
				}

				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX, tY - 1);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
						switch (owner_type) {
						case hb::shared::owner_class::Player:
							if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_client_list[owner_h]->m_is_gm_mode) break;
							if ((m_game->m_client_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
								if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
									m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
									m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
									m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
										owner_h, owner_type, 0, 0, 0, 1, 0, 0);
									m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
								}
							}
							break;

						case hb::shared::owner_class::Npc:
							if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
							if ((m_game->m_npc_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
								if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
									m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
									m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
									m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
										owner_h, owner_type, 0, 0, 0, 1, 0, 0);
								}
							}
							break;
						}
					}
				}

				// tx, ty+1
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX, tY + 1);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
					m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, sX, sY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					switch (owner_type) {
					case hb::shared::owner_class::Player:
						if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
						if ((m_game->m_client_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
							if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
								m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
								m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
								m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
									owner_h, owner_type, 0, 0, 0, 1, 0, 0);
								m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
							}
						}
						break;

					case hb::shared::owner_class::Npc:
						if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
						if ((m_game->m_npc_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
							if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
								m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
								m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
								m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
									owner_h, owner_type, 0, 0, 0, 1, 0, 0);
							}
						}
						break;
					}
				}

				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX, tY + 1);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
						switch (owner_type) {
						case hb::shared::owner_class::Player:
							if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_client_list[owner_h]->m_is_gm_mode) break;
							if ((m_game->m_client_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
								if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
									m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
									m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
									m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
										owner_h, owner_type, 0, 0, 0, 1, 0, 0);
									m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
								}
							}
							break;

						case hb::shared::owner_class::Npc:
							if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
							if ((m_game->m_npc_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
								if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
									m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
									m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
									m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
										owner_h, owner_type, 0, 0, 0, 1, 0, 0);
								}
							}
							break;
						}
					}
				}

				if ((abs(tX - dX) <= 1) && (abs(tY - dY) <= 1)) break;
			}

			for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
				for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
						switch (owner_type) {
						case hb::shared::owner_class::Player:
							if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_client_list[owner_h]->m_is_gm_mode) break;
							if ((m_game->m_client_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
								if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
									m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
									m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
									m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
										owner_h, owner_type, 0, 0, 0, 1, 0, 0);
									m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
								}
							}
							break;

						case hb::shared::owner_class::Npc:
							if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
							if ((m_game->m_npc_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
								if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
									m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
									m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
									m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
										owner_h, owner_type, 0, 0, 0, 1, 0, 0);
								}
							}
							break;
						}
					}

					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, ix, iy);
					if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
						(m_game->m_client_list[owner_h]->m_hp > 0)) {
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
							m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
							switch (owner_type) {
							case hb::shared::owner_class::Player:
								if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
								if ((m_game->m_client_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
									if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
										m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
										m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
										m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
											owner_h, owner_type, 0, 0, 0, 1, 0, 0);
										m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
									}
								}
								break;

							case hb::shared::owner_class::Npc:
								if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
								if ((m_game->m_npc_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
									if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
										m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
										m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
										m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
											owner_h, owner_type, 0, 0, 0, 1, 0, 0);
									}
								}
								break;
							}
						}
					}
				}

			// dX, dY
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
				m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr); // v1.41 false
				switch (owner_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
					if ((m_game->m_client_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
						if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
							m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
							m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
							m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
								owner_h, owner_type, 0, 0, 0, 1, 0, 0);
							m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
						}
					}
					break;

				case hb::shared::owner_class::Npc:
					if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
					if ((m_game->m_npc_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
						if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
							m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
							m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
							m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
								owner_h, owner_type, 0, 0, 0, 1, 0, 0);
						}
					}
					break;
				}
			}

			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, dX, dY);
			if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
				(m_game->m_client_list[owner_h]->m_hp > 0)) {
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
					m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr); // v1.41 false
					switch (owner_type) {
					case hb::shared::owner_class::Player:
						if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
						if ((m_game->m_client_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
							if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
								m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
								m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
								m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
									owner_h, owner_type, 0, 0, 0, 1, 0, 0);
								m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
							}
						}
						break;

					case hb::shared::owner_class::Npc:
						if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
						if ((m_game->m_npc_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
							if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
								m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
								m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
								m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
									owner_h, owner_type, 0, 0, 0, 1, 0, 0);
							}
						}
						break;
					}
				}
			}
			break;

		case hb::shared::magic::Inhibition:
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			switch (owner_type) {
			case hb::shared::owner_class::Player:
				if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
				if (m_game->m_client_list[owner_h]->m_is_gm_mode) { magic_noeffect(); return; }
				if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Inhibition] != 0) { magic_noeffect(); return; }
				if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Protect] == 5) { magic_noeffect(); return; }
				if (m_game->m_client_list[client_h]->m_side == m_game->m_client_list[owner_h]->m_side) { magic_noeffect(); return; }
				if (memcmp(m_game->m_client_list[client_h]->m_location, "NONE", 4) == 0) { magic_noeffect(); return; }

				m_game->m_client_list[owner_h]->m_inhibition = true;
				m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Inhibition, time + (m_game->m_magic_config_list[type]->m_last_time * 1000),
					owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);
				break;
			}
			break;

		case hb::shared::magic::Tremor:
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
				m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);

			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, dX, dY);
			if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
				(m_game->m_client_list[owner_h]->m_hp > 0)) {
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
					m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
			}

			for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
				for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, ix, iy);
					if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
						(m_game->m_client_list[owner_h]->m_hp > 0)) {
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
							m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					}
				}
			break;

		case hb::shared::magic::DamageAreaNoSpot:
			for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
				for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
						m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);

					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, ix, iy);
					if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
						(m_game->m_client_list[owner_h]->m_hp > 0)) {
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)
							m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					}
				}
			break;

		case hb::shared::magic::SpUpArea:
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			m_game->m_combat_manager->effect_sp_up_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6);
			for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
				for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
					m_game->m_combat_manager->effect_sp_up_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
				}
			break;

		case hb::shared::magic::DamageLinearSpDown:
			sX = m_game->m_client_list[client_h]->m_x;
			sY = m_game->m_client_list[client_h]->m_y;

			for(int i = 2; i < 10; i++) {
				err = 0;
				CMisc::GetPoint2(sX, sY, dX, dY, &tX, &tY, &err, i);

				// tx, ty
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX, tY);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
					m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, sX, sY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					switch (owner_type) {
					case hb::shared::owner_class::Player:
						if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
							m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
							m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
							m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
						}
						break;

					case hb::shared::owner_class::Npc:
						if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
							m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
							m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
							m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
						}
						break;
					}
				}

				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX, tY);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
						switch (owner_type) {
						case hb::shared::owner_class::Player:
							if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
								m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
								m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
								m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
							}
							break;

						case hb::shared::owner_class::Npc:
							if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
								m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
								m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
								m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
							}
							break;
						}
					}
				}

				// tx-1, ty
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX - 1, tY);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
					m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, sX, sY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					switch (owner_type) {
					case hb::shared::owner_class::Player:
						if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
							m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
							m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
							m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
						}
						break;

					case hb::shared::owner_class::Npc:
						if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {

							m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
							m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
							m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
						}
						break;
					}
				}

				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX - 1, tY);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
						switch (owner_type) {
						case hb::shared::owner_class::Player:
							if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
								m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
								m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
								m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
							}
							break;

						case hb::shared::owner_class::Npc:
							if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
								m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
								m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
								m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
							}
							break;
						}
					}
				}

				// tx+1, ty
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX + 1, tY);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
					m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, sX, sY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					switch (owner_type) {
					case hb::shared::owner_class::Player:
						if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
							m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
							m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
							m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
						}
						break;

					case hb::shared::owner_class::Npc:
						if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
							m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
							m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
							m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
						}
						break;
					}
				}

				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX + 1, tY);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
						switch (owner_type) {
						case hb::shared::owner_class::Player:
							if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
								m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
								m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
								m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
							}
							break;

						case hb::shared::owner_class::Npc:
							if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
								m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
								m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
								m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
							}
							break;
						}
					}
				}

				// tx, ty-1
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX, tY - 1);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
					m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, sX, sY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					switch (owner_type) {
					case hb::shared::owner_class::Player:
						if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
							m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
							m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
							m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
						}
						break;

					case hb::shared::owner_class::Npc:
						if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
							m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
							m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
							m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
						}
						break;
					}
				}

				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX, tY - 1);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
						switch (owner_type) {
						case hb::shared::owner_class::Player:
							if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
								m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
								m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
								m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
							}
							break;

						case hb::shared::owner_class::Npc:
							if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
								m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
								m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
								m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
							}
							break;
						}
					}
				}

				// tx, ty+1
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, tX, tY + 1);
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
					m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, sX, sY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
					switch (owner_type) {
					case hb::shared::owner_class::Player:
						if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
							m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
							m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
							m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
						}
						break;

					case hb::shared::owner_class::Npc:
						if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
							m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
							m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
							m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
						}
						break;
					}
				}

				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, tX, tY + 1);
				if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
					(m_game->m_client_list[owner_h]->m_hp > 0)) {
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
						switch (owner_type) {
						case hb::shared::owner_class::Player:
							if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
								m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
								m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
								m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
							}
							break;

						case hb::shared::owner_class::Npc:
							if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
								m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
								m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
								m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
							}
							break;
						}
					}
				}

				if ((abs(tX - dX) <= 1) && (abs(tY - dY) <= 1)) break;
			}

			for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
				for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
						switch (owner_type) {
						case hb::shared::owner_class::Player:
							if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
								m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
								m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
								m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
							}
							break;

						case hb::shared::owner_class::Npc:
							if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
								m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
								m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
								m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
							}
							break;
						}
					}

					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, ix, iy);
					if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
						(m_game->m_client_list[owner_h]->m_hp > 0)) {
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
							m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
							switch (owner_type) {
							case hb::shared::owner_class::Player:
								if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
								if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
									m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
									m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
									m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
								}
								break;

							case hb::shared::owner_class::Npc:
								if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
								if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
									m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
									m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
									m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
								}
								break;
							}
						}
					}
				}

			// dX, dY
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
				m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr); // v1.41 false
				switch (owner_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
						m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
						m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
					}
					break;

				case hb::shared::owner_class::Npc:
					if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
						m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
						m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
					}
					break;
				}
			}

			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, dX, dY);
			if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
				(m_game->m_client_list[owner_h]->m_hp > 0)) {
				if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
					m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr); // v1.41 false
					switch (owner_type) {
					case hb::shared::owner_class::Player:
						if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
							m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
							m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
							m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
						}
						break;

					case hb::shared::owner_class::Npc:
						if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
							m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
							m_game->m_combat_manager->effect_sp_down_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9);
							m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);

						}
						break;
					}
				}
			}
			break;

		case hb::shared::magic::Teleport:
			// . value 4    .
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);

			switch (m_game->m_magic_config_list[type]->m_value_4) {
			case 1:
				// . Recall.
				if ((owner_type == hb::shared::owner_class::Player) && (owner_h == client_h)) {
					// Recall  .
					m_game->request_teleport_handler(client_h, "1   ");
				}
				break;
			}
			break;

		case hb::shared::magic::Summon:
		{
			if (m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_is_fight_zone) return;

			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if ((owner_h == 0) || (owner_type != hb::shared::owner_class::Player)) break;

			followers_num = m_game->get_follower_number(owner_h, owner_type);
			if (followers_num >= (m_game->m_client_list[client_h]->m_skill_mastery[4] / 20))
			{
				m_game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0,
					"You cannot control any more creatures.");
				break;
			}

			naming_value = m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_empty_naming_value();
			if (naming_value == -1)
			{
				m_game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0,
					"The summoning fails - too many creatures in this area.");
				break;
			}

			// Build eligible pool from mastery-based thresholds
			int mastery = m_game->m_client_list[client_h]->m_skill_mastery[4];
			int total_weight = 0;
			for (const auto& entry : m_game->m_summon_thresholds)
			{
				if (mastery >= entry.min_mastery)
					total_weight += entry.weight;
			}

			if (total_weight <= 0)
				break;

			int roll = m_game->dice(1, total_weight);
			int cumulative = 0;
			int selected_npc_id = -1;
			for (const auto& entry : m_game->m_summon_thresholds)
			{
				if (mastery >= entry.min_mastery)
				{
					cumulative += entry.weight;
					if (roll <= cumulative)
					{
						selected_npc_id = entry.npc_id;
						break;
					}
				}
			}

			if (selected_npc_id < 0)
				break;

			std::memset(name, 0, sizeof(name));
			std::snprintf(name, sizeof(name), "XX%d", naming_value);
			name[0] = '_';
			name[1] = m_game->m_client_list[client_h]->m_map_index + 65;

			if (m_game->create_new_npc(selected_npc_id, name, m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_name, 0, 0, MoveType::Random, &dX, &dY, npc_waypoint, 0, 0, m_game->m_client_list[client_h]->m_side, false, true) == false)
			{
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_naming_value_empty(naming_value);
				m_game->send_notify_msg(0, client_h, Notify::NoticeMsg, 0, 0, 0,
					"The summoning fails.");
				break;
			}

			std::memset(cName_Master, 0, sizeof(cName_Master));
			switch (owner_type)
			{
			case hb::shared::owner_class::Player:
				memcpy(cName_Master, m_game->m_client_list[owner_h]->m_char_name, hb::shared::limits::CharNameLen - 1);
				break;
			case hb::shared::owner_class::Npc:
				memcpy(cName_Master, m_game->m_npc_list[owner_h]->m_name, 5);
				break;
			}
			if (m_game->m_entity_manager != 0) m_game->m_entity_manager->set_npc_follow_mode(name, cName_Master, owner_type);
		}
			break;

		case hb::shared::magic::Create:

			if (m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_is_move_allowed_tile(dX, dY) == false)
				{ magic_noeffect(); return; }

			item = new CItem;

			switch (m_game->m_magic_config_list[type]->m_value_4) {
			case 1:
				// Food
				if (m_game->dice(1, 2) == 1)
					std::snprintf(item_name, sizeof(item_name), "Meat");
				else std::snprintf(item_name, sizeof(item_name), "Baguette");
				break;
			}

			m_game->m_item_manager->init_item_attr(item, item_name);

			item->set_touch_effect_type(TouchEffectType::ID);
			item->m_instance.touch_effect_value1 = static_cast<short>(m_game->dice(1, 100000));
			item->m_instance.touch_effect_value2 = static_cast<short>(m_game->dice(1, 100000));
			item->m_instance.touch_effect_value3 = (short)GameClock::GetTimeMS();

			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(dX, dY, item);

			m_game->m_item_manager->item_log(ItemLogAction::Drop, client_h, (int)-1, item);

			m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index, dX, dY, item);
			break;

		case hb::shared::magic::Protect:
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);

			switch (owner_type) {
			case hb::shared::owner_class::Player:
				if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
				if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Protect] != 0) { magic_noeffect(); return; }
				if (memcmp(m_game->m_client_list[client_h]->m_location, "NONE", 4) == 0) { magic_noeffect(); return; }

				m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Protect] = (char)m_game->m_magic_config_list[type]->m_value_4;
				switch (m_game->m_magic_config_list[type]->m_value_4) {
				case 1:
					m_game->m_status_effect_manager->set_protection_from_arrow_flag(owner_h, hb::shared::owner_class::Player, true);
					break;
				case 2:
				case 5:
					m_game->m_status_effect_manager->set_magic_protection_flag(owner_h, hb::shared::owner_class::Player, true);
					break;
				case 3:
				case 4:
					m_game->m_status_effect_manager->set_defense_shield_flag(owner_h, hb::shared::owner_class::Player, true);
					break;
				}
				break;

			case hb::shared::owner_class::Npc:
				if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
				if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Protect] != 0) { magic_noeffect(); return; }
				// NPC    .
				if (m_game->m_npc_list[owner_h]->m_action_limit != 0) { magic_noeffect(); return; }
				m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Protect] = (char)m_game->m_magic_config_list[type]->m_value_4;

				switch (m_game->m_magic_config_list[type]->m_value_4) {
				case 1:
					m_game->m_status_effect_manager->set_protection_from_arrow_flag(owner_h, hb::shared::owner_class::Npc, true);
					break;
				case 2:
				case 5:
					m_game->m_status_effect_manager->set_magic_protection_flag(owner_h, hb::shared::owner_class::Npc, true);
					break;
				case 3:
				case 4:
					m_game->m_status_effect_manager->set_defense_shield_flag(owner_h, hb::shared::owner_class::Npc, true);
					break;
				}
				break;
			}

			m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Protect, time + (m_game->m_magic_config_list[type]->m_last_time * 1000),
				owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

			if (owner_type == hb::shared::owner_class::Player)
				m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Protect, m_game->m_magic_config_list[type]->m_value_4, 0, 0);
			break;

		case hb::shared::magic::Scan:
			std::memset(scan_message, 0, sizeof(scan_message));
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
				switch (owner_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
					std::snprintf(scan_message, sizeof(scan_message), " Player: %s HP:%d MP:%d.", m_game->m_client_list[owner_h]->m_char_name, m_game->m_client_list[owner_h]->m_hp, m_game->m_client_list[owner_h]->m_mp);
					m_game->show_client_msg(client_h, scan_message);
					break;

				case hb::shared::owner_class::Npc:
					if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
					std::snprintf(scan_message, sizeof(scan_message), " NPC: %s HP:%d MP:%d", m_game->m_npc_list[owner_h]->m_npc_name, m_game->m_npc_list[owner_h]->m_hp, m_game->m_npc_list[owner_h]->m_mana);
					m_game->show_client_msg(client_h, scan_message);
					break;
				}
				m_game->send_event_to_near_client_type_b(MsgId::EventCommon, CommonType::Magic, m_game->m_client_list[client_h]->m_map_index,
					m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, dX, dY, 10, (short)10);
			}
			break;

		case hb::shared::magic::HoldObject:
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {

				switch (owner_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
					if (m_game->m_client_list[owner_h]->m_is_gm_mode) { magic_noeffect(); return; }
					if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0) { magic_noeffect(); return; }
					if (m_game->m_client_list[owner_h]->m_add_poison_resistance >= 500) { magic_noeffect(); return; }
					if (memcmp(m_game->m_client_list[client_h]->m_location, "NONE", 4) == 0) { magic_noeffect(); return; }
					// 2002-09-10 #2 (No-Attack-Area)
					if (owner_type == hb::shared::owner_class::Player) {

						if (m_game->m_map_list[m_game->m_client_list[owner_h]->m_map_index]->get_attribute(sX, sY, 0x00000006) != 0) { magic_noeffect(); return; }
						if (m_game->m_map_list[m_game->m_client_list[owner_h]->m_map_index]->get_attribute(dX, dY, 0x00000006) != 0) { magic_noeffect(); return; }
					}

					// 2002-09-10 #3
					if (strcmp(m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_name, "middleland") != 0 &&
						m_game->m_is_crusade_mode == false &&
						m_game->m_client_list[client_h]->m_side == m_game->m_client_list[owner_h]->m_side)
						{ magic_noeffect(); return; }

					m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = (char)m_game->m_magic_config_list[type]->m_value_4;
					break;

				case hb::shared::owner_class::Npc:
					if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
					if (m_game->m_npc_list[owner_h]->m_magic_level >= 6) { magic_noeffect(); return; }
					if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] != 0) { magic_noeffect(); return; }
					m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::HoldObject] = (char)m_game->m_magic_config_list[type]->m_value_4;
					break;
				}

				m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::HoldObject, time + (m_game->m_magic_config_list[type]->m_last_time * 1000),
					owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

				if (owner_type == hb::shared::owner_class::Player)
					m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::HoldObject, m_game->m_magic_config_list[type]->m_value_4, 0, 0);
			}
			break;

		case hb::shared::magic::Invisibility:
			switch (m_game->m_magic_config_list[type]->m_value_4) {
			case 1:
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);

				switch (owner_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
					if ((owner_h != client_h) && ((memcmp(m_game->m_client_list[client_h]->m_location, "elvhunter", 9) == 0) || (memcmp(m_game->m_client_list[client_h]->m_location, "arehunter", 9) == 0)) && ((memcmp(m_game->m_client_list[owner_h]->m_location, "elvhunter", 9) != 0) || (memcmp(m_game->m_client_list[owner_h]->m_location, "arehunter", 9) != 0))) { magic_noeffect(); return; }
					if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] != 0) { magic_noeffect(); return; }
					if (memcmp(m_game->m_client_list[client_h]->m_location, "NONE", 4) == 0) { magic_noeffect(); return; }

					m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] = (char)m_game->m_magic_config_list[type]->m_value_4;
					m_game->m_status_effect_manager->set_invisibility_flag(owner_h, owner_type, true);
					m_game->m_combat_manager->remove_from_target(owner_h, hb::shared::owner_class::Player, hb::shared::magic::Invisibility);
					break;

				case hb::shared::owner_class::Npc:
					if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
					if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] != 0) { magic_noeffect(); return; }

					if (m_game->m_npc_list[owner_h]->m_action_limit == 0) {
						// NPC     .
						m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] = (char)m_game->m_magic_config_list[type]->m_value_4;
						m_game->m_status_effect_manager->set_invisibility_flag(owner_h, owner_type, true);
						// NPC    .
						m_game->m_combat_manager->remove_from_target(owner_h, hb::shared::owner_class::Npc, hb::shared::magic::Invisibility);
					}
					break;
				}

				m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Invisibility, time + (m_game->m_magic_config_list[type]->m_last_time * 1000),
					owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

				if (owner_type == hb::shared::owner_class::Player)
					m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Invisibility, m_game->m_magic_config_list[type]->m_value_4, 0, 0);
				break;

			case 2:
				if (memcmp(m_game->m_client_list[client_h]->m_location, "NONE", 4) == 0) { magic_noeffect(); return; }
				if ((memcmp(m_game->m_client_list[client_h]->m_location, "elvhunter", 9) == 0) || (memcmp(m_game->m_client_list[client_h]->m_location, "arehunter", 9) == 0)) { magic_noeffect(); return; }

				// dX, dY  8  Invisibility  Object   .
				for(int ix = dX - 8; ix <= dX + 8; ix++)
					for(int iy = dY - 8; iy <= dY + 8; iy++) {
						m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
						if (owner_h != 0) {
							switch (owner_type) {
							case hb::shared::owner_class::Player:
								if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
								if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] != 0) {
									m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] = 0;
									m_game->m_status_effect_manager->set_invisibility_flag(owner_h, owner_type, false);
									m_game->m_delay_event_manager->remove_from_delay_event_list(owner_h, owner_type, hb::shared::magic::Invisibility);
								}
								break;

							case hb::shared::owner_class::Npc:
								if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
								if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] != 0) {
									m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Invisibility] = 0;
									m_game->m_status_effect_manager->set_invisibility_flag(owner_h, owner_type, false);
									m_game->m_delay_event_manager->remove_from_delay_event_list(owner_h, owner_type, hb::shared::magic::Invisibility);
								}
								break;
							}
						}
					}
				break;
			}
			break;

		case hb::shared::magic::CreateDynamic:
			// Dynamic Object    .

			if (m_game->m_is_crusade_mode == false) {
				if (strcmp(m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_name, "aresden") == 0) return;
				if (strcmp(m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_name, "elvine") == 0) return;
				// v2.14
				if (strcmp(m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_name, "arefarm") == 0) return;
				if (strcmp(m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_name, "elvfarm") == 0) return;
			}

			switch (m_game->m_magic_config_list[type]->m_value_10) {
			case dynamic_object::PCloudBegin:

			case dynamic_object::Fire:   // Fire .
			case dynamic_object::Spike:  // Spike

				switch (m_game->m_magic_config_list[type]->m_value_11) {
				case 1:
					// wall - type
					dir = CMisc::get_next_move_dir(m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, dX, dY);
					switch (dir) {
					case 1:	rx = 1; ry = 0;   break;
					case 2: rx = 1; ry = 1;   break;
					case 3: rx = 0; ry = 1;   break;
					case 4: rx = -1; ry = 1;  break;
					case 5: rx = 1; ry = 0;   break;
					case 6: rx = -1; ry = -1; break;
					case 7: rx = 0; ry = -1;  break;
					case 8: rx = 1; ry = -1;  break;
					}

					m_game->m_dynamic_object_manager->add_dynamic_object_list(client_h, hb::shared::owner_class::PlayerIndirect, m_game->m_magic_config_list[type]->m_value_10, m_game->m_client_list[client_h]->m_map_index,
						dX, dY, m_game->m_magic_config_list[type]->m_last_time * 1000);

					m_game->m_combat_manager->analyze_criminal_action(client_h, dX, dY);

					for(int i = 1; i <= m_game->m_magic_config_list[type]->m_value_12; i++) {
						m_game->m_dynamic_object_manager->add_dynamic_object_list(client_h, hb::shared::owner_class::PlayerIndirect, m_game->m_magic_config_list[type]->m_value_10, m_game->m_client_list[client_h]->m_map_index,
							dX + i * rx, dY + i * ry, m_game->m_magic_config_list[type]->m_last_time * 1000);
						m_game->m_combat_manager->analyze_criminal_action(client_h, dX + i * rx, dY + i * ry);

						m_game->m_dynamic_object_manager->add_dynamic_object_list(client_h, hb::shared::owner_class::PlayerIndirect, m_game->m_magic_config_list[type]->m_value_10, m_game->m_client_list[client_h]->m_map_index,
							dX - i * rx, dY - i * ry, m_game->m_magic_config_list[type]->m_last_time * 1000);
						m_game->m_combat_manager->analyze_criminal_action(client_h, dX - i * rx, dY - i * ry);
					}
					break;

				case 2:
					// Field - Type
					bool flag = false;
					int cx, cy;
					for(int ix = dX - m_game->m_magic_config_list[type]->m_value_12; ix <= dX + m_game->m_magic_config_list[type]->m_value_12; ix++)
						for(int iy = dY - m_game->m_magic_config_list[type]->m_value_12; iy <= dY + m_game->m_magic_config_list[type]->m_value_12; iy++) {
							m_game->m_dynamic_object_manager->add_dynamic_object_list(client_h, hb::shared::owner_class::PlayerIndirect, m_game->m_magic_config_list[type]->m_value_10, m_game->m_client_list[client_h]->m_map_index,
								ix, iy, m_game->m_magic_config_list[type]->m_last_time * 1000, m_game->m_magic_config_list[type]->m_value_5);

							if (m_game->m_combat_manager->analyze_criminal_action(client_h, ix, iy, true)) {
								flag = true;
								cx = ix;
								cy = iy;
							}
						}
					if (flag) m_game->m_combat_manager->analyze_criminal_action(client_h, cx, cy);
					break;
				}
				break;

			case dynamic_object::IceStorm:
				// Ice-Storm Dynamic Object 
				m_game->m_dynamic_object_manager->add_dynamic_object_list(client_h, hb::shared::owner_class::PlayerIndirect, m_game->m_magic_config_list[type]->m_value_10, m_game->m_client_list[client_h]->m_map_index,
					dX, dY, m_game->m_magic_config_list[type]->m_last_time * 1000,
					m_game->m_client_list[client_h]->m_skill_mastery[4]);
				break;

			default:
				break;
			}
			break;

		case hb::shared::magic::Possession:
		{
			if (m_game->m_client_list[client_h]->m_side == 0) { magic_noeffect(); return; }

			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);
			if (owner_h != 0) break;

			CItem* remain = nullptr;
			item = m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_item(dX, dY, &remain);
			if (item != 0) {
				if (m_game->m_item_manager->add_client_item_list(client_h, item, &erase_req)) {

					m_game->m_item_manager->item_log(ItemLogAction::get, client_h, (int)-1, item);

					ret = m_game->m_item_manager->send_item_notify_msg(client_h, Notify::ItemObtained, item, 0);

					switch (ret) {
					case sock::Event::QueueFull:
					case sock::Event::SocketError:
					case sock::Event::CriticalError:
					case sock::Event::SocketClosed:
						m_game->delete_client(client_h, true, true);
						return;
					}

					// Broadcast remaining item state to nearby clients
					m_game->send_ground_item_event(CommonType::SetItem,
						m_game->m_client_list[client_h]->m_map_index, dX, dY, remain);
				}
				else
				{

					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(dX, dY, item);

					ret = m_game->m_item_manager->send_item_notify_msg(client_h, Notify::CannotCarryMoreItem, 0, 0);

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
			break;

		case hb::shared::magic::Confuse:
			// if the caster side is the same as the targets side, no effect occurs
			switch (m_game->m_magic_config_list[type]->m_value_4) {
			case 1: // confuse Language.
			case 2: // Confusion, Mass Confusion 	
				for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
					for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
						m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
						if (owner_type == hb::shared::owner_class::Player) {
							if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
							if ((m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) && (m_game->m_client_list[client_h]->m_side != m_game->m_client_list[owner_h]->m_side)) {
								if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Confuse] != 0) break; // Confuse  .
								m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Confuse] = (char)m_game->m_magic_config_list[type]->m_value_4;

								m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Confuse, time + (m_game->m_magic_config_list[type]->m_last_time * 1000),
									owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

								m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Confuse, m_game->m_magic_config_list[type]->m_value_4, 0, 0);
							}
						}
					}
				break;

			case 3: // Ilusion, Mass-Ilusion
				for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
					for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
						m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
						if (owner_type == hb::shared::owner_class::Player) {
							if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
							if ((m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) && (m_game->m_client_list[client_h]->m_side != m_game->m_client_list[owner_h]->m_side)) {
								if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Confuse] != 0) break; // Confuse  .
								m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Confuse] = (char)m_game->m_magic_config_list[type]->m_value_4;

								switch (m_game->m_magic_config_list[type]->m_value_4) {
								case 3:
									m_game->m_status_effect_manager->set_illusion_flag(owner_h, hb::shared::owner_class::Player, true);
									break;
								}

								m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Confuse, time + (m_game->m_magic_config_list[type]->m_last_time * 1000),
									owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

								m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Confuse, m_game->m_magic_config_list[type]->m_value_4, client_h, 0);
							}
						}
					}
				break;

			case 4: // Ilusion Movement
				if (m_game->m_client_list[client_h]->m_magic_effect_status[hb::shared::magic::Invisibility] != 0) break;
				for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
					for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
						m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
						if (owner_type == hb::shared::owner_class::Player) {
							if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
							if ((m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) && (m_game->m_client_list[client_h]->m_side != m_game->m_client_list[owner_h]->m_side)) {
								if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Confuse] != 0) break;
								m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Confuse] = (char)m_game->m_magic_config_list[type]->m_value_4;
								switch (m_game->m_magic_config_list[type]->m_value_4) {
								case 4:
									m_game->m_status_effect_manager->set_illusion_movement_flag(owner_h, hb::shared::owner_class::Player, true);
									break;
								}

								m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Confuse, time + (m_game->m_magic_config_list[type]->m_last_time * 1000),
									owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

								m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Confuse, m_game->m_magic_config_list[type]->m_value_4, client_h, 0);
							}
						}
					}
			}
			break;

		case hb::shared::magic::Poison:
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);

			if (m_game->m_magic_config_list[type]->m_value_4 == 1) {
				switch (owner_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
					if (m_game->m_client_list[owner_h]->m_is_gm_mode) { magic_noeffect(); return; }
					if (memcmp(m_game->m_client_list[client_h]->m_location, "NONE", 4) == 0) { magic_noeffect(); return; }

					m_game->m_combat_manager->analyze_criminal_action(client_h, dX, dY);

					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						if (m_game->m_combat_manager->check_resisting_poison_success(owner_h, owner_type) == false) {
							m_game->m_client_list[owner_h]->m_is_poisoned = true;
							m_game->m_client_list[owner_h]->m_poison_level = m_game->m_magic_config_list[type]->m_value_5;
							m_game->m_client_list[owner_h]->m_poison_time = time;
							// 05/06/2004 - Hypnotoad - poison aura appears when cast Poison
							m_game->m_status_effect_manager->set_poison_flag(owner_h, owner_type, true);
							m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Poison, m_game->m_magic_config_list[type]->m_value_5, 0, 0);

						}
					}
					break;

				case hb::shared::owner_class::Npc:
					if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
					if (m_game->m_npc_list[owner_h]->m_hp > 0) { magic_noeffect(); return; }
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						if (m_game->m_combat_manager->check_resisting_poison_success(owner_h, owner_type) == false) {

						}
					}
					break;
				}
			}
			else if (m_game->m_magic_config_list[type]->m_value_4 == 0) {
				switch (owner_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }

					if (m_game->m_client_list[owner_h]->m_is_poisoned) {
						m_game->m_client_list[owner_h]->m_is_poisoned = false;
						m_game->m_status_effect_manager->set_poison_flag(owner_h, owner_type, false);
						m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOff, hb::shared::magic::Poison, 0, 0, 0);
						m_game->send_notify_msg(0, owner_h, Notify::NoticeMsg, 0, 0, 0, "Poison has been cured.");
					}
					break;

				case hb::shared::owner_class::Npc:
					if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
					break;
				}
			}
			break;

		case hb::shared::magic::Berserk:
			switch (m_game->m_magic_config_list[type]->m_value_4) {
			case 1:
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, dX, dY);

				switch (owner_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
					if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Berserk] != 0) { magic_noeffect(); return; }
					m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Berserk] = (char)m_game->m_magic_config_list[type]->m_value_4;
					m_game->m_status_effect_manager->set_berserk_flag(owner_h, owner_type, true);
					break;

				case hb::shared::owner_class::Npc:
					if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
					if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Berserk] != 0) { magic_noeffect(); return; }
					if (m_game->m_npc_list[owner_h]->m_action_limit != 0) { magic_noeffect(); return; }
					// 2002-09-11 #3
					if (m_game->m_client_list[client_h]->m_side != m_game->m_npc_list[owner_h]->m_side) { magic_noeffect(); return; }

					m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Berserk] = (char)m_game->m_magic_config_list[type]->m_value_4;
					m_game->m_status_effect_manager->set_berserk_flag(owner_h, owner_type, true);
					break;
				}

				m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Berserk, time + (m_game->m_magic_config_list[type]->m_last_time * 1000),
					owner_h, owner_type, 0, 0, 0, m_game->m_magic_config_list[type]->m_value_4, 0, 0);

				if (owner_type == hb::shared::owner_class::Player)
					m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Berserk, m_game->m_magic_config_list[type]->m_value_4, 0, 0);
				break;
			}
			break;

		case hb::shared::magic::DamageAreaArmorBreak:
			hb::logger::debug<log_channel::events>("ArmorBreak: caster={} target_area=({},{}) radius=({},{}) value10={}",
				client_h, dX, dY, m_game->m_magic_config_list[type]->m_value_2, m_game->m_magic_config_list[type]->m_value_3, m_game->m_magic_config_list[type]->m_value_10);
			for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
				for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {
					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
						hb::logger::debug<log_channel::events>("ArmorBreak: calling armor_life_decrement on owner_h={} owner_type={}", owner_h, (int)owner_type);
						m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
					}

					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, ix, iy);
					if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
						(m_game->m_client_list[owner_h]->m_hp > 0)) {
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
							m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_7, m_game->m_magic_config_list[type]->m_value_8, m_game->m_magic_config_list[type]->m_value_9 + whether_bonus, false, magic_attr);
							m_game->m_combat_manager->armor_life_decrement(client_h, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_10);
						}
					}
				}
			break;

			// Resurrection Magic. 
		case hb::shared::magic::Resurrection:
			// 10 Mins once
			if (m_game->m_client_list[client_h]->m_special_ability_time != 0) { magic_noeffect(); return; }
			m_game->m_client_list[client_h]->m_special_ability_time = SpecialAbilityTimeSec / 2;
			// get the ID of the dead Player/NPC on coords dX, dY. 
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, dX, dY);
			switch (owner_type) {
				// For Player. 
			case hb::shared::owner_class::Player:
				// The Player has to exist. 
				if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
				// Resurrection is not for alive Players. 
				if (m_game->m_client_list[owner_h]->m_is_killed == false) { magic_noeffect(); return; }
				// Set Deadflag to Alive. 
				m_game->m_client_list[owner_h]->m_is_killed = false;
				// Player's HP becomes half of the Max HP. 
				m_game->m_client_list[owner_h]->m_hp = m_game->get_max_hp(owner_h) / 2;
				// Send new HP to Player. 
				m_game->send_notify_msg(0, owner_h, Notify::Hp, 0, 0, 0, 0);
				// Make Player stand up. (Currently, by a fake damage). 
				m_game->m_map_list[m_game->m_client_list[owner_h]->m_map_index]->clear_dead_owner(dX, dY);
				m_game->m_map_list[m_game->m_client_list[owner_h]->m_map_index]->set_owner(owner_h, hb::shared::owner_class::Player, dX, dY);
				m_game->send_event_to_near_client_type_a(owner_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Damage, 0, 0, 0);
				m_game->send_notify_msg(0, owner_h, Notify::Hp, 0, 0, 0, 0);
				break;
				// Resurrection is not for NPC's. 
			case hb::shared::owner_class::Npc:
				{ magic_noeffect(); return; }
				break;
			}
			break;

		case hb::shared::magic::Ice:
			for(int iy = dY - m_game->m_magic_config_list[type]->m_value_3; iy <= dY + m_game->m_magic_config_list[type]->m_value_3; iy++)
				for(int ix = dX - m_game->m_magic_config_list[type]->m_value_2; ix <= dX + m_game->m_magic_config_list[type]->m_value_2; ix++) {

					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, ix, iy);
					if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {
						//m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
						m_game->m_combat_manager->effect_damage_spot_damage_move(client_h, hb::shared::owner_class::Player, owner_h, owner_type, dX, dY, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
						switch (owner_type) {
						case hb::shared::owner_class::Player:
							if (m_game->m_client_list[owner_h] == 0) { magic_noeffect(); return; }
							if (m_game->m_client_list[owner_h]->m_is_gm_mode) break;
							if ((m_game->m_client_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
								if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
									m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
									m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
									m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
										owner_h, owner_type, 0, 0, 0, 1, 0, 0);
									m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
								}
							}
							break;

						case hb::shared::owner_class::Npc:
							if (m_game->m_npc_list[owner_h] == 0) { magic_noeffect(); return; }
							if ((m_game->m_npc_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
								if (m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
									m_game->m_npc_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
									m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
									m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
										owner_h, owner_type, 0, 0, 0, 1, 0, 0);
								}
							}
							break;
						}

					}

					m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, ix, iy);
					if ((owner_type == hb::shared::owner_class::Player) && (m_game->m_client_list[owner_h] != 0) &&
						(m_game->m_client_list[owner_h]->m_hp > 0)) {
						if (m_game->m_combat_manager->check_resisting_magic_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false) {

							//m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
							m_game->m_combat_manager->effect_damage_spot(client_h, hb::shared::owner_class::Player, owner_h, owner_type, m_game->m_magic_config_list[type]->m_value_4, m_game->m_magic_config_list[type]->m_value_5, m_game->m_magic_config_list[type]->m_value_6 + whether_bonus, true, magic_attr);
							if ((m_game->m_client_list[owner_h]->m_hp > 0) && (m_game->m_combat_manager->check_resisting_ice_success(m_game->m_client_list[client_h]->m_dir, owner_h, owner_type, result) == false)) {
								if (m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] == 0) {
									m_game->m_client_list[owner_h]->m_magic_effect_status[hb::shared::magic::Ice] = 1;
									m_game->m_status_effect_manager->set_ice_flag(owner_h, owner_type, true);
									m_game->m_delay_event_manager->register_delay_event(sdelay::Type::MagicRelease, hb::shared::magic::Ice, time + (m_game->m_magic_config_list[type]->m_value_10 * 1000),
										owner_h, owner_type, 0, 0, 0, 1, 0, 0);

									m_game->send_notify_msg(0, owner_h, Notify::MagicEffectOn, hb::shared::magic::Ice, 1, 0, 0);
								}
							}
						}
					}
				}
			break;

		default:
			break;
		}
	}
	else {
		// Casting
		// Resurrection wand(MS.10) or Resurrection wand(MS.20)

		if (m_game->m_magic_config_list[type]->m_type == hb::shared::magic::Resurrection) {
			//Check if player has resurrection wand
			if (m_game->m_client_list[client_h] != 0 && m_game->m_client_list[client_h]->m_special_ability_time == 0 &&
				m_game->m_client_list[client_h]->m_is_special_ability_enabled == false) {
				m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, dX, dY);
				if (m_game->m_client_list[owner_h] != 0) {
					// GM's can ressurect ne1, and players must be on same side to ressurect

					if (m_game->m_client_list[owner_h]->m_side != m_game->m_client_list[client_h]->m_side) {
						return;
					}
					if (owner_type == hb::shared::owner_class::Player && m_game->m_client_list[owner_h] != 0 &&
						m_game->m_client_list[owner_h]->m_hp <= 0) {
						m_game->m_client_list[owner_h]->m_is_being_resurrected = true;
						m_game->send_notify_msg(0, owner_h, Notify::ResurrectPlayer, 0, 0, 0, 0);
						m_game->m_client_list[client_h]->m_is_special_ability_enabled = true;
						m_game->m_client_list[client_h]->m_special_ability_start_time = time;
						m_game->m_client_list[client_h]->m_special_ability_last_sec = 0;
						m_game->m_client_list[client_h]->m_special_ability_time = m_game->m_magic_config_list[type]->m_delay_time;

						m_game->m_client_list[client_h]->m_appearance.effect_type = 4;
						m_game->send_notify_msg(0, client_h, Notify::SpecialAbilityStatus, 1, m_game->m_client_list[client_h]->m_special_ability_type, m_game->m_client_list[client_h]->m_special_ability_last_sec, 0);
						m_game->send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::NullAction, 0, 0, 0);
					}
				}
			}
		}
	}

	magic_noeffect();

}

void MagicManager::request_study_magic_handler(int client_h, const char* name, bool is_purchase)
{
	char magic_name[31];
	uint64_t gold_count;
	int req_int, cost, ret;
	bool magic = true;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;

	std::memset(magic_name, 0, sizeof(magic_name));
	memcpy(magic_name, name, 30);

	ret = get_magic_number(magic_name, &req_int, &cost);
	if (ret == -1) {

	}
	else {
		if (is_purchase) {
			if (m_game->m_magic_config_list[ret]->m_gold_cost < 0) magic = false;
			gold_count = m_game->m_item_manager->get_item_count_by_id(client_h, hb::shared::item::ItemId::Gold);
			if (static_cast<uint64_t>(cost) > gold_count)  magic = false;
		}
		//wizard remove
		//if (m_game->m_client_list[client_h]->m_is_inside_wizard_tower == false && is_purchase) return;
		if (m_game->m_client_list[client_h]->m_magic_mastery[ret] != 0) return;

		if ((req_int <= (m_game->m_client_list[client_h]->m_int + m_game->m_client_list[client_h]->m_angelic_int)) && (magic)) {

			if (is_purchase) m_game->m_item_manager->set_item_count_by_id(client_h, hb::shared::item::ItemId::Gold, gold_count - cost);

			m_game->calc_total_weight(client_h);

			m_game->m_client_list[client_h]->m_magic_mastery[ret] = 1;

			{

				hb::net::PacketNotifyMagicStudySuccess pkt{};
				pkt.header.msg_id = MsgId::Notify;
				pkt.header.msg_type = Notify::MagicStudySuccess;
				pkt.magic_id = static_cast<uint8_t>(ret);
				memcpy(pkt.magic_name, magic_name, sizeof(pkt.magic_name));
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
		}
		else {
			{

				hb::net::PacketNotifyMagicStudyFail pkt{};
				pkt.header.msg_id = MsgId::Notify;
				pkt.header.msg_type = Notify::MagicStudyFail;
				pkt.result = 1;
				pkt.magic_id = static_cast<uint8_t>(ret);
				memcpy(pkt.magic_name, magic_name, sizeof(pkt.magic_name));
				pkt.cost = static_cast<int32_t>(cost);
				pkt.req_int = static_cast<int32_t>(req_int);
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
		}
	}
}

int MagicManager::get_magic_number(char* magic_name, int* req_int, int* cost)
{
	
	char tmp_name[31];

	std::memset(tmp_name, 0, sizeof(tmp_name));
	strcpy(tmp_name, magic_name);

	for(int i = 0; i < hb::shared::limits::MaxMagicType; i++)
		if (m_game->m_magic_config_list[i] != 0) {
			if (memcmp(tmp_name, m_game->m_magic_config_list[i]->m_name, 30) == 0) {
				*req_int = (int)m_game->m_magic_config_list[i]->m_intelligence_limit;
				*cost = (int)m_game->m_magic_config_list[i]->m_gold_cost;

				return i;
			}
		}

	return -1;
}

int MagicManager::get_weather_magic_bonus_effect(short type, char wheather_status)
{
	int wheather_bonus;

	wheather_bonus = 0;
	switch (wheather_status) {
	case 0: break;
	case 1:
	case 2:
	case 3:
		switch (type) {
		case 10:
		case 37:
		case 43:
		case 51:
			wheather_bonus = 1;
			break;

		case 20:
		case 30:
			wheather_bonus = -1;
			break;
		}
		break;
	}
	return wheather_bonus;
}

void MagicManager::get_magic_ability_handler(int client_h)
{
	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_skill_mastery[4] != 0) return;

	m_game->m_client_list[client_h]->m_skill_mastery[4] = 20;
	m_game->send_notify_msg(0, client_h, Notify::Skill, 4, m_game->m_client_list[client_h]->m_skill_mastery[4], 0, 0);
	m_game->m_skill_manager->check_total_skill_mastery_points(client_h, 4);
}

bool MagicManager::check_client_magic_frequency(int client_h, uint32_t client_time)
{
	uint32_t time_gap;

	if (m_game->m_client_list[client_h] == 0) return false;

	if (m_game->m_client_list[client_h]->m_magic_freq_time == 0)
		m_game->m_client_list[client_h]->m_magic_freq_time = client_time;
	else {
		time_gap = client_time - m_game->m_client_list[client_h]->m_magic_freq_time;
		m_game->m_client_list[client_h]->m_magic_freq_time = client_time;

		if ((time_gap < 1500) && (m_game->m_client_list[client_h]->m_magic_confirm)) {
			hb::logger::warn<log_channel::security>("Speed cast: IP={} player={}, irregular casting rate", m_game->m_client_list[client_h]->m_ip_address, m_game->m_client_list[client_h]->m_char_name);
		}

		m_game->m_client_list[client_h]->m_spell_count--;
		if (m_game->m_client_list[client_h]->m_spell_count < 0)
			m_game->m_client_list[client_h]->m_spell_count = 0;
		m_game->m_client_list[client_h]->m_magic_confirm = false;
	}

	return false;
}

void MagicManager::reload_magic_configs()
{
	sqlite3* configDb = nullptr;
	std::string configDbPath;
	bool configDbCreated = false;
	if (!EnsureGameConfigDatabase(&configDb, configDbPath, &configDbCreated) || configDbCreated)
	{
		hb::logger::log("Magic config reload failed: gamedata.db unavailable");
		return;
	}

	for(int i = 0; i < hb::shared::limits::MaxMagicType; i++)
	{
		if (m_game->m_magic_config_list[i] != 0)
		{
			delete m_game->m_magic_config_list[i];
			m_game->m_magic_config_list[i] = 0;
		}
	}

	if (!LoadMagicConfigs(configDb, m_game))
	{
		hb::logger::log("Magic config reload failed");
		CloseGameConfigDatabase(configDb);
		return;
	}

	CloseGameConfigDatabase(configDb);
	m_game->compute_config_hashes();
	hb::logger::log("Magic configs reloaded successfully");
}
