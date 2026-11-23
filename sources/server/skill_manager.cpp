#include "skill_manager.h"
#include "game.h"
#include "item.h"
#include "item_manager.h"
#include "magic_manager.h"
#include "entity_manager.h"
#include "dynamic_object_manager.h"
#include "combat_manager.h"
#include "fishing_manager.h"
#include "packet/shared_packets.h"
#include "skill.h"
#include "game_config_sqlite_store.h"
#include "log.h"

using namespace hb::shared::net;
using namespace hb::shared::action;
using namespace hb::server::net;
using namespace hb::server::config;
using namespace hb::shared::item;
using namespace hb::shared::direction;
namespace sock = hb::shared::net::socket;
using namespace hb::server::skill;

extern char G_cTxt[512];
extern char G_cData50000[50000];

static char _tmp_cCorpseX[] = { 0,  1, 1, 1, 0, -1, -1, -1, 0, 0, 0, 0 };
static char _tmp_cCorpseY[] = { -1, -1, 0, 1, 1,  1,  0, -1, 0, 0, 0 };

bool SkillManager::send_client_skill_configs(int client_h)
{
	if (m_game->m_client_list[client_h] == 0) {
		return false;
	}

	constexpr size_t maxPacketSize = 7000;
	constexpr size_t headerSize = sizeof(hb::net::PacketSkillConfigHeader);
	constexpr size_t entrySize = sizeof(hb::net::PacketSkillConfigEntry);
	constexpr size_t maxEntriesPerPacket = (maxPacketSize - headerSize) / entrySize;

	// Count total skills
	int totalSkills = 0;
	for(int i = 0; i < hb::shared::limits::MaxSkillType; i++) {
		if (m_game->m_skill_config_list[i] != 0) {
			totalSkills++;
		}
	}

	// Send skills in packets
	int skillsSent = 0;
	int packetIndex = 0;

	while (skillsSent < totalSkills) {
		std::memset(G_cData50000, 0, sizeof(G_cData50000));

		auto* pktHeader = reinterpret_cast<hb::net::PacketSkillConfigHeader*>(G_cData50000);
		pktHeader->header.msg_id = MsgId::SkillConfigContents;
		pktHeader->header.msg_type = MsgType::Confirm;
		pktHeader->totalSkills = static_cast<uint16_t>(totalSkills);
		pktHeader->packetIndex = static_cast<uint16_t>(packetIndex);

		auto* entries = reinterpret_cast<hb::net::PacketSkillConfigEntry*>(G_cData50000 + headerSize);

		uint16_t entriesInPacket = 0;
		int skipped = 0;

		for(int i = 0; i < hb::shared::limits::MaxSkillType && entriesInPacket < maxEntriesPerPacket; i++) {
			if (m_game->m_skill_config_list[i] == 0) {
				continue;
			}

			if (skipped < skillsSent) {
				skipped++;
				continue;
			}

			const CSkill* skill = m_game->m_skill_config_list[i];
			auto& entry = entries[entriesInPacket];

			entry.skillId = static_cast<int16_t>(i);
			std::memset(entry.name, 0, sizeof(entry.name));
			std::snprintf(entry.name, sizeof(entry.name), "%s", skill->m_name);
			entry.useable = skill->m_is_useable ? 1 : 0;
			entry.useMethod = skill->m_use_method;

			entriesInPacket++;
		}

		pktHeader->skillCount = entriesInPacket;
		size_t packetSize = headerSize + (entriesInPacket * entrySize);

		int ret = m_game->m_client_list[client_h]->m_socket->send_msg(G_cData50000, static_cast<int>(packetSize));
		switch (ret) {
		case sock::Event::QueueFull:
		case sock::Event::SocketError:
		case sock::Event::CriticalError:
		case sock::Event::SocketClosed:
			hb::logger::log("Failed to send skill configs: Client({}) Packet({})", client_h, packetIndex);
			m_game->delete_client(client_h, true, true);
			delete m_game->m_client_list[client_h];
			m_game->m_client_list[client_h] = 0;
			return false;
		}

		skillsSent += entriesInPacket;
		packetIndex++;
	}

	return true;
}

