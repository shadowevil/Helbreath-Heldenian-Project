// FishingManager.cpp: Implementation of FishingManager.
// Fish spawning, catch processing, and fishing interactions.
// Extracted from CGame (Phase B1).

#include "FishingManager.h"
#include "Game.h"
#include "SkillManager.h"
#include "ItemManager.h"
#include "DynamicObjectManager.h"
#include "Packet/SharedPackets.h"

using namespace hb::shared::net;
using namespace hb::server::config;
namespace dynamic_object = hb::shared::dynamic_object;

FishingManager::FishingManager()
{
	init_arrays();
}

FishingManager::~FishingManager()
{
	cleanup_arrays();
}

void FishingManager::init_arrays()
{
	for (int i = 0; i < MaxFishs; i++)
		m_fish[i] = 0;
}

void FishingManager::cleanup_arrays()
{
	for (int i = 0; i < MaxFishs; i++)
		if (m_fish[i] != 0) {
			delete m_fish[i];
			m_fish[i] = 0;
		}
}

int FishingManager::create_fish(char map_index, short sX, short sY, short type, CItem* item, int difficulty, uint32_t last_time)
{
	int dynamic_handle;

	if ((map_index < 0) || (map_index >= MaxMaps)) return 0;
	if (m_game->m_map_list[map_index] == 0) return 0;
	if (m_game->m_map_list[map_index]->get_is_water(sX, sY) == false) return 0;

	for(int i = 1; i < MaxFishs; i++)
		if (m_fish[i] == 0) {
			m_fish[i] = new class CFish(map_index, sX, sY, type, item, difficulty);
			if (m_fish[i] == 0) return 0;

			// Dynamic Object . Owner Fish  .
			switch (item->m_id_num) {
			case 101:
			case 102:
			case 103:
			case 570:
			case 571:
			case 572:
			case 573:
			case 574:
			case 575:
			case 576:
			case 577:
				dynamic_handle = m_game->m_dynamic_object_manager->add_dynamic_object_list(i, 0, dynamic_object::Fish, map_index, sX, sY, last_time);
				break;
			default:
				dynamic_handle = m_game->m_dynamic_object_manager->add_dynamic_object_list(i, 0, dynamic_object::FishObject, map_index, sX, sY, last_time);
				break;
			}

			if (dynamic_handle == 0) {
				delete m_fish[i];
				m_fish[i] = 0;
				return 0;
			}
			m_fish[i]->m_dynamic_object_handle = dynamic_handle;
			m_game->m_map_list[map_index]->m_cur_fish++;

			return i;
		}

	return 0;
}


bool FishingManager::delete_fish(int handle, int del_mode)
{
	int iH;
	uint32_t time;

	if (m_fish[handle] == 0) return false;

	time = GameClock::GetTimeMS();

	// DynamicObject .
	iH = m_fish[handle]->m_dynamic_object_handle;

	if (m_game->m_dynamic_object_manager->m_dynamic_object_list[iH] != 0) {
		m_game->send_event_to_near_client_type_b(MsgId::DynamicObject, MsgType::Reject, m_game->m_dynamic_object_manager->m_dynamic_object_list[iH]->m_map_index, m_game->m_dynamic_object_manager->m_dynamic_object_list[iH]->m_x, m_game->m_dynamic_object_manager->m_dynamic_object_list[iH]->m_y, m_game->m_dynamic_object_manager->m_dynamic_object_list[iH]->m_type, iH, 0, (short)0);
		m_game->m_map_list[m_game->m_dynamic_object_manager->m_dynamic_object_list[iH]->m_map_index]->set_dynamic_object(0, 0, m_game->m_dynamic_object_manager->m_dynamic_object_list[iH]->m_x, m_game->m_dynamic_object_manager->m_dynamic_object_list[iH]->m_y, time);
		m_game->m_map_list[m_game->m_dynamic_object_manager->m_dynamic_object_list[iH]->m_map_index]->m_cur_fish--;

		delete m_game->m_dynamic_object_manager->m_dynamic_object_list[iH];
		m_game->m_dynamic_object_manager->m_dynamic_object_list[iH] = 0;
	}

	for(int i = 1; i < MaxClients; i++) {
		if ((m_game->m_client_list[i] != 0) && (m_game->m_client_list[i]->m_is_init_complete) &&
			(m_game->m_client_list[i]->m_allocated_fish == handle)) {
			m_game->send_notify_msg(0, i, Notify::FishCanceled, del_mode, 0, 0, 0);
			m_game->m_skill_manager->clear_skill_using_status(i);
		}
	}

	delete m_fish[handle];
	m_fish[handle] = 0;

	return true;
}