void SkillManager::train_skill_response(bool success, int client_h, int skill_num, int skill_level)
{

	int   ret;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;
	if ((skill_num < 0) || (skill_num > 100)) return;
	if ((skill_level < 0) || (skill_level > 100)) return;

	if (success) {
		if (m_game->m_client_list[client_h]->m_skill_mastery[skill_num] != 0) return;

		m_game->m_client_list[client_h]->m_skill_mastery[skill_num] = skill_level;
		check_total_skill_mastery_points(client_h, skill_num);

		{

			hb::net::PacketNotifySkillTrainSuccess pkt{};
			pkt.header.msg_id = MsgId::Notify;
			pkt.header.msg_type = Notify::SkillTrainSuccess;
			pkt.skill_num = static_cast<uint8_t>(skill_num);
			pkt.skill_level = static_cast<uint8_t>(skill_level);
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

	}

}

int SkillManager::calc_skill_ssn_point(int level)
{
	int ret;

	if (level < 1) return 1;

	if (level <= 50)
		ret = level;
	else if (level > 50) {
		ret = (level * 2);
	}

	return ret;
}

void SkillManager::calculate_ssn_skill_index(int client_h, short skill_index, int value)
{
	int   old_ssn, ss_npoint, weapon_index;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;
	if ((skill_index < 0) || (skill_index >= hb::shared::limits::MaxSkillType)) return;
	if (m_game->m_client_list[client_h]->m_is_killed) return;

	if (m_game->m_client_list[client_h]->m_skill_mastery[skill_index] == 0) return;

	old_ssn = m_game->m_client_list[client_h]->m_skill_progress[skill_index];
	m_game->m_client_list[client_h]->m_skill_progress[skill_index] += value;

	ss_npoint = m_game->m_skill_progress_threshold[m_game->m_client_list[client_h]->m_skill_mastery[skill_index] + 1];

	// SkillSSN   Skill .
	if ((m_game->m_client_list[client_h]->m_skill_mastery[skill_index] < 100) &&
		(m_game->m_client_list[client_h]->m_skill_progress[skill_index] > ss_npoint)) {

		m_game->m_client_list[client_h]->m_skill_mastery[skill_index]++;
		// Skill .
		switch (skill_index) {
		case 0:
		case 5:
		case 13:
			if (m_game->m_client_list[client_h]->m_skill_mastery[skill_index] > ((m_game->m_client_list[client_h]->m_str + m_game->m_client_list[client_h]->m_angelic_str) * 2)) {
				m_game->m_client_list[client_h]->m_skill_mastery[skill_index]--;
				m_game->m_client_list[client_h]->m_skill_progress[skill_index] = old_ssn;
			}
			else m_game->m_client_list[client_h]->m_skill_progress[skill_index] = 0;
			break;

		case 3:
			// Level*2 .
			if (m_game->m_client_list[client_h]->m_skill_mastery[skill_index] > (m_game->m_client_list[client_h]->m_level * 2)) {
				m_game->m_client_list[client_h]->m_skill_mastery[skill_index]--;
				m_game->m_client_list[client_h]->m_skill_progress[skill_index] = old_ssn;
			}
			else m_game->m_client_list[client_h]->m_skill_progress[skill_index] = 0;
			break;

		case 4:
		case 18: // Crafting
		case 21:
			if (m_game->m_client_list[client_h]->m_skill_mastery[skill_index] > ((m_game->m_client_list[client_h]->m_mag + m_game->m_client_list[client_h]->m_angelic_mag) * 2)) {
				m_game->m_client_list[client_h]->m_skill_mastery[skill_index]--;
				m_game->m_client_list[client_h]->m_skill_progress[skill_index] = old_ssn;
			}
			else m_game->m_client_list[client_h]->m_skill_progress[skill_index] = 0;
			break;

		case 1:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
			if (m_game->m_client_list[client_h]->m_skill_mastery[skill_index] > ((m_game->m_client_list[client_h]->m_dex + m_game->m_client_list[client_h]->m_angelic_dex) * 2)) {
				m_game->m_client_list[client_h]->m_skill_mastery[skill_index]--;
				m_game->m_client_list[client_h]->m_skill_progress[skill_index] = old_ssn;
			}
			else m_game->m_client_list[client_h]->m_skill_progress[skill_index] = 0;
			break;

		case 2:
		case 12:
		case 14:
		case 15:
		case 19:
		case 20: // Enchanting
			if (m_game->m_client_list[client_h]->m_skill_mastery[skill_index] > ((m_game->m_client_list[client_h]->m_int + m_game->m_client_list[client_h]->m_angelic_int) * 2)) {
				m_game->m_client_list[client_h]->m_skill_mastery[skill_index]--;
				m_game->m_client_list[client_h]->m_skill_progress[skill_index] = old_ssn;
			}
			else m_game->m_client_list[client_h]->m_skill_progress[skill_index] = 0;
			break;

		case 23:
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
				weapon_index = m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)];
				if (m_game->m_client_list[client_h]->m_item_list[weapon_index]->m_related_skill == skill_index) {
					m_game->m_client_list[client_h]->m_hit_ratio++;
				}
			}

			if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)] != -1) {
				weapon_index = m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)];
				if (m_game->m_client_list[client_h]->m_item_list[weapon_index]->m_related_skill == skill_index) {
					// Mace    .  1 .
					m_game->m_client_list[client_h]->m_hit_ratio++;
				}
			}
		}

		if (m_game->m_client_list[client_h]->m_skill_progress[skill_index] == 0) {
			// SKill  700     1 .
			check_total_skill_mastery_points(client_h, skill_index);

			// Skill    .
			m_game->send_notify_msg(0, client_h, Notify::Skill, skill_index, m_game->m_client_list[client_h]->m_skill_mastery[skill_index], 0, 0);
		}
	}
}

bool SkillManager::check_total_skill_mastery_points(int client_h, int skill)
{
	
	int remain_point, total_points, weapon_index, down_skill_ssn, down_point;
	short down_skill_index;

	if (m_game->m_client_list[client_h] == 0) return false;

	total_points = 0;
	for(int i = 0; i < hb::shared::limits::MaxSkillType; i++)
		total_points += m_game->m_client_list[client_h]->m_skill_mastery[i];

	remain_point = total_points - MaxSkillPoints;

	if (remain_point > 0) {
		// .      SSN    .
		while (remain_point > 0) {

			down_skill_index = -1; // v1.4
			if (m_game->m_client_list[client_h]->m_down_skill_index != -1) {
				switch (m_game->m_client_list[client_h]->m_down_skill_index) {
				case 3:

				default:
					// 20    0  .
					if (m_game->m_client_list[client_h]->m_skill_mastery[m_game->m_client_list[client_h]->m_down_skill_index] > 0) {
						down_skill_index = m_game->m_client_list[client_h]->m_down_skill_index;
					}
					else {
						down_skill_ssn = 99999999;
						for(int i = 0; i < hb::shared::limits::MaxSkillType; i++)
							if ((m_game->m_client_list[client_h]->m_skill_mastery[i] >= 21) && (i != skill) &&
								(m_game->m_client_list[client_h]->m_skill_progress[i] <= down_skill_ssn)) {
								// V1.22     20    .
								down_skill_ssn = m_game->m_client_list[client_h]->m_skill_progress[i];
								down_skill_index = i;
							}
					}
					break;
				}
			}
			// 1      SSN   down_skill_index

			if (down_skill_index != -1) {

				if (m_game->m_client_list[client_h]->m_skill_mastery[down_skill_index] <= 20) // v1.4
					down_point = m_game->m_client_list[client_h]->m_skill_mastery[down_skill_index];
				else down_point = 1;

				m_game->m_client_list[client_h]->m_skill_mastery[down_skill_index] -= down_point; // v1.4
				m_game->m_client_list[client_h]->m_skill_progress[down_skill_index] = m_game->m_skill_progress_threshold[m_game->m_client_list[client_h]->m_skill_mastery[down_skill_index] + 1] - 1;
				remain_point -= down_point; // v1.4

				if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)] != -1) {
					weapon_index = m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::TwoHand)];
					if (m_game->m_client_list[client_h]->m_item_list[weapon_index]->m_related_skill == down_skill_index) {
						m_game->m_client_list[client_h]->m_hit_ratio -= down_point; // v1.4
						if (m_game->m_client_list[client_h]->m_hit_ratio < 0) m_game->m_client_list[client_h]->m_hit_ratio = 0;
					}
				}

				if (m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)] != -1) {
					weapon_index = m_game->m_client_list[client_h]->m_item_equipment_status[to_int(EquipPos::RightHand)];
					if (m_game->m_client_list[client_h]->m_item_list[weapon_index]->m_related_skill == down_skill_index) {
						m_game->m_client_list[client_h]->m_hit_ratio -= down_point; // v1.4
						if (m_game->m_client_list[client_h]->m_hit_ratio < 0) m_game->m_client_list[client_h]->m_hit_ratio = 0;
					}
				}
				m_game->send_notify_msg(0, client_h, Notify::Skill, down_skill_index, m_game->m_client_list[client_h]->m_skill_mastery[down_skill_index], 0, 0);
			}
			else {
				return false;
			}
		}
		return true;
	}

	return false;
}