int FishingManager::check_fish(int client_h, char map_index, short dX, short dY)
{

	short dist_x, dist_y;

	if (m_game->m_client_list[client_h] == 0) return 0;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return 0;

	if ((map_index < 0) || (map_index >= MaxMaps)) return 0;

	for(int i = 1; i < MaxDynamicObjects; i++)
		if (m_game->m_dynamic_object_manager->m_dynamic_object_list[i] != 0) {
			dist_x = abs(m_game->m_dynamic_object_manager->m_dynamic_object_list[i]->m_x - dX);
			dist_y = abs(m_game->m_dynamic_object_manager->m_dynamic_object_list[i]->m_y - dY);

			if ((m_game->m_dynamic_object_manager->m_dynamic_object_list[i]->m_map_index == map_index) &&
				((m_game->m_dynamic_object_manager->m_dynamic_object_list[i]->m_type == dynamic_object::Fish) || (m_game->m_dynamic_object_manager->m_dynamic_object_list[i]->m_type == dynamic_object::FishObject)) &&
				(dist_x <= 2) && (dist_y <= 2)) {
				// .       Fish  .

				if (m_fish[m_game->m_dynamic_object_manager->m_dynamic_object_list[i]->m_owner] == 0) return 0;
				if (m_fish[m_game->m_dynamic_object_manager->m_dynamic_object_list[i]->m_owner]->m_engaging_count >= MaxEngagingFish) return 0;

				if (m_game->m_client_list[client_h]->m_allocated_fish != 0) return 0;
				if (m_game->m_client_list[client_h]->m_map_index != map_index) return 0;
				m_game->m_client_list[client_h]->m_allocated_fish = m_game->m_dynamic_object_manager->m_dynamic_object_list[i]->m_owner;
				m_game->m_client_list[client_h]->m_fish_chance = 1;
				m_game->m_client_list[client_h]->m_skill_using_status[1] = true;

				m_game->send_notify_msg(0, client_h, Notify::EventFishMode, (m_fish[m_game->m_dynamic_object_manager->m_dynamic_object_list[i]->m_owner]->m_item->m_sell_price / 2), 0,
					0, m_fish[m_game->m_dynamic_object_manager->m_dynamic_object_list[i]->m_owner]->m_item->m_name);

				m_fish[m_game->m_dynamic_object_manager->m_dynamic_object_list[i]->m_owner]->m_engaging_count++;

				return i;
			}
		}

	return 0;
}

void FishingManager::fish_processor()
{
	int skill_level, result, change_value;

	for(int i = 1; i < MaxClients; i++) {
		if ((m_game->m_client_list[i] != 0) && (m_game->m_client_list[i]->m_is_init_complete) &&
			(m_game->m_client_list[i]->m_allocated_fish != 0)) {

			if (m_fish[m_game->m_client_list[i]->m_allocated_fish] == 0) break;

			skill_level = m_game->m_client_list[i]->m_skill_mastery[1];
			skill_level -= m_fish[m_game->m_client_list[i]->m_allocated_fish]->m_difficulty;
			if (skill_level <= 0) skill_level = 1;

			change_value = skill_level / 10;
			if (change_value <= 0) change_value = 1;
			change_value = m_game->dice(1, change_value);

			result = m_game->dice(1, 100);
			if (skill_level > result) {
				m_game->m_client_list[i]->m_fish_chance += change_value;
				if (m_game->m_client_list[i]->m_fish_chance > 99) m_game->m_client_list[i]->m_fish_chance = 99;

				m_game->send_notify_msg(0, i, Notify::FishChance, m_game->m_client_list[i]->m_fish_chance, 0, 0, 0);
			}
			else if (skill_level < result) {
				m_game->m_client_list[i]->m_fish_chance -= change_value;
				if (m_game->m_client_list[i]->m_fish_chance < 1) m_game->m_client_list[i]->m_fish_chance = 1;

				m_game->send_notify_msg(0, i, Notify::FishChance, m_game->m_client_list[i]->m_fish_chance, 0, 0, 0);
			}
		}
	}
}


void FishingManager::req_get_fish_this_time_handler(int client_h)
{
	int result, fish_h;
	CItem* item;

	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_is_init_complete == false) return;
	if (m_game->m_client_list[client_h]->m_allocated_fish == 0) return;
	if (m_fish[m_game->m_client_list[client_h]->m_allocated_fish] == 0) return;

	m_game->m_client_list[client_h]->m_skill_using_status[1] = false;

	result = m_game->dice(1, 100);
	if (m_game->m_client_list[client_h]->m_fish_chance >= result) {

		m_game->get_exp(client_h, m_game->dice(m_fish[m_game->m_client_list[client_h]->m_allocated_fish]->m_difficulty, 5));
		m_game->m_skill_manager->calculate_ssn_skill_index(client_h, 1, m_fish[m_game->m_client_list[client_h]->m_allocated_fish]->m_difficulty);

		item = m_fish[m_game->m_client_list[client_h]->m_allocated_fish]->m_item;
		m_fish[m_game->m_client_list[client_h]->m_allocated_fish]->m_item = 0;

		m_game->m_map_list[m_game->m_client_list[client_h]->m_map_index]->set_item(m_game->m_client_list[client_h]->m_x,
			m_game->m_client_list[client_h]->m_y,
			item);

		m_game->send_ground_item_event(CommonType::ItemDrop, m_game->m_client_list[client_h]->m_map_index,
			m_game->m_client_list[client_h]->m_x, m_game->m_client_list[client_h]->m_y, item);

		m_game->send_notify_msg(0, client_h, Notify::FishSuccess, 0, 0, 0, 0);
		fish_h = m_game->m_client_list[client_h]->m_allocated_fish;
		m_game->m_client_list[client_h]->m_allocated_fish = 0;

		delete_fish(fish_h, 1);
		return;
	}

	m_fish[m_game->m_client_list[client_h]->m_allocated_fish]->m_engaging_count--;
	m_game->send_notify_msg(0, client_h, Notify::FishFail, 0, 0, 0, 0);

	m_game->m_client_list[client_h]->m_allocated_fish = 0;
}