void SkillManager::clear_skill_using_status(int client_h)
{
	
	short tX, fX, tY, fY;

	if (m_game->m_client_list[client_h] == 0) return;

	if (m_game->m_client_list[client_h]->m_skill_using_status[19]) {
		tX = m_game->m_client_list[client_h]->m_x;
		tY = m_game->m_client_list[client_h]->m_y;
		if (m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_moveable(tX, tY, 0) == false) {
			fX = m_game->m_client_list[client_h]->m_x + _tmp_cCorpseX[m_game->m_client_list[client_h]->m_dir];
			fY = m_game->m_client_list[client_h]->m_y + _tmp_cCorpseY[m_game->m_client_list[client_h]->m_dir];
			if (m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_moveable(fX, fY, 0) == false) {
				m_game->m_client_list[client_h]->m_dir = static_cast<direction>(m_game->dice(1, 8));
				fX = m_game->m_client_list[client_h]->m_x + _tmp_cCorpseX[m_game->m_client_list[client_h]->m_dir];
				fY = m_game->m_client_list[client_h]->m_y + _tmp_cCorpseY[m_game->m_client_list[client_h]->m_dir];
				if (m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_moveable(fX, fY, 0) == false) {
					return;
				}
			}
			m_game->send_notify_msg(0, client_h, Notify::DamageMove, m_game->m_client_list[client_h]->m_dir, 0, 0, 0);
		}
	}
	for(int i = 0; i < hb::shared::limits::MaxSkillType; i++) {
		m_game->m_client_list[client_h]->m_skill_using_status[i] = false;
		m_game->m_client_list[client_h]->m_skill_using_time_id[i] = 0;
	}

	if (m_game->m_client_list[client_h]->m_allocated_fish != 0) {
		m_game->m_fishing_manager->release_fish_engagement(client_h);
		m_game->send_notify_msg(0, client_h, Notify::FishCanceled, 0, 0, 0, 0);
	}

}

void SkillManager::use_skill_handler(int client_h, int v1, int v2, int v3)
{
	char  owner_type;
	short attacker_weapon, owner_h;
	int   result, player_skill_level;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;

	if ((v1 < 0) || (v1 >= hb::shared::limits::MaxSkillType)) return;
	if (m_game->m_skill_config_list[v1] == 0) return;
	if (m_game->m_client_list[client_h]->m_skill_using_status[v1]) return;

	/*
	if (v1 != 19) {
		m_game->m_client_list[client_h]->m_abuse_count++;
		if ((m_game->m_client_list[client_h]->m_abuse_count % 30) == 0) {
			std::snprintf(G_cTxt, sizeof(G_cTxt), "(!) ÇØÅ· ¿ëÀÇÀÚ(%s) Skill(%d) Tries(%d)",m_game->m_client_list[client_h]->m_char_name,
																	   v1, m_game->m_client_list[client_h]->m_abuse_count);
			PutLogFileList(G_cTxt);
		}
	}
	*/

	player_skill_level = m_game->m_client_list[client_h]->m_skill_mastery[v1];
	result = m_game->dice(1, 100);

	if (result > player_skill_level) {
		m_game->send_notify_msg(0, client_h, Notify::SkillUsingEnd, 0, 0, 0, 0);
		return;
	}

	switch (m_game->m_skill_config_list[v1]->m_type) {
	case EffectType::Pretend:
		switch (m_game->m_skill_config_list[v1]->m_value_1) {
		case 1:

			if (m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->m_is_fight_zone) {
				m_game->send_notify_msg(0, client_h, Notify::SkillUsingEnd, 0, 0, 0, 0);
				return;
			}

			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_dead_owner(&owner_h, &owner_type, m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y);
			if (owner_h != 0) {
				m_game->send_notify_msg(0, client_h, Notify::SkillUsingEnd, 0, 0, 0, 0);
				return;
			}

			result = 0;
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y - 1);
			result += owner_h;
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y + 1);
			result += owner_h;
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, m_game->m_client_list[client_h]->m_x - 1, m_game->m_client_list[client_h]->m_y);
			result += owner_h;
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, m_game->m_client_list[client_h]->m_x + 1, m_game->m_client_list[client_h]->m_y);
			result += owner_h;

			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, m_game->m_client_list[client_h]->m_x - 1, m_game->m_client_list[client_h]->m_y - 1);
			result += owner_h;
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, m_game->m_client_list[client_h]->m_x + 1, m_game->m_client_list[client_h]->m_y - 1);
			result += owner_h;
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, m_game->m_client_list[client_h]->m_x - 1, m_game->m_client_list[client_h]->m_y + 1);
			result += owner_h;
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->get_owner(&owner_h, &owner_type, m_game->m_client_list[client_h]->m_x + 1, m_game->m_client_list[client_h]->m_y + 1);
			result += owner_h;

			if (result != 0) {
				m_game->send_notify_msg(0, client_h, Notify::SkillUsingEnd, 0, 0, 0, 0);
				return;
			}

			calculate_ssn_skill_index(client_h, v1, 1);

			attacker_weapon = 1;
			m_game->send_event_to_near_client_type_a(client_h, hb::shared::owner_class::Player, MsgId::EventMotion, Type::Dying, 0, attacker_weapon, 0);
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->clear_owner(14, client_h, hb::shared::owner_class::Player, m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y);
			m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_dead_owner(client_h, hb::shared::owner_class::Player, m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y);
			break;
		}
		break;

	}

	m_game->m_client_list[client_h]->m_skill_using_status[v1] = true;
}

void SkillManager::set_down_skill_index_handler(int client_h, int skill_index)
{
	if (m_game->m_client_list[client_h] == 0) return;
	if ((skill_index < 0) || (skill_index >= hb::shared::limits::MaxSkillType)) return;

	if (m_game->m_client_list[client_h]->m_skill_mastery[skill_index] > 0)
		m_game->m_client_list[client_h]->m_down_skill_index = skill_index;

	m_game->send_notify_msg(0, client_h, Notify::DownSkillIndexSet, m_game->m_client_list[client_h]->m_down_skill_index, 0, 0, 0);
}

void SkillManager::taming_handler(int client_h, int skill_num, char map_index, int dX, int dY)
{
	int skill_level, range, taming_level, result;
	short owner_h;
	char  owner_type;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_map_list[map_index] == 0) return;

	skill_level = (int)m_game->m_client_list[client_h]->m_skill_mastery[skill_num];
	range = skill_level / 12;

	for(int iX = dX - range; iX <= dX + range; iX++)
		for(int iY = dY - range; iY <= dY + range; iY++) {
			owner_h = 0;
			if ((iX > 0) && (iY > 0) && (iX < m_game->m_map_list[map_index]->m_size_x) && (iY < m_game->m_map_list[map_index]->m_size_y))
				m_game->m_map_list[map_index]->get_owner(&owner_h, &owner_type, iX, iY);

			if (owner_h != 0) {
				switch (owner_type) {
				case hb::shared::owner_class::Player:
					if (m_game->m_client_list[owner_h] == 0) break;
					break;

				case hb::shared::owner_class::Npc:
					if (m_game->m_npc_list[owner_h] == 0) break;
					taming_level = 10;
					switch (m_game->m_npc_list[owner_h]->m_type) {
					case 10:
					case 16: taming_level = 1; break;
					case 22: taming_level = 2; break;
					case 17:
					case 14: taming_level = 3; break;
					case 18: taming_level = 4; break;
					case 11: taming_level = 5; break;
					case 23:
					case 12: taming_level = 6; break;
					case 28: taming_level = 7; break;
					case 13:
					case 27: taming_level = 8; break;
					case 29: taming_level = 9; break;
					case 33: taming_level = 9; break;
					case 30: taming_level = 9; break;
					case 31:
					case 32: taming_level = 10; break;
					}

					result = (skill_level / 10);

					if (result < taming_level) break;

					break;
				}
			}
		}
}