void FishingManager::fish_generator()
{
	int iP, tX, tY, ret;
	char  item_name[hb::shared::limits::ItemNameLen];
	int difficulty;
	uint32_t last_time;
	CItem* item;

	for(int i = 0; i < MaxMaps; i++) {
		if ((m_game->dice(1, 10) == 5) && (m_game->m_map_list[i] != 0) &&
			(m_game->m_map_list[i]->m_cur_fish < m_game->m_map_list[i]->m_max_fish)) {

			iP = m_game->dice(1, m_game->m_map_list[i]->m_total_fish_point) - 1;
			if ((m_game->m_map_list[i]->m_fish_point_list[iP].x == -1) || (m_game->m_map_list[i]->m_fish_point_list[iP].y == -1)) break;

			tX = m_game->m_map_list[i]->m_fish_point_list[iP].x + (m_game->dice(1, 3) - 2);
			tY = m_game->m_map_list[i]->m_fish_point_list[iP].y + (m_game->dice(1, 3) - 2);

			item = new CItem;
			if (item == 0) break;

			std::memset(item_name, 0, sizeof(item_name));
			switch (m_game->dice(1, 9)) {
			case 1:   strcpy(item_name, "RedCarp"); difficulty = m_game->dice(1, 10) + 20; break;
			case 2:   strcpy(item_name, "GreenCarp"); difficulty = m_game->dice(1, 5) + 10; break;
			case 3:   strcpy(item_name, "GoldCarp"); difficulty = m_game->dice(1, 10) + 1;  break;
			case 4:   strcpy(item_name, "CrucianCarp"); difficulty = 1;  break;
			case 5:   strcpy(item_name, "BlueSeaBream"); difficulty = m_game->dice(1, 15) + 1;  break;
			case 6:   strcpy(item_name, "RedSeaBream"); difficulty = m_game->dice(1, 18) + 1;  break;
			case 7:   strcpy(item_name, "Salmon"); difficulty = m_game->dice(1, 12) + 1;  break;
			case 8:   strcpy(item_name, "GrayMullet"); difficulty = m_game->dice(1, 10) + 1;  break;
			case 9:
				switch (m_game->dice(1, 150)) {
				case 1:
				case 2:
				case 3:
					strcpy(item_name, "PowerGreenPotion");
					difficulty = m_game->dice(5, 4) + 30;
					break;

				case 10:
				case 11:
					strcpy(item_name, "SuperPowerGreenPotion");
					difficulty = m_game->dice(5, 4) + 50;
					break;

				case 20:
					strcpy(item_name, "Dagger+2");
					difficulty = m_game->dice(5, 4) + 30;
					break;

				case 30:
					strcpy(item_name, "LongSword+2");
					difficulty = m_game->dice(5, 4) + 40;
					break;

				case 40:
					strcpy(item_name, "Scimitar+2");
					difficulty = m_game->dice(5, 4) + 50;
					break;

				case 50:
					strcpy(item_name, "Rapier+2");
					difficulty = m_game->dice(5, 4) + 60;
					break;

				case 60:
					strcpy(item_name, "Flameberge+2");
					difficulty = m_game->dice(5, 4) + 60;
					break;

				case 70:
					strcpy(item_name, "WarAxe+2");
					difficulty = m_game->dice(5, 4) + 50;
					break;

				case 90:
					strcpy(item_name, "Ruby");
					difficulty = m_game->dice(5, 4) + 40;
					break;

				case 95:
					strcpy(item_name, "Diamond");
					difficulty = m_game->dice(5, 4) + 40;
					break;
				}
				break;
			}
			last_time = (60000 * 10) + (m_game->dice(1, 3) - 1) * (60000 * 10);

			if (m_game->m_item_manager->init_item_attr(item, item_name)) {
				ret = create_fish(i, tX, tY, 1, item, difficulty, last_time);
			}
			else {
				delete item;
				item = 0;
			}
		}
	}
}


void FishingManager::release_fish_engagement(int client_h)
{
	if (m_game->m_client_list[client_h] == 0) return;
	if (m_game->m_client_list[client_h]->m_allocated_fish == 0) return;

	if (m_fish[m_game->m_client_list[client_h]->m_allocated_fish] != 0)
		m_fish[m_game->m_client_list[client_h]->m_allocated_fish]->m_engaging_count--;

	m_game->m_client_list[client_h]->m_allocated_fish = 0;
}