void SkillManager::skill_check(int target_h) {
	//magic
	while ((m_game->m_client_list[target_h]->m_mag * 2) < m_game->m_client_list[target_h]->m_skill_mastery[4]) {
		m_game->m_client_list[target_h]->m_skill_mastery[4]--;
	}
	//hand attack
	while ((m_game->m_client_list[target_h]->m_str * 2) < m_game->m_client_list[target_h]->m_skill_mastery[5]) {
		m_game->m_client_list[target_h]->m_skill_mastery[5]--;
	}
	//hammer
	while ((m_game->m_client_list[target_h]->m_dex * 2) < m_game->m_client_list[target_h]->m_skill_mastery[14]) {
		m_game->m_client_list[target_h]->m_skill_mastery[14]--;
	}
	//shield
	while ((m_game->m_client_list[target_h]->m_dex * 2) < m_game->m_client_list[target_h]->m_skill_mastery[11]) {
		m_game->m_client_list[target_h]->m_skill_mastery[11]--;
	}
	//axe
	while ((m_game->m_client_list[target_h]->m_dex * 2) < m_game->m_client_list[target_h]->m_skill_mastery[10]) {
		m_game->m_client_list[target_h]->m_skill_mastery[10]--;
	}
	//fencing
	while ((m_game->m_client_list[target_h]->m_dex * 2) < m_game->m_client_list[target_h]->m_skill_mastery[9]) {
		m_game->m_client_list[target_h]->m_skill_mastery[9]--;
	}
	while ((m_game->m_client_list[target_h]->m_dex * 2) < m_game->m_client_list[target_h]->m_skill_mastery[8]) {
		m_game->m_client_list[target_h]->m_skill_mastery[8]--;
	}
	while ((m_game->m_client_list[target_h]->m_dex * 2) < m_game->m_client_list[target_h]->m_skill_mastery[7]) {
		m_game->m_client_list[target_h]->m_skill_mastery[7]--;
	}
	//archery
	while ((m_game->m_client_list[target_h]->m_dex * 2) < m_game->m_client_list[target_h]->m_skill_mastery[6]) {
		m_game->m_client_list[target_h]->m_skill_mastery[6]--;
	}
	//staff
	while ((m_game->m_client_list[target_h]->m_mag * 2) < m_game->m_client_list[target_h]->m_skill_mastery[21]) {
		m_game->m_client_list[target_h]->m_skill_mastery[21]--;
	}
	//alc
	while ((m_game->m_client_list[target_h]->m_int * 2) < m_game->m_client_list[target_h]->m_skill_mastery[12]) {
		m_game->m_client_list[target_h]->m_skill_mastery[12]--;
	}
	//manu
	while ((m_game->m_client_list[target_h]->m_str * 2) < m_game->m_client_list[target_h]->m_skill_mastery[13]) {
		m_game->m_client_list[target_h]->m_skill_mastery[13]--;
	}
	while ((m_game->m_client_list[target_h]->m_vit * 2) < m_game->m_client_list[target_h]->m_skill_mastery[23]) {
		m_game->m_client_list[target_h]->m_skill_mastery[23]--;
	}
	while ((m_game->m_client_list[target_h]->m_int * 2) < m_game->m_client_list[target_h]->m_skill_mastery[19]) {
		m_game->m_client_list[target_h]->m_skill_mastery[19]--;
	}
	//farming
	while ((m_game->m_client_list[target_h]->m_int * 2) < m_game->m_client_list[target_h]->m_skill_mastery[2]) {
		m_game->m_client_list[target_h]->m_skill_mastery[2]--;
	}
	//fishing
	while ((m_game->m_client_list[target_h]->m_dex * 2) < m_game->m_client_list[target_h]->m_skill_mastery[1]) {
		m_game->m_client_list[target_h]->m_skill_mastery[1]--;
	}
	//mining
	while ((m_game->m_client_list[target_h]->m_str * 2) < m_game->m_client_list[target_h]->m_skill_mastery[0]) {
		m_game->m_client_list[target_h]->m_skill_mastery[0]--;
	}
}

void SkillManager::reload_skill_configs()
{
	sqlite3* configDb = nullptr;
	std::string configDbPath;
	bool configDbCreated = false;
	if (!EnsureGameConfigDatabase(&configDb, configDbPath, &configDbCreated) || configDbCreated)
	{
		hb::logger::log("Skill config reload failed: gamedata.db unavailable");
		return;
	}

	for(int i = 0; i < hb::shared::limits::MaxSkillType; i++)
	{
		if (m_game->m_skill_config_list[i] != 0)
		{
			delete m_game->m_skill_config_list[i];
			m_game->m_skill_config_list[i] = 0;
		}
	}

	if (!LoadSkillConfigs(configDb, m_game))
	{
		hb::logger::log("Skill config reload failed");
		CloseGameConfigDatabase(configDb);
		return;
	}

	CloseGameConfigDatabase(configDb);
	m_game->compute_config_hashes();
	hb::logger::log("Skill configs reloaded successfully");
}

void SkillManager::set_skill_all(int client_h, char* data, size_t msg_size)
//set_skill_all Acidx Command,  Added July 04, 2005 INDEPENDENCE BABY Fuck YEA
{
	if (m_game->m_client_list[client_h] == 0) return;
	//Magic
	if (m_game->m_client_list[client_h]->m_skill_mastery[4] < 100)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[4] = m_game->m_client_list[client_h]->m_mag * 2;
		if (m_game->m_client_list[client_h]->m_skill_mastery[4] > 100)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[4] = 100;
		}
		if (m_game->m_client_list[client_h]->m_mag > 50)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[4] = 100;
		}
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 4, m_game->m_client_list[client_h]->m_skill_mastery[4], 0, 0);

	}
	//LongSword
	if (m_game->m_client_list[client_h]->m_skill_mastery[8] < 100)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[8] = m_game->m_client_list[client_h]->m_dex * 2;

		if (m_game->m_client_list[client_h]->m_skill_mastery[8] > 100)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[8] = 100;
		}
		if (m_game->m_client_list[client_h]->m_dex > 50)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[8] = 100;
		}
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 8, m_game->m_client_list[client_h]->m_skill_mastery[8], 0, 0);

	}
	//Hammer
	if (m_game->m_client_list[client_h]->m_skill_mastery[14] < 100)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[14] = m_game->m_client_list[client_h]->m_dex * 2;
		if (m_game->m_client_list[client_h]->m_skill_mastery[14] > 100)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[14] = 100;
		}
		if (m_game->m_client_list[client_h]->m_dex > 50)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[14] = 100;
		}
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 14, m_game->m_client_list[client_h]->m_skill_mastery[14], 0, 0);

	}
	//Axes
	if (m_game->m_client_list[client_h]->m_skill_mastery[10] < 100)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[10] = m_game->m_client_list[client_h]->m_dex * 2;
		if (m_game->m_client_list[client_h]->m_skill_mastery[10] > 100)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[10] = 100;
		}
		if (m_game->m_client_list[client_h]->m_dex > 50)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[10] = 100;
		}
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 10, m_game->m_client_list[client_h]->m_skill_mastery[10], 0, 0);

	}
	//hand attack
	if (m_game->m_client_list[client_h]->m_skill_mastery[5] < 100)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[5] = m_game->m_client_list[client_h]->m_str * 2;
		if (m_game->m_client_list[client_h]->m_skill_mastery[5] > 100)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[5] = 100;
		}
		if (m_game->m_client_list[client_h]->m_str > 50)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[5] = 100;
		}
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 5, m_game->m_client_list[client_h]->m_skill_mastery[5], 0, 0);

	}
	//ShortSword
	if (m_game->m_client_list[client_h]->m_skill_mastery[7] < 100)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[7] = m_game->m_client_list[client_h]->m_dex * 2;
		if (m_game->m_client_list[client_h]->m_skill_mastery[7] > 100)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[7] = 100;
		}
		if (m_game->m_client_list[client_h]->m_dex > 50)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[7] = 100;
		}
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 7, m_game->m_client_list[client_h]->m_skill_mastery[7], 0, 0);

	}
	//archery
	if (m_game->m_client_list[client_h]->m_skill_mastery[6] < 100)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[6] = m_game->m_client_list[client_h]->m_dex * 2;
		if (m_game->m_client_list[client_h]->m_skill_mastery[6] > 100)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[6] = 100;
		}
		if (m_game->m_client_list[client_h]->m_dex > 50)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[6] = 100;
		}
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 6, m_game->m_client_list[client_h]->m_skill_mastery[6], 0, 0);

	}
	//Fencing
	if (m_game->m_client_list[client_h]->m_skill_mastery[9] < 100)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[9] = m_game->m_client_list[client_h]->m_dex * 2;
		if (m_game->m_client_list[client_h]->m_skill_mastery[9] > 100)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[9] = 100;
		}
		if (m_game->m_client_list[client_h]->m_dex > 50)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[9] = 100;
		}
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 9, m_game->m_client_list[client_h]->m_skill_mastery[9], 0, 0);

	}
	//Staff Attack
	if (m_game->m_client_list[client_h]->m_skill_mastery[21] < 100)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[21] = m_game->m_client_list[client_h]->m_int * 2;
		if (m_game->m_client_list[client_h]->m_skill_mastery[21] > 100)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[21] = 100;
		}
		if (m_game->m_client_list[client_h]->m_int > 50)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[21] = 100;
		}
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 21, m_game->m_client_list[client_h]->m_skill_mastery[21], 0, 0);

	}
	//shield
	if (m_game->m_client_list[client_h]->m_skill_mastery[11] < 100)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[11] = m_game->m_client_list[client_h]->m_dex * 2;
		if (m_game->m_client_list[client_h]->m_skill_mastery[11] > 100)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[11] = 100;
		}
		if (m_game->m_client_list[client_h]->m_dex > 50)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[11] = 100;
		}
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 11, m_game->m_client_list[client_h]->m_skill_mastery[11], 0, 0);

	}
	//mining
	if (m_game->m_client_list[client_h]->m_skill_mastery[0] < 100)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[0] = 100;
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 0, m_game->m_client_list[client_h]->m_skill_mastery[0], 0, 0);

	}
	//fishing
	if (m_game->m_client_list[client_h]->m_skill_mastery[1] < 100)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[1] = 100;
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 1, m_game->m_client_list[client_h]->m_skill_mastery[1], 0, 0);

	}
	//farming
	if (m_game->m_client_list[client_h]->m_skill_mastery[2] < 100)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[2] = 100;
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 2, m_game->m_client_list[client_h]->m_skill_mastery[2], 0, 0);

	}
	//alchemy
	if (m_game->m_client_list[client_h]->m_skill_mastery[12] < 100)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[12] = 100;
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 12, m_game->m_client_list[client_h]->m_skill_mastery[12], 0, 0);

	}
	//manufacturing
	if (m_game->m_client_list[client_h]->m_skill_mastery[13] < 100)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[13] = 100;
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 13, m_game->m_client_list[client_h]->m_skill_mastery[13], 0, 0);

	}
	//poison resistance
	if (m_game->m_client_list[client_h]->m_skill_mastery[23] < 20)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[23] = 20;
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 23, m_game->m_client_list[client_h]->m_skill_mastery[23], 0, 0);

	}
	//pretend corpse
	if (m_game->m_client_list[client_h]->m_skill_mastery[19] < 100)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[19] = m_game->m_client_list[client_h]->m_int * 2;
		if (m_game->m_client_list[client_h]->m_skill_mastery[19] > 100)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[19] = 100;
		}
		if (m_game->m_client_list[client_h]->m_int > 50)
		{
			m_game->m_client_list[client_h]->m_skill_mastery[19] = 100;
		}
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 19, m_game->m_client_list[client_h]->m_skill_mastery[19], 0, 0);

	}
	//magic resistance
	if (m_game->m_client_list[client_h]->m_skill_mastery[3] < 20)
	{
		// now we add skills
		m_game->m_client_list[client_h]->m_skill_mastery[3] = 20;
		//Send a notify to update the client
		m_game->send_notify_msg(0, client_h, Notify::Skill, 3, m_game->m_client_list[client_h]->m_skill_mastery[3], 0, 0);

	}
}
