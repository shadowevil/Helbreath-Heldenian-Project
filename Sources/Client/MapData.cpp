// MapData.cpp: implementation of the CMapData class.
//
//////////////////////////////////////////////////////////////////////
#include "MapData.h"
#include "FloatingTextManager.h"
#include "OwnerType.h"
#include "ObjectIDRange.h"
#include "DirectionHelpers.h"
#include "CommonTypes.h"
#include "Benchmark.h"
#include "EntityMotion.h"
#include "WeatherManager.h"
#include "AudioManager.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>



using namespace hb::shared::net;
namespace dynamic_object = hb::shared::dynamic_object;

using namespace hb::shared::action;
using namespace hb::client::config;
using namespace hb::shared::direction;

namespace
{
	constexpr uint32_t corpse_linger_time_ms = 1000;
	const uint32_t DEF_FULLDATA_REQUEST_INTERVAL = 2000;
	uint32_t g_dwLastFullDataRequestTime[hb::shared::object_id::NpcMax];
	bool ShouldRequestFullData(uint16_t object_id, int sX, int sY)
	{
		if (hb::shared::object_id::IsNearbyOffset(object_id)) return false;
		if (sX != -1 || sY != -1) return true;

		uint32_t now = GameClock::get_time_ms();
		if (now - g_dwLastFullDataRequestTime[object_id] < DEF_FULLDATA_REQUEST_INTERVAL) {
			return false;
		}
		g_dwLastFullDataRequestTime[object_id] = now;
		return true;
	}
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMapData::CMapData(class CGame* game)
{
	int i;
	m_game = game;
	m_map_size_x = 0;
	m_map_size_y = 0;
	m_rect_x = 0;
	m_rect_y = 0;
	m_pivot_x = 0;
	m_pivot_y = 0;
	m_frame_check_time = 0;
	m_frame_adjust_time = 0;
	std::fill(std::begin(m_object_id_cache_loc_x), std::end(m_object_id_cache_loc_x), 0);
	std::fill(std::begin(m_object_id_cache_loc_y), std::end(m_object_id_cache_loc_y), 0);
	m_dynamic_object_frame_time = m_frame_time = GameClock::get_time_ms();

	for (i = 0; i < TotalCharacters; i++)
	{
		m_stFrame[i][Type::Move].m_sMaxFrame = 7;
	}
	for (i = 1; i <= 6; i++)
	{
		// Original Helbreath 3.82 timing values
		m_stFrame[i][Type::stop].m_sMaxFrame = 14;
		m_stFrame[i][Type::stop].m_sFrameTime = 60;
		m_stFrame[i][Type::Move].m_sMaxFrame = 7;
		m_stFrame[i][Type::Move].m_sFrameTime = 70;
		m_stFrame[i][Type::DamageMove].m_sMaxFrame = 3;
		m_stFrame[i][Type::DamageMove].m_sFrameTime = 50;
		m_stFrame[i][Type::Run].m_sMaxFrame = 7;
		m_stFrame[i][Type::Run].m_sFrameTime = 39;
		m_stFrame[i][Type::Attack].m_sMaxFrame = 7;
		m_stFrame[i][Type::Attack].m_sFrameTime = 78;
		m_stFrame[i][Type::AttackMove].m_sMaxFrame = 12;
		m_stFrame[i][Type::AttackMove].m_sFrameTime = 78;
		m_stFrame[i][Type::Magic].m_sMaxFrame = 15;
		m_stFrame[i][Type::Magic].m_sFrameTime = 88;
		m_stFrame[i][Type::GetItem].m_sMaxFrame = 3;
		m_stFrame[i][Type::GetItem].m_sFrameTime = 150;
		m_stFrame[i][Type::Damage].m_sMaxFrame = 7;
		m_stFrame[i][Type::Damage].m_sFrameTime = 70;
		m_stFrame[i][Type::Dying].m_sMaxFrame = 12;
		m_stFrame[i][Type::Dying].m_sFrameTime = 80;
	}

	m_stFrame[hb::shared::owner::Slime][Type::stop].m_sFrameTime = 240;
	m_stFrame[hb::shared::owner::Slime][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Slime][Type::Move].m_sFrameTime = 63;
	m_stFrame[hb::shared::owner::Slime][Type::Attack].m_sFrameTime = 90;
	m_stFrame[hb::shared::owner::Slime][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Slime][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Slime][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Slime][Type::Dying].m_sFrameTime = 240;
	m_stFrame[hb::shared::owner::Slime][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Skeleton][Type::stop].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Skeleton][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Skeleton][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::Skeleton][Type::Attack].m_sFrameTime = 90;
	m_stFrame[hb::shared::owner::Skeleton][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Skeleton][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Skeleton][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Skeleton][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::Skeleton][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::StoneGolem][Type::stop].m_sFrameTime = 210;
	m_stFrame[hb::shared::owner::StoneGolem][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::StoneGolem][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::StoneGolem][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::StoneGolem][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::StoneGolem][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::StoneGolem][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::StoneGolem][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::StoneGolem][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Cyclops][Type::stop].m_sFrameTime = 210;
	m_stFrame[hb::shared::owner::Cyclops][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Cyclops][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Cyclops][Type::Attack].m_sFrameTime = 90;
	m_stFrame[hb::shared::owner::Cyclops][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Cyclops][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Cyclops][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Cyclops][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::Cyclops][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::OrcMage][Type::stop].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::OrcMage][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::OrcMage][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::OrcMage][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::OrcMage][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::OrcMage][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::OrcMage][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::OrcMage][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::OrcMage][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::ShopKeeper][Type::stop].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::ShopKeeper][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::ShopKeeper][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::ShopKeeper][Type::Attack].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::ShopKeeper][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::ShopKeeper][Type::Damage].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::ShopKeeper][Type::Damage].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::ShopKeeper][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::ShopKeeper][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GiantAnt][Type::stop].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::GiantAnt][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::GiantAnt][Type::Move].m_sFrameTime = 55;
	m_stFrame[hb::shared::owner::GiantAnt][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::GiantAnt][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::GiantAnt][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::GiantAnt][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GiantAnt][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::GiantAnt][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Scorpion][Type::stop].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Scorpion][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Scorpion][Type::Move].m_sFrameTime = 40;
	m_stFrame[hb::shared::owner::Scorpion][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Scorpion][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Scorpion][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Scorpion][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Scorpion][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::Scorpion][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Zombie][Type::stop].m_sFrameTime = 210;
	m_stFrame[hb::shared::owner::Zombie][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Zombie][Type::Move].m_sFrameTime = 90;
	m_stFrame[hb::shared::owner::Zombie][Type::Attack].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Zombie][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Zombie][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Zombie][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Zombie][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::Zombie][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Gandalf][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Gandalf][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Gandalf][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Gandalf][Type::Attack].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Gandalf][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Gandalf][Type::Damage].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::Gandalf][Type::Damage].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Gandalf][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::Gandalf][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Howard][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Howard][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Howard][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Howard][Type::Attack].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Howard][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Howard][Type::Damage].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::Howard][Type::Damage].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Howard][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::Howard][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Guard][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Guard][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Guard][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Guard][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Guard][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Guard][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Guard][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Guard][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::Guard][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Amphis][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Amphis][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Amphis][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Amphis][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Amphis][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Amphis][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Amphis][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Amphis][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::Amphis][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::ClayGolem][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::ClayGolem][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::ClayGolem][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::ClayGolem][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::ClayGolem][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::ClayGolem][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::ClayGolem][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::ClayGolem][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::ClayGolem][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Tom][Type::stop].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Tom][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::William][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::William][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Kennedy][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Kennedy][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Hellhound][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Hellhound][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Hellhound][Type::Move].m_sFrameTime = 50;
	m_stFrame[hb::shared::owner::Hellhound][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Hellhound][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Hellhound][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Hellhound][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Hellhound][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::Hellhound][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Troll][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Troll][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Troll][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Troll][Type::Attack].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Troll][Type::Attack].m_sMaxFrame = 5;
	m_stFrame[hb::shared::owner::Troll][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Troll][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Troll][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Troll][Type::Dying].m_sMaxFrame = 9;
	m_stFrame[hb::shared::owner::Ogre][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Ogre][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Ogre][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Ogre][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Ogre][Type::Attack].m_sMaxFrame = 5;
	m_stFrame[hb::shared::owner::Ogre][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Ogre][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Ogre][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Ogre][Type::Dying].m_sMaxFrame = 9;
	m_stFrame[hb::shared::owner::Liche][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Liche][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Liche][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Liche][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Liche][Type::Attack].m_sMaxFrame = 5;
	m_stFrame[hb::shared::owner::Liche][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Liche][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Liche][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Liche][Type::Dying].m_sMaxFrame = 9;
	m_stFrame[hb::shared::owner::Demon][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Demon][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Demon][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Demon][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Demon][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Demon][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Demon][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Demon][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Demon][Type::Dying].m_sMaxFrame = 9;
	m_stFrame[hb::shared::owner::Unicorn][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Unicorn][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Unicorn][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Unicorn][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Unicorn][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Unicorn][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Unicorn][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Unicorn][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Unicorn][Type::Dying].m_sMaxFrame = 11;
	m_stFrame[hb::shared::owner::WereWolf][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::WereWolf][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::WereWolf][Type::Move].m_sFrameTime = 80;
	m_stFrame[hb::shared::owner::WereWolf][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::WereWolf][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::WereWolf][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::WereWolf][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::WereWolf][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::WereWolf][Type::Dying].m_sMaxFrame = 11;
	m_stFrame[hb::shared::owner::Dummy][Type::stop].m_sFrameTime = 240;
	m_stFrame[hb::shared::owner::Dummy][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Dummy][Type::Move].m_sFrameTime = 80;
	m_stFrame[hb::shared::owner::Dummy][Type::Attack].m_sFrameTime = 90;
	m_stFrame[hb::shared::owner::Dummy][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Dummy][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Dummy][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Dummy][Type::Dying].m_sFrameTime = 240;
	m_stFrame[hb::shared::owner::Dummy][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::EnergySphere][Type::stop].m_sFrameTime = 80;
	m_stFrame[hb::shared::owner::EnergySphere][Type::stop].m_sMaxFrame = 9;
	m_stFrame[hb::shared::owner::EnergySphere][Type::Move].m_sFrameTime = 20;
	m_stFrame[hb::shared::owner::EnergySphere][Type::Move].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::EnergySphere][Type::Attack].m_sFrameTime = 80;
	m_stFrame[hb::shared::owner::EnergySphere][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::EnergySphere][Type::Damage].m_sFrameTime = 80;
	m_stFrame[hb::shared::owner::EnergySphere][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::EnergySphere][Type::Dying].m_sFrameTime = 80;
	m_stFrame[hb::shared::owner::EnergySphere][Type::Dying].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::ArrowGuardTower][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::ArrowGuardTower][Type::stop].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::ArrowGuardTower][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::ArrowGuardTower][Type::Move].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::ArrowGuardTower][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::ArrowGuardTower][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::ArrowGuardTower][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::ArrowGuardTower][Type::Damage].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::ArrowGuardTower][Type::Dying].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::ArrowGuardTower][Type::Dying].m_sMaxFrame = 6;
	m_stFrame[hb::shared::owner::CannonGuardTower][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::CannonGuardTower][Type::stop].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::CannonGuardTower][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::CannonGuardTower][Type::Move].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::CannonGuardTower][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::CannonGuardTower][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::CannonGuardTower][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::CannonGuardTower][Type::Damage].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::CannonGuardTower][Type::Dying].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::CannonGuardTower][Type::Dying].m_sMaxFrame = 6;
	m_stFrame[hb::shared::owner::ManaCollector][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::ManaCollector][Type::stop].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::ManaCollector][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::ManaCollector][Type::Move].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::ManaCollector][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::ManaCollector][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::ManaCollector][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::ManaCollector][Type::Damage].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::ManaCollector][Type::Dying].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::ManaCollector][Type::Dying].m_sMaxFrame = 6;
	m_stFrame[hb::shared::owner::Detector][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Detector][Type::stop].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::Detector][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Detector][Type::Move].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::Detector][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Detector][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Detector][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Detector][Type::Damage].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::Detector][Type::Dying].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::Detector][Type::Dying].m_sMaxFrame = 6;
	m_stFrame[hb::shared::owner::EnergyShield][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::EnergyShield][Type::stop].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::EnergyShield][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::EnergyShield][Type::Move].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::EnergyShield][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::EnergyShield][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::EnergyShield][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::EnergyShield][Type::Damage].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::EnergyShield][Type::Dying].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::EnergyShield][Type::Dying].m_sMaxFrame = 6;
	m_stFrame[hb::shared::owner::GrandMagicGenerator][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::GrandMagicGenerator][Type::stop].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::GrandMagicGenerator][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::GrandMagicGenerator][Type::Move].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::GrandMagicGenerator][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::GrandMagicGenerator][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::GrandMagicGenerator][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::GrandMagicGenerator][Type::Damage].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::GrandMagicGenerator][Type::Dying].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::GrandMagicGenerator][Type::Dying].m_sMaxFrame = 6;
	m_stFrame[hb::shared::owner::ManaStone][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::ManaStone][Type::stop].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::ManaStone][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::ManaStone][Type::Move].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::ManaStone][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::ManaStone][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::ManaStone][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::ManaStone][Type::Damage].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::ManaStone][Type::Dying].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::ManaStone][Type::Dying].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::LightWarBeetle][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::LightWarBeetle][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::LightWarBeetle][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::LightWarBeetle][Type::Attack].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::LightWarBeetle][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::LightWarBeetle][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::LightWarBeetle][Type::Damage].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::LightWarBeetle][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::LightWarBeetle][Type::Dying].m_sMaxFrame = 9;
	m_stFrame[hb::shared::owner::GodsHandKnight][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::GodsHandKnight][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GodsHandKnight][Type::Move].m_sFrameTime = 55;
	m_stFrame[hb::shared::owner::GodsHandKnight][Type::Attack].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::GodsHandKnight][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GodsHandKnight][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::GodsHandKnight][Type::Damage].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::GodsHandKnight][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::GodsHandKnight][Type::Dying].m_sMaxFrame = 9;
	m_stFrame[hb::shared::owner::GodsHandKnightCK][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::GodsHandKnightCK][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GodsHandKnightCK][Type::Move].m_sFrameTime = 55;
	m_stFrame[hb::shared::owner::GodsHandKnightCK][Type::Attack].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::GodsHandKnightCK][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GodsHandKnightCK][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::GodsHandKnightCK][Type::Damage].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::GodsHandKnightCK][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::GodsHandKnightCK][Type::Dying].m_sMaxFrame = 9;
	m_stFrame[hb::shared::owner::TempleKnight][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::TempleKnight][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::TempleKnight][Type::Move].m_sFrameTime = 55;
	m_stFrame[hb::shared::owner::TempleKnight][Type::Attack].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::TempleKnight][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::TempleKnight][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::TempleKnight][Type::Damage].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::TempleKnight][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::TempleKnight][Type::Dying].m_sMaxFrame = 9;
	m_stFrame[hb::shared::owner::BattleGolem][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::BattleGolem][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::BattleGolem][Type::Move].m_sFrameTime = 55;
	m_stFrame[hb::shared::owner::BattleGolem][Type::Attack].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::BattleGolem][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::BattleGolem][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::BattleGolem][Type::Damage].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::BattleGolem][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::BattleGolem][Type::Dying].m_sMaxFrame = 9;
	m_stFrame[hb::shared::owner::Stalker][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Stalker][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Stalker][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Stalker][Type::Attack].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Stalker][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Stalker][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Stalker][Type::Damage].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::Stalker][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Stalker][Type::Dying].m_sMaxFrame = 9;
	m_stFrame[hb::shared::owner::HellClaw][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::HellClaw][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::HellClaw][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::HellClaw][Type::Attack].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::HellClaw][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::HellClaw][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::HellClaw][Type::Damage].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::HellClaw][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::HellClaw][Type::Dying].m_sMaxFrame = 9;
	m_stFrame[hb::shared::owner::TigerWorm][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::TigerWorm][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::TigerWorm][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::TigerWorm][Type::Attack].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::TigerWorm][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::TigerWorm][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::TigerWorm][Type::Damage].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::TigerWorm][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::TigerWorm][Type::Dying].m_sMaxFrame = 9;
	m_stFrame[hb::shared::owner::Catapult][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Catapult][Type::stop].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::Catapult][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Catapult][Type::Attack].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Catapult][Type::Attack].m_sMaxFrame = 4;
	m_stFrame[hb::shared::owner::Catapult][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Catapult][Type::Damage].m_sMaxFrame = 0;
	m_stFrame[hb::shared::owner::Catapult][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Catapult][Type::Dying].m_sMaxFrame = 6;
	m_stFrame[hb::shared::owner::Gargoyle][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Gargoyle][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Gargoyle][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Gargoyle][Type::Attack].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::Gargoyle][Type::Attack].m_sMaxFrame = 9;
	m_stFrame[hb::shared::owner::Gargoyle][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Gargoyle][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Gargoyle][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Gargoyle][Type::Dying].m_sMaxFrame = 11 + 3;
	m_stFrame[hb::shared::owner::Beholder][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Beholder][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Beholder][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Beholder][Type::Attack].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Beholder][Type::Attack].m_sMaxFrame = 12;
	m_stFrame[hb::shared::owner::Beholder][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Beholder][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Beholder][Type::Dying].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::Beholder][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::DarkElf][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::DarkElf][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::DarkElf][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::DarkElf][Type::Attack].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::DarkElf][Type::Attack].m_sMaxFrame = 9;
	m_stFrame[hb::shared::owner::DarkElf][Type::Damage].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::DarkElf][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::DarkElf][Type::Dying].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::DarkElf][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::Bunny][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Bunny][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Bunny][Type::Move].m_sFrameTime = 30;
	m_stFrame[hb::shared::owner::Bunny][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Bunny][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Bunny][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Bunny][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Bunny][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Bunny][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::Cat][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Cat][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Cat][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Cat][Type::Attack].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Cat][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Cat][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Cat][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Cat][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Cat][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::GiantFrog][Type::stop].m_sFrameTime = 300;
	m_stFrame[hb::shared::owner::GiantFrog][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GiantFrog][Type::Move].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::GiantFrog][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::GiantFrog][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GiantFrog][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::GiantFrog][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GiantFrog][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::GiantFrog][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::MountainGiant][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::MountainGiant][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::MountainGiant][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::MountainGiant][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::MountainGiant][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::MountainGiant][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::MountainGiant][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::MountainGiant][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::MountainGiant][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::Ettin][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Ettin][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Ettin][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::Ettin][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Ettin][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Ettin][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Ettin][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Ettin][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Ettin][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::CannibalPlant][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::CannibalPlant][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::CannibalPlant][Type::Move].m_sFrameTime = 80;
	m_stFrame[hb::shared::owner::CannibalPlant][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::CannibalPlant][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::CannibalPlant][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::CannibalPlant][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::CannibalPlant][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::CannibalPlant][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::Rudolph][Type::stop].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::Rudolph][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Rudolph][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::Rudolph][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::Rudolph][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Rudolph][Type::Damage].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Rudolph][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Rudolph][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Rudolph][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::DireBoar][Type::stop].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::DireBoar][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::DireBoar][Type::Move].m_sFrameTime = 55;
	m_stFrame[hb::shared::owner::DireBoar][Type::Attack].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::DireBoar][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::DireBoar][Type::Damage].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::DireBoar][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::DireBoar][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::DireBoar][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::Frost][Type::stop].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::Frost][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Frost][Type::Move].m_sFrameTime = 55;
	m_stFrame[hb::shared::owner::Frost][Type::Attack].m_sFrameTime = 80;
	m_stFrame[hb::shared::owner::Frost][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Frost][Type::Damage].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Frost][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Frost][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Frost][Type::Dying].m_sMaxFrame = 5 + 3;
	m_stFrame[hb::shared::owner::Crops][Type::stop].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::Crops][Type::stop].m_sMaxFrame = 40;
	m_stFrame[hb::shared::owner::Crops][Type::Move].m_sFrameTime = 160;
	m_stFrame[hb::shared::owner::Crops][Type::Attack].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::Crops][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Crops][Type::Damage].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::Crops][Type::Damage].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Crops][Type::Dying].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::Crops][Type::Dying].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::IceGolem][Type::stop].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::IceGolem][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::IceGolem][Type::Move].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::IceGolem][Type::Attack].m_sFrameTime = 105;
	m_stFrame[hb::shared::owner::IceGolem][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::IceGolem][Type::Damage].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::IceGolem][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::IceGolem][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::IceGolem][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::Wyvern][Type::stop].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Wyvern][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Wyvern][Type::Move].m_sFrameTime = 90;
	m_stFrame[hb::shared::owner::Wyvern][Type::Attack].m_sFrameTime = 80;
	m_stFrame[hb::shared::owner::Wyvern][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Wyvern][Type::Damage].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Wyvern][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Wyvern][Type::Dying].m_sFrameTime = 65;
	m_stFrame[hb::shared::owner::Wyvern][Type::Dying].m_sMaxFrame = 15 + 3;
	m_stFrame[hb::shared::owner::McGaffin][Type::stop].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::McGaffin][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::McGaffin][Type::Move].m_sFrameTime = 80;
	m_stFrame[hb::shared::owner::Perry][Type::Move].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::McGaffin][Type::Attack].m_sFrameTime = 80;
	m_stFrame[hb::shared::owner::McGaffin][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::McGaffin][Type::Damage].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::McGaffin][Type::Damage].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::McGaffin][Type::Dying].m_sFrameTime = 65;
	m_stFrame[hb::shared::owner::McGaffin][Type::Dying].m_sMaxFrame = 3 + 3;
	m_stFrame[hb::shared::owner::Perry][Type::stop].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::Perry][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Perry][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::Perry][Type::Move].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Perry][Type::Attack].m_sFrameTime = 80;
	m_stFrame[hb::shared::owner::Perry][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Perry][Type::Damage].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Perry][Type::Damage].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Perry][Type::Dying].m_sFrameTime = 65;
	m_stFrame[hb::shared::owner::Perry][Type::Dying].m_sMaxFrame = 3 + 3;
	m_stFrame[hb::shared::owner::Devlin][Type::stop].m_sFrameTime = 200;
	m_stFrame[hb::shared::owner::Devlin][Type::stop].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Devlin][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::Perry][Type::Move].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Devlin][Type::Attack].m_sFrameTime = 80;
	m_stFrame[hb::shared::owner::Devlin][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Devlin][Type::Damage].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::Devlin][Type::Damage].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::Devlin][Type::Dying].m_sFrameTime = 65;
	m_stFrame[hb::shared::owner::Devlin][Type::Dying].m_sMaxFrame = 3 + 3;
	m_stFrame[hb::shared::owner::Dragon][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Dragon][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Dragon][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::Dragon][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Dragon][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Dragon][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Dragon][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Dragon][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Dragon][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::Centaur][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Centaur][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Centaur][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::Centaur][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Centaur][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Centaur][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Centaur][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Centaur][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Centaur][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::ClawTurtle][Type::stop].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::ClawTurtle][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::ClawTurtle][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::ClawTurtle][Type::Attack].m_sFrameTime = 80;
	m_stFrame[hb::shared::owner::ClawTurtle][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::ClawTurtle][Type::Damage].m_sFrameTime = 60;
	m_stFrame[hb::shared::owner::ClawTurtle][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::ClawTurtle][Type::Dying].m_sFrameTime = 65;
	m_stFrame[hb::shared::owner::ClawTurtle][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::FireWyvern][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::FireWyvern][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::FireWyvern][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::FireWyvern][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::FireWyvern][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::FireWyvern][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::FireWyvern][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::FireWyvern][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::FireWyvern][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::GiantCrayfish][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::GiantCrayfish][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GiantCrayfish][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::GiantCrayfish][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::GiantCrayfish][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GiantCrayfish][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::GiantCrayfish][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GiantCrayfish][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::GiantCrayfish][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::GiLizard][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::GiLizard][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GiLizard][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::GiLizard][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::GiLizard][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GiLizard][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::GiLizard][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GiLizard][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::GiLizard][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::GiTree][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::GiTree][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GiTree][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::GiTree][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::GiTree][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GiTree][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::GiTree][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::GiTree][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::GiTree][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::MasterOrc][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::MasterOrc][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::MasterOrc][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::MasterOrc][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::MasterOrc][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::MasterOrc][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::MasterOrc][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::MasterOrc][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::MasterOrc][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::Minaus][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Minaus][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Minaus][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::Minaus][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Minaus][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Minaus][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Minaus][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Minaus][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Minaus][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::Nizie][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Nizie][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Nizie][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::Nizie][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Nizie][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Nizie][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Nizie][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Nizie][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Nizie][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::Tentocle][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Tentocle][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Tentocle][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::Tentocle][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Tentocle][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Tentocle][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Tentocle][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Tentocle][Type::Dying].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::Tentocle][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::Abaddon][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Abaddon][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Abaddon][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::Abaddon][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Abaddon][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Abaddon][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Abaddon][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Abaddon][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::Abaddon][Type::Dying].m_sMaxFrame = 15 + 3;
	m_stFrame[hb::shared::owner::Sorceress][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Sorceress][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Sorceress][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::Sorceress][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Sorceress][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Sorceress][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Sorceress][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Sorceress][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::Sorceress][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::ATK][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::ATK][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::ATK][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::ATK][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::ATK][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::ATK][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::ATK][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::ATK][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::ATK][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::MasterElf][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::MasterElf][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::MasterElf][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::MasterElf][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::MasterElf][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::MasterElf][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::MasterElf][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::MasterElf][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::MasterElf][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::DSK][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::DSK][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::DSK][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::DSK][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::DSK][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::DSK][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::DSK][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::DSK][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::DSK][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::HBT][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::HBT][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::HBT][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::HBT][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::HBT][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::HBT][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::HBT][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::HBT][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::HBT][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::CT][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::CT][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::CT][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::CT][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::CT][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::CT][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::CT][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::CT][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::CT][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::Barbarian][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Barbarian][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Barbarian][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::Barbarian][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Barbarian][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Barbarian][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Barbarian][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Barbarian][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::Barbarian][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::AGC][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::AGC][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::AGC][Type::Move].m_sFrameTime = 70;
	m_stFrame[hb::shared::owner::AGC][Type::Attack].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::AGC][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::AGC][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::AGC][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::AGC][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::AGC][Type::Dying].m_sMaxFrame = 10;
	m_stFrame[hb::shared::owner::Gail][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Gail][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Gate][Type::stop].m_sFrameTime = 250;
	m_stFrame[hb::shared::owner::Gate][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Gate][Type::Damage].m_sFrameTime = 100;
	m_stFrame[hb::shared::owner::Gate][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::Gate][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::Gate][Type::Dying].m_sMaxFrame = 10;

	m_stFrame[99][Type::stop].m_sFrameTime = 250;
	m_stFrame[99][Type::stop].m_sMaxFrame = 3;
	m_stFrame[99][Type::Move].m_sFrameTime = 80;
	m_stFrame[99][Type::Attack].m_sFrameTime = 120;
	m_stFrame[99][Type::Attack].m_sMaxFrame = 7;
	m_stFrame[99][Type::Damage].m_sFrameTime = 120;
	m_stFrame[99][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[99][Type::Dying].m_sFrameTime = 100;
	m_stFrame[99][Type::Dying].m_sMaxFrame = 9;

	m_stFrame[hb::shared::owner::AirElemental][Type::stop].m_sFrameTime = 40;
	m_stFrame[hb::shared::owner::AirElemental][Type::stop].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::AirElemental][Type::Move].m_sFrameTime = 80;
	m_stFrame[hb::shared::owner::AirElemental][Type::Attack].m_sFrameTime = 120;
	m_stFrame[hb::shared::owner::AirElemental][Type::Attack].m_sMaxFrame = 3;
	m_stFrame[hb::shared::owner::AirElemental][Type::Damage].m_sFrameTime = 150;
	m_stFrame[hb::shared::owner::AirElemental][Type::Damage].m_sMaxFrame = 7;
	m_stFrame[hb::shared::owner::AirElemental][Type::Dying].m_sFrameTime = 180;
	m_stFrame[hb::shared::owner::AirElemental][Type::Dying].m_sMaxFrame = 7;

}

void CMapData::init()
{
	int x, y;
	m_frame_check_time = GameClock::get_time_ms();
	m_frame_adjust_time = 0;
	m_pivot_x = -1;
	m_pivot_y = -1;

	for (x = 0; x < MapDataSizeX; x++)
		for (y = 0; y < MapDataSizeY; y++)
			m_data[x][y].clear();

	for (x = 0; x < hb::shared::object_id::NpcMax; x++) {
		m_object_id_cache_loc_x[x] = 0;
		m_object_id_cache_loc_y[x] = 0;
	}
}

CMapData::~CMapData()
{
}

void CMapData::open_map_data_file(char* fn)
{
	char header[260];
	char* cp, * cpMapData;
	std::memset(header, 0, sizeof(header));
	std::ifstream file(fn, std::ios::binary);
	if (!file) return;
	file.read(header, 256);
	decode_map_info(header);
	cpMapData = new char[m_map_size_x * m_map_size_y * 10];
	file.read(cpMapData, m_map_size_x * m_map_size_y * 10);
	cp = cpMapData;
	for (int y = 0; y < m_map_size_y; y++)
	{
		for (int x = 0; x < m_map_size_x; x++)
		{
			std::memcpy(&m_tile[x][y].m_sTileSprite, cp, sizeof(short));
			cp += 2;
			std::memcpy(&m_tile[x][y].m_sTileSpriteFrame, cp, sizeof(short));
			cp += 2;
			std::memcpy(&m_tile[x][y].m_sObjectSprite, cp, sizeof(short));
			cp += 2;
			std::memcpy(&m_tile[x][y].m_sObjectSpriteFrame, cp, sizeof(short));
			cp += 2;
			m_tile[x][y].m_bIsMoveAllowed = ((*cp) & 0x80) == 0;
			m_tile[x][y].m_bIsTeleport = ((*cp) & 0x40) != 0;
			cp += 2;
		}
	}
	delete[] cpMapData;
}

void CMapData::decode_map_info(char* header)
{
	for (int i = 0; i < 256; i++)
		if (header[i] == 0) header[i] = ' ';

	constexpr std::string_view seps = "= ,\t\n";
	std::string_view input(header, 256);
	char read_mode = 0;

	size_t pos = input.find_first_not_of(seps);
	while (pos != std::string_view::npos)
	{
		size_t end = input.find_first_of(seps, pos);
		std::string_view token = input.substr(pos, end - pos);

		if (read_mode != 0)
		{
			// token is null-terminated within header, safe for atoi
			switch (read_mode)
			{
			case 1:
				std::from_chars(token.data(), token.data() + token.size(), m_map_size_x);
				read_mode = 0;
				break;
			case 2:
				std::from_chars(token.data(), token.data() + token.size(), m_map_size_y);
				read_mode = 0;
				break;
			}
		}
		else
		{
			if (token == "MAPSIZEX") read_mode = 1;
			if (token == "MAPSIZEY") read_mode = 2;
		}
		pos = (end == std::string_view::npos) ? end : input.find_first_not_of(seps, end);
	}
}

void CMapData::shift_map_data(direction dir)
{
	int ix, iy;
	for (iy = 0; iy < MapDataSizeY; iy++)
		for (ix = 0; ix < MapDataSizeX; ix++)
			m_tmp_data[ix][iy].clear();

	switch (dir) {
	case direction::north:
		for (ix = 0; ix < hb::shared::view::InitDataTilesX + 1; ix++)
			for (iy = 0; iy < hb::shared::view::InitDataTilesY; iy++)
				m_tmp_data[hb::shared::view::MapDataBufferX + ix][hb::shared::view::MapDataBufferY + 1 + iy] = m_data[hb::shared::view::MapDataBufferX + ix][hb::shared::view::MapDataBufferY + iy];
		m_pivot_y--;
		break;
	case direction::northeast:
		for (ix = 0; ix < hb::shared::view::InitDataTilesX; ix++)
			for (iy = 0; iy < hb::shared::view::InitDataTilesY; iy++)
				m_tmp_data[hb::shared::view::MapDataBufferX + ix][hb::shared::view::MapDataBufferY + 1 + iy] = m_data[hb::shared::view::MapDataBufferY + ix][hb::shared::view::MapDataBufferY + iy];
		m_pivot_x++;
		m_pivot_y--;
		break;
	case direction::east:
		for (ix = 0; ix < hb::shared::view::InitDataTilesX; ix++)
			for (iy = 0; iy < hb::shared::view::InitDataTilesY + 1; iy++)
				m_tmp_data[hb::shared::view::MapDataBufferX + ix][hb::shared::view::MapDataBufferY + iy] = m_data[hb::shared::view::MapDataBufferY + ix][hb::shared::view::MapDataBufferY + iy];
		m_pivot_x++;
		break;
	case direction::southeast:
		for (ix = 0; ix < hb::shared::view::InitDataTilesX; ix++)
			for (iy = 0; iy < hb::shared::view::InitDataTilesY; iy++)
				m_tmp_data[hb::shared::view::MapDataBufferX + ix][hb::shared::view::MapDataBufferY + iy] = m_data[hb::shared::view::MapDataBufferY + ix][hb::shared::view::MapDataBufferY + 1 + iy];
		m_pivot_x++;
		m_pivot_y++;
		break;
	case direction::south:
		for (ix = 0; ix < hb::shared::view::InitDataTilesX + 1; ix++)
			for (iy = 0; iy < hb::shared::view::InitDataTilesY; iy++)
				m_tmp_data[hb::shared::view::MapDataBufferX + ix][hb::shared::view::MapDataBufferY + iy] = m_data[hb::shared::view::MapDataBufferX + ix][hb::shared::view::MapDataBufferY + 1 + iy];
		m_pivot_y++;
		break;
	case direction::southwest:
		for (ix = 0; ix < hb::shared::view::InitDataTilesX; ix++)
			for (iy = 0; iy < hb::shared::view::InitDataTilesY; iy++)
				m_tmp_data[hb::shared::view::MapDataBufferY + ix][hb::shared::view::MapDataBufferY + iy] = m_data[hb::shared::view::MapDataBufferX + ix][hb::shared::view::MapDataBufferY + 1 + iy];
		m_pivot_x--;
		m_pivot_y++;
		break;
	case direction::west:
		for (ix = 0; ix < hb::shared::view::InitDataTilesX; ix++)
			for (iy = 0; iy < hb::shared::view::InitDataTilesY + 1; iy++)
				m_tmp_data[hb::shared::view::MapDataBufferY + ix][hb::shared::view::MapDataBufferY + iy] = m_data[hb::shared::view::MapDataBufferX + ix][hb::shared::view::MapDataBufferY + iy];
		m_pivot_x--;
		break;
	case direction::northwest:
		for (ix = 0; ix < hb::shared::view::InitDataTilesX; ix++)
			for (iy = 0; iy < hb::shared::view::InitDataTilesY; iy++)
				m_tmp_data[hb::shared::view::MapDataBufferY + ix][hb::shared::view::MapDataBufferY + 1 + iy] = m_data[hb::shared::view::MapDataBufferX + ix][hb::shared::view::MapDataBufferY + iy];
		m_pivot_x--;
		m_pivot_y--;
		break;
	default:
		break;
	}
	for (ix = 0; ix < MapDataSizeX; ix++)
		for (iy = 0; iy < MapDataSizeY; iy++)
			m_data[ix][iy] = m_tmp_data[ix][iy];
}

bool CMapData::get_is_locatable(short sX, short sY)
{
	int dX, dY;
	if ((sX < m_pivot_x) || (sX > m_pivot_x + MapDataSizeX) ||
		(sY < m_pivot_y) || (sY > m_pivot_y + MapDataSizeY)) return false;
	dX = sX - m_pivot_x;
	dY = sY - m_pivot_y;
	//Helltrayn 28/05/09. A�adimos esto para corregir el bug MIM que cierra el cliente
	if (dX <= 0 || dY <= 0) return false;
	// Allow pathing through dying entities — server already considers them dead
	if (m_data[dX][dY].m_owner_type != 0 && m_data[dX][dY].m_animation.m_action != Type::Dying) return false;
	if (m_tile[sX][sY].m_bIsMoveAllowed == false) return false;
	if (m_data[dX][dY].m_dynamic_object_type == dynamic_object::Mineral1) return false; // 4
	if (m_data[dX][dY].m_dynamic_object_type == dynamic_object::Mineral2) return false; // 5

	if (m_data[dX + 1][dY + 1].m_owner_type == hb::shared::owner::Wyvern) return false;
	if (m_data[dX + 1][dY].m_owner_type == hb::shared::owner::Wyvern) return false;
	if ((dY > 0) && (m_data[dX + 1][dY - 1].m_owner_type == hb::shared::owner::Wyvern)) return false;
	if (m_data[dX][dY + 1].m_owner_type == hb::shared::owner::Wyvern) return false;
	if (m_data[dX][dY].m_owner_type == hb::shared::owner::Wyvern) return false;
	if ((dY > 0) && (m_data[dX][dY - 1].m_owner_type == hb::shared::owner::Wyvern)) return false;
	if ((dX > 0) && (m_data[dX - 1][dY + 1].m_owner_type == hb::shared::owner::Wyvern)) return false;
	if ((dX > 0) && (m_data[dX - 1][dY].m_owner_type == hb::shared::owner::Wyvern)) return false;
	if ((dX > 0) && (dY > 0) && (m_data[dX - 1][dY - 1].m_owner_type == hb::shared::owner::Wyvern)) return false;
	if (m_data[dX + 1][dY + 1].m_owner_type == hb::shared::owner::FireWyvern) return false;
	if (m_data[dX + 1][dY].m_owner_type == hb::shared::owner::FireWyvern) return false;
	if ((dY > 0) && (m_data[dX + 1][dY - 1].m_owner_type == hb::shared::owner::FireWyvern)) return false;
	if (m_data[dX][dY + 1].m_owner_type == hb::shared::owner::FireWyvern) return false;
	if (m_data[dX][dY].m_owner_type == hb::shared::owner::FireWyvern) return false;
	if ((dY > 0) && (m_data[dX][dY - 1].m_owner_type == hb::shared::owner::FireWyvern)) return false;
	if ((dX > 0) && (m_data[dX - 1][dY + 1].m_owner_type == hb::shared::owner::FireWyvern)) return false;
	if ((dX > 0) && (m_data[dX - 1][dY].m_owner_type == hb::shared::owner::FireWyvern)) return false;
	if ((dX > 0) && (dY > 0) && (m_data[dX - 1][dY - 1].m_owner_type == hb::shared::owner::FireWyvern)) return false;
	if (m_data[dX + 1][dY + 1].m_owner_type == hb::shared::owner::Abaddon) return false;
	if (m_data[dX + 1][dY].m_owner_type == hb::shared::owner::Abaddon) return false;
	if ((dY > 0) && (m_data[dX + 1][dY - 1].m_owner_type == hb::shared::owner::Abaddon)) return false;
	if (m_data[dX][dY + 1].m_owner_type == hb::shared::owner::Abaddon) return false;
	if (m_data[dX][dY].m_owner_type == hb::shared::owner::Abaddon) return false;
	if ((dY > 0) && (m_data[dX][dY - 1].m_owner_type == hb::shared::owner::Abaddon)) return false;
	if ((dX > 0) && (m_data[dX - 1][dY + 1].m_owner_type == hb::shared::owner::Abaddon)) return false;
	if ((dX > 0) && (m_data[dX - 1][dY].m_owner_type == hb::shared::owner::Abaddon)) return false;
	if ((dX > 0) && (dY > 0) && (m_data[dX - 1][dY - 1].m_owner_type == hb::shared::owner::Abaddon)) return false;
	if (m_data[dX + 1][dY + 1].m_owner_type == hb::shared::owner::Gate) return false;
	if (m_data[dX + 1][dY].m_owner_type == hb::shared::owner::Gate) return false;
	if ((dY > 0) && (m_data[dX + 1][dY - 1].m_owner_type == hb::shared::owner::Gate)) return false;
	if (m_data[dX][dY + 1].m_owner_type == hb::shared::owner::Gate) return false;
	if (m_data[dX][dY].m_owner_type == hb::shared::owner::Gate) return false;
	if ((dY > 0) && (m_data[dX][dY - 1].m_owner_type == hb::shared::owner::Gate)) return false;
	if ((dX > 0) && (m_data[dX - 1][dY + 1].m_owner_type == hb::shared::owner::Gate)) return false;
	if ((dX > 0) && (m_data[dX - 1][dY].m_owner_type == hb::shared::owner::Gate)) return false;
	if ((dX > 0) && (dY > 0) && (m_data[dX - 1][dY - 1].m_owner_type == hb::shared::owner::Gate)) return false;
	return true;
}

bool CMapData::is_teleport_loc(short sX, short sY)
{
	if ((sX < m_pivot_x) || (sX > m_pivot_x + MapDataSizeX) ||
		(sY < m_pivot_y) || (sY > m_pivot_y + MapDataSizeY)) return false;

	if (m_tile[sX][sY].m_bIsTeleport == false) return false;

	return true;
}

bool CMapData::set_owner(uint16_t object_id, int sX, int sY, int type, direction dir, const hb::shared::entity::PlayerAppearance& appearance, const hb::shared::entity::PlayerStatus& status, std::string& name, short action, short v1, short v2, short v3, int pre_loc, int frame, short npcConfigId)
{
	int   iX, iY, dX, dY;
	int   chat_index, add;
	std::string tmp_name;
	uint32_t time;
	int   effect_type, effect_frame, effect_total_frame;
	bool  use_abs_pos = false;
	uint16_t original_object_id = object_id;
	hb::shared::entity::PlayerStatus localStatus = status;
	hb::shared::entity::PlayerAppearance localAppearance = appearance;
	short localNpcConfigId = npcConfigId;
	// Track old motion offset for seamless tile transitions during continuous movement
	float old_motion_offset_x = 0.0f;
	float old_motion_offset_y = 0.0f;
	int8_t old_motion_dir = 0;
	bool had_old_motion = false;

	if ((m_pivot_x == -1) || (m_pivot_y == -1)) return false;
	tmp_name.clear();
	tmp_name = name;
	time = m_frame_time;
	effect_type = effect_frame = effect_total_frame = 0;
	if ((hb::shared::object_id::IsNearbyOffset(object_id)) &&
		((action == Type::Move) || (action == Type::Run) ||
			(action == Type::DamageMove) || (action == Type::Damage) ||
			(action == Type::Dying))) {
		if ((sX >= m_pivot_x) && (sX < m_pivot_x + MapDataSizeX) &&
			(sY >= m_pivot_y) && (sY < m_pivot_y + MapDataSizeY)) {
			use_abs_pos = true;
		}
	}
	if ((!hb::shared::object_id::IsNearbyOffset(object_id))
		&& ((sX < m_pivot_x) || (sX >= m_pivot_x + MapDataSizeX)
			|| (sY < m_pivot_y) || (sY >= m_pivot_y + MapDataSizeY)))
	{
		if (m_object_id_cache_loc_x[object_id] > 0)
		{
			iX = m_object_id_cache_loc_x[object_id] - m_pivot_x;
			iY = m_object_id_cache_loc_y[object_id] - m_pivot_y;
			if ((iX < 0) || (iX >= MapDataSizeX) || (iY < 0) || (iY >= MapDataSizeY))
			{
				m_object_id_cache_loc_x[object_id] = 0;
				m_object_id_cache_loc_y[object_id] = 0;
				return false;
			}

			if (m_data[iX][iY].m_object_id == object_id)
			{
				m_data[iX][iY].m_owner_type = 0;
				m_data[iX][iY].m_npc_config_id = -1;
				m_data[iX][iY].m_owner_name.clear();
				name.clear();

				m_game->get_floating_text().clear(m_data[iX][iY].m_chat_msg);
				m_data[iX][iY].m_chat_msg = 0;
				m_data[iX][iY].m_effect_type = 0;
				m_object_id_cache_loc_x[object_id] = 0;
				m_object_id_cache_loc_y[object_id] = 0;
				return false;
			}
		}
		else if (m_object_id_cache_loc_x[object_id] < 0)
		{
			iX = abs(m_object_id_cache_loc_x[object_id]) - m_pivot_x;
			iY = abs(m_object_id_cache_loc_y[object_id]) - m_pivot_y;
			if ((iX < 0) || (iX >= MapDataSizeX) || (iY < 0) || (iY >= MapDataSizeY))
			{
				m_object_id_cache_loc_x[object_id] = 0;
				m_object_id_cache_loc_y[object_id] = 0;
				return false;
			}
			if ((m_data[iX][iY].m_dead_owner_frame == -1) && (m_data[iX][iY].m_dead_object_id == object_id))
			{
				m_data[iX][iY].m_dead_owner_frame = 0;
				m_data[iX][iY].m_dead_owner_time = m_frame_time; // Reset timer for smooth fade
				name.clear();
				m_game->get_floating_text().clear(m_data[iX][iY].m_dead_chat_msg);
				m_data[iX][iY].m_dead_chat_msg = 0;
				m_object_id_cache_loc_x[object_id] = 0;
				m_object_id_cache_loc_y[object_id] = 0;
				return false;
			}
		}

		for (iX = 0; iX < MapDataSizeX; iX++)
			for (iY = 0; iY < MapDataSizeY; iY++)
			{
				if (m_data[iX][iY].m_object_id == object_id)
				{
					m_data[iX][iY].m_owner_type = 0;
					m_data[iX][iY].m_npc_config_id = -1;
					m_data[iX][iY].m_owner_name.clear();
					name.clear();
					m_game->get_floating_text().clear(m_data[iX][iY].m_chat_msg);
					m_data[iX][iY].m_chat_msg = 0;
					m_object_id_cache_loc_x[object_id] = 0;
					m_object_id_cache_loc_y[object_id] = 0;
					m_data[iX][iY].m_effect_type = 0;
					return false;
				}

				if ((m_data[iX][iY].m_dead_owner_frame == -1) && (m_data[iX][iY].m_dead_object_id == object_id))
				{
					m_data[iX][iY].m_dead_owner_frame = 0;
					m_data[iX][iY].m_dead_owner_time = m_frame_time; // Reset timer for smooth fade
					name.clear();
					m_game->get_floating_text().clear(m_data[iX][iY].m_dead_chat_msg);
					m_data[iX][iY].m_dead_chat_msg = 0;
					m_object_id_cache_loc_x[object_id] = 0;
					m_object_id_cache_loc_y[object_id] = 0;
					return false;
				}
			}
		name.clear();
		return false;
	}
	chat_index = 0;

	bool found_old = false;

	if ((!hb::shared::object_id::IsNearbyOffset(object_id)) && (action != Type::NullAction))
	{
		tmp_name.clear();
		tmp_name = name;
		dX = sX - m_pivot_x;
		dY = sY - m_pivot_y;
		if (m_object_id_cache_loc_x[object_id] > 0)
		{
			iX = m_object_id_cache_loc_x[object_id] - m_pivot_x;
			iY = m_object_id_cache_loc_y[object_id] - m_pivot_y;
			if ((iX < 0) || (iX >= MapDataSizeX) || (iY < 0) || (iY >= MapDataSizeY))
			{
				m_object_id_cache_loc_x[object_id] = 0;
				m_object_id_cache_loc_y[object_id] = 0;
				return false;
			}
			if (m_data[iX][iY].m_object_id == object_id)
			{
				chat_index = m_data[iX][iY].m_chat_msg;
				effect_type = m_data[iX][iY].m_effect_type;
				effect_frame = m_data[iX][iY].m_effect_frame;
				effect_total_frame = m_data[iX][iY].m_effect_total_frame;

				// Capture old motion offset and direction for seamless continuous movement
				if (m_data[iX][iY].m_motion.m_is_moving) {
					old_motion_offset_x = m_data[iX][iY].m_motion.m_current_offset_x;
					old_motion_offset_y = m_data[iX][iY].m_motion.m_current_offset_y;
					old_motion_dir = m_data[iX][iY].m_motion.m_direction;
					had_old_motion = true;
				}

				m_data[iX][iY].m_object_id = 0; //-1; v1.41
				m_data[iX][iY].m_chat_msg = 0; // v1.4
				m_data[iX][iY].m_owner_type = 0;
				m_data[iX][iY].m_npc_config_id = -1;
				m_data[iX][iY].m_owner_name.clear();
				m_object_id_cache_loc_x[object_id] = sX;
				m_object_id_cache_loc_y[object_id] = sY;
				found_old = true;
			}
		}
		if (!found_old && m_object_id_cache_loc_x[object_id] < 0)
		{
			iX = abs(m_object_id_cache_loc_x[object_id]) - m_pivot_x;
			iY = abs(m_object_id_cache_loc_y[object_id]) - m_pivot_y;
			if ((iX < 0) || (iX >= MapDataSizeX) || (iY < 0) || (iY >= MapDataSizeY))
			{
				m_object_id_cache_loc_x[object_id] = 0;
				m_object_id_cache_loc_y[object_id] = 0;
				return false;
			}
			if ((m_data[iX][iY].m_dead_owner_frame == -1) && (m_data[iX][iY].m_dead_object_id == object_id))
			{
				chat_index = m_data[iX][iY].m_dead_chat_msg;
				effect_type = m_data[iX][iY].m_effect_type;
				effect_frame = m_data[iX][iY].m_effect_frame;
				effect_total_frame = m_data[iX][iY].m_effect_total_frame;
				m_data[iX][iY].m_dead_object_id = 0;
				m_data[iX][iY].m_dead_chat_msg = 0; // v1.4
				m_data[iX][iY].m_dead_owner_type = 0;
				m_object_id_cache_loc_x[object_id] = -1 * sX;
				m_object_id_cache_loc_y[object_id] = -1 * sY;
				found_old = true;
			}
		}

		if (!found_old)
		{
			add = 7;
			for (iX = sX - add; iX <= sX + add && !found_old; iX++)
				for (iY = sY - add; iY <= sY + add; iY++)
				{
					if (iX < m_pivot_x) break;
					else if (iX >= m_pivot_x + MapDataSizeX) break;
					if (iY < m_pivot_y) break;
					else if (iY >= m_pivot_y + MapDataSizeY) break;
					if (m_data[iX - m_pivot_x][iY - m_pivot_y].m_object_id == object_id)
					{
						chat_index = m_data[iX - m_pivot_x][iY - m_pivot_y].m_chat_msg;
						effect_type = m_data[iX - m_pivot_x][iY - m_pivot_y].m_effect_type;
						effect_frame = m_data[iX - m_pivot_x][iY - m_pivot_y].m_effect_frame;
						effect_total_frame = m_data[iX - m_pivot_x][iY - m_pivot_y].m_effect_total_frame;
						m_data[iX - m_pivot_x][iY - m_pivot_y].m_object_id = 0; //-1; v1.41
						m_data[iX - m_pivot_x][iY - m_pivot_y].m_chat_msg = 0;
						m_data[iX - m_pivot_x][iY - m_pivot_y].m_owner_type = 0;
						m_data[iX - m_pivot_x][iY - m_pivot_y].m_npc_config_id = -1;
						m_data[iX - m_pivot_x][iY - m_pivot_y].m_effect_type = 0;
						m_data[iX - m_pivot_x][iY - m_pivot_y].m_owner_name.clear();
						m_object_id_cache_loc_x[object_id] = sX;
						m_object_id_cache_loc_y[object_id] = sY;
						found_old = true;
						break;
					}

					if (m_data[iX - m_pivot_x][iY - m_pivot_y].m_dead_object_id == object_id)
					{
						chat_index = m_data[iX - m_pivot_x][iY - m_pivot_y].m_dead_chat_msg;
						effect_type = m_data[iX - m_pivot_x][iY - m_pivot_y].m_effect_type;
						effect_frame = m_data[iX - m_pivot_x][iY - m_pivot_y].m_effect_frame;
						effect_total_frame = m_data[iX - m_pivot_x][iY - m_pivot_y].m_effect_total_frame;
						m_data[iX - m_pivot_x][iY - m_pivot_y].m_dead_object_id = 0; //-1; v1.41
						m_data[iX - m_pivot_x][iY - m_pivot_y].m_dead_chat_msg = 0;
						m_data[iX - m_pivot_x][iY - m_pivot_y].m_dead_owner_type = 0;
						m_data[iX - m_pivot_x][iY - m_pivot_y].m_dead_owner_name.clear();
						m_object_id_cache_loc_x[object_id] = -1 * sX;
						m_object_id_cache_loc_y[object_id] = -1 * sY;
						found_old = true;
						break;
					}
				}
		}
		if (!found_old)
		{
			m_object_id_cache_loc_x[object_id] = sX;
			m_object_id_cache_loc_y[object_id] = sY;
		}
	}
	else
	{
		if (action != Type::NullAction)// ObjectID
			object_id = hb::shared::object_id::ToRealID(object_id);
		// v1.5 Crash
		if (hb::shared::object_id::IsNearbyOffset(object_id)) return false;
		if (m_object_id_cache_loc_x[object_id] > 0)
		{
			iX = m_object_id_cache_loc_x[object_id] - m_pivot_x;
			iY = m_object_id_cache_loc_y[object_id] - m_pivot_y;
			if ((iX < 0) || (iX >= MapDataSizeX) || (iY < 0) || (iY >= MapDataSizeY))
			{
				m_object_id_cache_loc_x[object_id] = 0;
				m_object_id_cache_loc_y[object_id] = 0;
				return false;
			}
			if (m_data[iX][iY].m_object_id == object_id)
			{
				dX = iX;
				dY = iY;
				if (use_abs_pos) {
					dX = sX - m_pivot_x;
					dY = sY - m_pivot_y;
				}
				else {
					switch (action) {
					case Type::Run:
					case Type::Move:
					case Type::DamageMove:
					case Type::AttackMove:
						hb::shared::direction::ApplyOffset(dir, dX, dY);
						break;
					default:
						break;
					}
				}
				if ((object_id != static_cast<uint16_t>(m_game->m_player->m_player_object_id))
					&& (m_data[dX][dY].m_owner_type != 0) && (m_data[dX][dY].m_object_id != object_id))
				{
					m_game->request_full_object_data(object_id);
					name.clear();
					return false;
				}
				chat_index = m_data[iX][iY].m_chat_msg;
				if (action != Type::NullAction)
				{
					type = m_data[iX][iY].m_owner_type;
					localNpcConfigId = m_data[iX][iY].m_npc_config_id;
					localAppearance = m_data[iX][iY].m_appearance;
					localStatus = m_data[iX][iY].m_status;
					effect_type = m_data[iX][iY].m_effect_type;
					effect_frame = m_data[iX][iY].m_effect_frame;
					effect_total_frame = m_data[iX][iY].m_effect_total_frame;
				}
				tmp_name.clear();
				tmp_name = m_data[iX][iY].m_owner_name;
				name.clear();
				name = m_data[iX][iY].m_owner_name;
				m_data[iX][iY].m_object_id = 0; //-1; v1.41
				m_data[iX][iY].m_chat_msg = 0;
				m_data[iX][iY].m_owner_type = 0;
				m_data[iX][iY].m_npc_config_id = -1;
				m_data[iX][iY].m_effect_type = 0;
				m_data[iX][iY].m_owner_name.clear();
				m_object_id_cache_loc_x[object_id] = dX + m_pivot_x;
				m_object_id_cache_loc_y[object_id] = dY + m_pivot_y;
				found_old = true;
			}
		}
		if (!found_old && m_object_id_cache_loc_x[object_id] < 0)
		{
			iX = abs(m_object_id_cache_loc_x[object_id]) - m_pivot_x;
			iY = abs(m_object_id_cache_loc_y[object_id]) - m_pivot_y;
			if ((iX < 0) || (iX >= MapDataSizeX) || (iY < 0) || (iY >= MapDataSizeY))
			{
				m_object_id_cache_loc_x[object_id] = 0;
				m_object_id_cache_loc_y[object_id] = 0;
				return false;
			}
			if ((m_data[iX][iY].m_dead_owner_frame == -1) && (m_data[iX][iY].m_dead_object_id == object_id))
			{
				dX = iX;
				dY = iY;
				if (use_abs_pos) {
					dX = sX - m_pivot_x;
					dY = sY - m_pivot_y;
				}
				else {
					switch (action) {
					case Type::Move:
					case Type::Run:
					case Type::DamageMove:
					case Type::AttackMove:
						hb::shared::direction::ApplyOffset(dir, dX, dY);
						break;
					default:
						break;
					}
				}
				if ((object_id != static_cast<uint16_t>(m_game->m_player->m_player_object_id)) &&
					(m_data[dX][dY].m_owner_type != 0) && (m_data[dX][dY].m_object_id != object_id))
				{
					m_game->request_full_object_data(object_id);
					name.clear();
					return false;
				}
				chat_index = m_data[iX][iY].m_dead_chat_msg;
				if (action != Type::NullAction) {
					type = m_data[iX][iY].m_dead_owner_type;
					localAppearance = m_data[iX][iY].m_dead_appearance;
					localStatus = m_data[iX][iY].m_deadStatus;
				}
				tmp_name.clear();
				tmp_name = m_data[iX][iY].m_dead_owner_name;
				name.clear();
				name = m_data[iX][iY].m_dead_owner_name;
				m_data[iX][iY].m_dead_object_id = 0; // -1; v1.41
				m_data[iX][iY].m_dead_chat_msg = 0;
				m_data[iX][iY].m_dead_owner_type = 0;
				m_data[iX][iY].m_dead_owner_name.clear();
				m_object_id_cache_loc_x[object_id] = -1 * (dX + m_pivot_x);
				m_object_id_cache_loc_y[object_id] = -1 * (dY + m_pivot_y);
				found_old = true;
			}
		}

		if (!found_old)
		{
			for (iX = 0; iX < MapDataSizeX && !found_old; iX++)
				for (iY = 0; iY < MapDataSizeY; iY++)
				{
					if (m_data[iX][iY].m_object_id == object_id)
					{
						dX = iX;
						dY = iY;
						if (use_abs_pos) {
							dX = sX - m_pivot_x;
							dY = sY - m_pivot_y;
						}
						else {
							switch (action) {
							case Type::Run:
							case Type::Move:
							case Type::DamageMove:
							case Type::AttackMove:
								hb::shared::direction::ApplyOffset(dir, dX, dY);
								break;
							default:
								break;
							}
						}
						if ((object_id != static_cast<uint16_t>(m_game->m_player->m_player_object_id))
							&& (m_data[dX][dY].m_owner_type != 0) && (m_data[dX][dY].m_object_id != object_id))
						{
							m_game->request_full_object_data(object_id);
							name.clear();
							return false;
						}
						chat_index = m_data[iX][iY].m_chat_msg;
						if (action != Type::NullAction) {
							type = m_data[iX][iY].m_owner_type;
							localNpcConfigId = m_data[iX][iY].m_npc_config_id;
							localAppearance = m_data[iX][iY].m_appearance;
							localStatus = m_data[iX][iY].m_status;
							effect_type = m_data[iX][iY].m_effect_type;
							effect_frame = m_data[iX][iY].m_effect_frame;
							effect_total_frame = m_data[iX][iY].m_effect_total_frame;
						}
						tmp_name.clear();
						tmp_name = m_data[iX][iY].m_owner_name;
						name.clear();
						name = m_data[iX][iY].m_owner_name;
						m_data[iX][iY].m_object_id = 0; //-1; v1.41
						m_data[iX][iY].m_chat_msg = 0;
						m_data[iX][iY].m_owner_type = 0;
						m_data[iX][iY].m_npc_config_id = -1;
						m_data[iX][iY].m_effect_type = 0;
						m_data[iX][iY].m_owner_name.clear();
						m_object_id_cache_loc_x[object_id] = dX + m_pivot_x;
						m_object_id_cache_loc_y[object_id] = dY + m_pivot_y;
						found_old = true;
						break;
					}
					if (m_data[iX][iY].m_dead_object_id == object_id)
					{
						dX = iX;
						dY = iY;
						if (use_abs_pos) {
							dX = sX - m_pivot_x;
							dY = sY - m_pivot_y;
						}
						else {
							switch (action) {
							case Type::Move:
							case Type::Run:
							case Type::DamageMove:
							case Type::AttackMove:
								hb::shared::direction::ApplyOffset(dir, dX, dY);
								break;
							default:
								break;
							}
						}
						if ((object_id != static_cast<uint16_t>(m_game->m_player->m_player_object_id)) &&
							(m_data[dX][dY].m_owner_type != 0) && (m_data[dX][dY].m_object_id != object_id))
						{
							m_game->request_full_object_data(object_id);
							name.clear();
							return false;
						}
						chat_index = m_data[iX][iY].m_dead_chat_msg;
						if (action != Type::NullAction) {
							type = m_data[iX][iY].m_dead_owner_type;
							localNpcConfigId = m_data[iX][iY].m_dead_npc_config_id;
							localAppearance = m_data[iX][iY].m_dead_appearance;
							localStatus = m_data[iX][iY].m_deadStatus;
						}
						tmp_name.clear();
						tmp_name = m_data[iX][iY].m_dead_owner_name;
						name.clear();
						name = m_data[iX][iY].m_dead_owner_name;
						m_data[iX][iY].m_dead_object_id = 0; //-1; v1.41
						m_data[iX][iY].m_dead_chat_msg = 0;
						m_data[iX][iY].m_dead_owner_type = 0;
						m_data[iX][iY].m_dead_npc_config_id = -1;
						m_data[iX][iY].m_effect_type = 0;
						m_data[iX][iY].m_dead_owner_name.clear();
						m_object_id_cache_loc_x[object_id] = -1 * (dX + m_pivot_x);
						m_object_id_cache_loc_y[object_id] = -1 * (dY + m_pivot_y);
						found_old = true;
						break;
					}
				}
		}
		if (!found_old)
		{
			if (ShouldRequestFullData(object_id, sX, sY)) {
				m_game->request_full_object_data(object_id);
			}
			name.clear();
			return false;
		}
	}

	if (pre_loc == 0 && m_data[dX][dY].m_owner_type != 0)
	{
		if (action == Type::Dying)
		{
			dX = sX - m_pivot_x;
			dY = sY - m_pivot_y;
		}
		if (m_data[dX][dY].m_animation.m_action == Type::Dying)
		{
			m_data[dX][dY].m_dead_object_id = m_data[dX][dY].m_object_id;
			m_data[dX][dY].m_dead_owner_type = m_data[dX][dY].m_owner_type;
			m_data[dX][dY].m_dead_npc_config_id = m_data[dX][dY].m_npc_config_id;
			m_data[dX][dY].m_dead_dir = m_data[dX][dY].m_animation.m_dir;
			m_data[dX][dY].m_dead_appearance = m_data[dX][dY].m_appearance;
			m_data[dX][dY].m_deadStatus = m_data[dX][dY].m_status;
			m_data[dX][dY].m_dead_owner_frame = -1;
			m_data[dX][dY].m_dead_owner_time = time;
			m_data[dX][dY].m_dead_owner_name = m_data[dX][dY].m_owner_name;
			m_data[dX][dY].m_dead_chat_msg = m_data[dX][dY].m_chat_msg;
			m_data[dX][dY].m_object_id = 0;
			m_data[dX][dY].m_owner_type = 0;
			m_data[dX][dY].m_npc_config_id = -1;
			m_data[dX][dY].m_chat_msg = 0;
			m_data[dX][dY].m_owner_name.clear();
			m_object_id_cache_loc_x[m_data[dX][dY].m_dead_object_id] = -1 * m_object_id_cache_loc_x[m_data[dX][dY].m_dead_object_id];
			m_object_id_cache_loc_y[m_data[dX][dY].m_dead_object_id] = -1 * m_object_id_cache_loc_y[m_data[dX][dY].m_dead_object_id];

			if (m_data[dX][dY].m_effect_type != 0)
			{
				m_data[dX][dY].m_effect_type = 0;
				m_data[dX][dY].m_effect_frame = 0;
				m_data[dX][dY].m_effect_total_frame = 0;
				m_data[dX][dY].m_effect_time = 0;
			}
		}
	}

	if (m_data[dX][dY].m_owner_type != 0)
	{
		if ((object_id != static_cast<uint16_t>(m_game->m_player->m_player_object_id))
			&& (m_data[dX][dY].m_object_id == static_cast<uint16_t>(m_game->m_player->m_player_object_id)))
		{
			m_game->request_full_object_data(object_id);
			return false;
		}
		else
		{
			return false;
		}
	}

	if (pre_loc == 0)
	{
		m_data[dX][dY].m_object_id = object_id;
		m_data[dX][dY].m_owner_type = type;
		m_data[dX][dY].m_npc_config_id = localNpcConfigId;
		m_data[dX][dY].m_animation.m_dir = dir;
		m_data[dX][dY].m_appearance = localAppearance;
		m_data[dX][dY].m_status = localStatus;
		m_data[dX][dY].m_v1 = v1;
		m_data[dX][dY].m_v2 = v2;
		m_data[dX][dY].m_v3 = v3;
		m_data[dX][dY].m_effect_type = effect_type;
		m_data[dX][dY].m_effect_frame = effect_frame;
		m_data[dX][dY].m_effect_total_frame = effect_total_frame;
		m_data[dX][dY].m_owner_name.clear();
		m_data[dX][dY].m_owner_name = tmp_name;
		if ((action != Type::NullAction) && (action != MsgType::Confirm) && (action != MsgType::Reject))
		{
			// Look up animation definition: players use PlayerAnim, NPCs use m_stFrame
			int16_t maxFrame, frameTime;
			bool loop;
			if (hb::shared::owner::is_player(type)) {
				const AnimDef& def = PlayerAnim::from_action(static_cast<int8_t>(action));
				maxFrame = def.m_max_frame;
				frameTime = def.m_frame_time;
				loop = def.m_loop;
			} else {
				maxFrame = m_stFrame[type][action].m_sMaxFrame;
				frameTime = m_stFrame[type][action].m_sFrameTime;
				loop = false; // All actions are one-shot; overflow triggers STOP transition + command unlock
			}
			m_data[dX][dY].m_animation.set_action(static_cast<int8_t>(action), dir,
				maxFrame, frameTime, loop, static_cast<int8_t>(frame));

			// initialize smooth movement interpolation for movement actions
			if (action == Type::Move || action == Type::Run ||
				action == Type::DamageMove || action == Type::AttackMove)
			{
				bool hasHaste = localStatus.haste;
				bool frozen = localStatus.frozen;
				uint32_t duration = EntityMotion::get_duration_for_action(action, hasHaste, frozen);

				if (m_data[dX][dY].m_motion.is_moving())
				{
					// Entity still interpolating previous tile — queue this move
					m_data[dX][dY].m_motion.queue_move(dir, duration);
				}
				else if (had_old_motion)
				{
					// Seamless tile transition with direction-aware offset handling
					//
					// Key insight: Cardinal directions (N/S/E/W) only move ONE axis.
					// When changing from diagonal to cardinal, the "unused" axis has
					// a residual offset that would cause unwanted lateral movement.
					//
					// Solution: Only preserve offset in axes the NEW direction uses.
					// This may cause a small instant jump (1-3 pixels) but prevents
					// the "sliding" effect during cardinal movement.
					//
					int16_t stdStartX, stdStartY;
					EntityMotion::get_direction_start_offset(dir, stdStartX, stdStartY);

					float newOffsetX, newOffsetY;

					// Determine which axes the new direction uses
					bool usesX = (stdStartX != 0);  // E/W/NE/SE/SW/NW use X
					bool usesY = (stdStartY != 0);  // N/S/NE/SE/SW/NW use Y

					if (old_motion_dir == dir)
					{
						// Same direction: full blending for smooth continuous movement
						newOffsetX = old_motion_offset_x + static_cast<float>(stdStartX);
						newOffsetY = old_motion_offset_y + static_cast<float>(stdStartY);
					}
					else
					{
						// Direction changed: only blend axes the new direction uses
						newOffsetX = usesX ? (old_motion_offset_x + static_cast<float>(stdStartX))
						                   : static_cast<float>(stdStartX);
						newOffsetY = usesY ? (old_motion_offset_y + static_cast<float>(stdStartY))
						                   : static_cast<float>(stdStartY);
					}

					m_data[dX][dY].m_motion.start_move_with_offset(dir, time, duration, newOffsetX, newOffsetY);
				}
				else
				{
					// No previous motion - use standard start offset
					m_data[dX][dY].m_motion.start_move(dir, time, duration);
				}
			}
		}
		else
		{
			// NULLACTION/CONFIRM/REJECT: initialize with STOP animation if not already set
			if (m_data[dX][dY].m_animation.m_max_frame == 0)
			{
				int16_t maxFrame, frameTime;
				if (hb::shared::owner::is_player(type)) {
					maxFrame = PlayerAnim::stop.m_max_frame;
					frameTime = PlayerAnim::stop.m_frame_time;
				} else {
					maxFrame = m_stFrame[type][Type::stop].m_sMaxFrame;
					frameTime = m_stFrame[type][Type::stop].m_sFrameTime;
				}
				m_data[dX][dY].m_animation.set_action(Type::stop, dir,
					maxFrame, frameTime, false);
			}
		}
		m_data[dX][dY].m_chat_msg = chat_index;
		if (localAppearance.effect_type != 0)
		{
			m_data[dX][dY].m_effect_type = localAppearance.effect_type;
			if (action == Type::NullAction)
			{
				m_data[dX][dY].m_effect_frame = 0;
				m_data[dX][dY].m_effect_time = time;
			}
			switch (m_data[dX][dY].m_effect_type) {
			case 1: m_data[dX][dY].m_effect_total_frame = 13; break;
			case 2: m_data[dX][dY].m_effect_total_frame = 11; break;
			}
		}
		else
		{
			m_data[dX][dY].m_effect_type = 0;
		}
	}
	else // pre_loc == 1
	{
		m_data[dX][dY].m_dead_object_id = object_id;
		m_data[dX][dY].m_dead_owner_type = type;
		m_data[dX][dY].m_dead_npc_config_id = localNpcConfigId;
		m_data[dX][dY].m_dead_dir = dir;
		m_data[dX][dY].m_dead_appearance = localAppearance;
		m_data[dX][dY].m_deadStatus = localStatus;
		m_data[dX][dY].m_dead_owner_name.clear();
		m_data[dX][dY].m_dead_owner_name = tmp_name;
		m_data[dX][dY].m_dead_owner_time = time;
		m_data[dX][dY].m_dead_chat_msg = chat_index;
		if (localAppearance.effect_type != 0)
		{
			m_data[dX][dY].m_effect_type = localAppearance.effect_type;
			if (action == Type::NullAction)
			{
				m_data[dX][dY].m_effect_frame = 0;
				m_data[dX][dY].m_effect_time = time;
			}
			switch (m_data[dX][dY].m_effect_type) {
			case 1: m_data[dX][dY].m_effect_total_frame = 13; break;
			case 2: m_data[dX][dY].m_effect_total_frame = 11; break;
			}
		}
		else
		{
			m_data[dX][dY].m_effect_type = 0;
		}
	}
	return true;
}






int CMapData::object_frame_counter(const std::string& player_name, short view_point_x, short view_point_y)
{
	int dX, dY, val;
	uint32_t time, real_time, frame_time;
	int  delay;
	int  ret, sound_index;
	int  dir, total_frame, frame_move_dots;
	static uint32_t S_dwUpdateTime = GameClock::get_time_ms();
	int   weapon_type, center_x, center_y, dist;
	bool  auto_update = false, dynObjsNeedUpdate = false;
	short dx, dy;
	long  lPan;

	ret = 0;
	time = real_time = GameClock::get_time_ms();
	if ((time - m_frame_time) >= 1)
		m_frame_time = time;

	val = view_point_x - (m_pivot_x * 32);
	center_x = (val / 32) + VIEW_CENTER_TILE_X();
	val = view_point_y - (m_pivot_y * 32);
	center_y = (val / 32) + VIEW_CENTER_TILE_Y();
	m_rect_x = m_game->m_vdl_x - m_pivot_x;
	m_rect_y = m_game->m_vdl_y - m_pivot_y;

	dynObjsNeedUpdate = (time - m_dynamic_object_frame_time) > 100;
	auto_update = (time - S_dwUpdateTime) > 40;

	// PERFORMANCE OPTIMIZATION: Only process tiles near player's view
	// Screen is ~LOGICAL_WIDTHxLOGICAL_HEIGHT pixels = ~20x15 tiles, add buffer for effects
	// OLD: Processed all 60x55 = 3300 tiles every frame
	// NEW: Process only ~35x30 = 1050 tiles (68% reduction)
	int halfViewX = VIEW_TILE_WIDTH() / 2;
	int halfViewY = VIEW_TILE_HEIGHT() / 2;
	int bufferX = 5;
	int bufferY = 6;
	int startX = center_x - (halfViewX + bufferX);
	int endX = center_x + (halfViewX + bufferX + 1);
	int startY = center_y - (halfViewY + bufferY);
	int endY = center_y + (halfViewY + bufferY);
	if (startX < 0) startX = 0;
	if (startY < 0) startY = 0;
	if (endX > MapDataSizeX) endX = MapDataSizeX;
	if (endY > MapDataSizeY) endY = MapDataSizeY;

	for (dX = startX; dX < endX; dX++)
		for (dY = startY; dY < endY; dY++)
		{
			dist = (abs(center_x - dX) + abs(center_y - dY)) / 2;
			lPan = halfViewX > 0 ? ((dX - center_x) * 100) / halfViewX : 0;

			// Dynamic Object
			if (dynObjsNeedUpdate)//00496B99  JBE 00496F43
			{
				m_data[dX][dY].m_effect_frame++;
				switch (m_data[dX][dY].m_effect_type) {
				case 1:
					if (m_data[dX][dY].m_effect_total_frame < m_data[dX][dY].m_effect_frame)
						m_data[dX][dY].m_effect_frame = 4;
					break;
				case 2:
					if (m_data[dX][dY].m_effect_total_frame < m_data[dX][dY].m_effect_frame)
						m_data[dX][dY].m_effect_frame = 3;
					break;
				}
				if ((m_data[dX][dY].m_dynamic_object_type != 0))
				{
					m_data[dX][dY].m_dynamic_object_frame++;
					switch (m_data[dX][dY].m_dynamic_object_type) {
					case dynamic_object::Spike:
						if (m_data[dX][dY].m_dynamic_object_frame >= 13)
							m_data[dX][dY].m_dynamic_object_frame = 0;
						break;

					case dynamic_object::IceStorm:
						if (m_data[dX][dY].m_dynamic_object_frame >= 10)
							m_data[dX][dY].m_dynamic_object_frame = 0;
						break;

					case dynamic_object::Fire:// Firewall
					case dynamic_object::Fire3: // by Snoopy(FireBow)
						if (m_data[dX][dY].m_dynamic_object_frame >= 24)
							m_data[dX][dY].m_dynamic_object_frame = 0;
						if (m_data[dX][dY].m_dynamic_object_frame == 1)
						{
							audio_manager::get().play_game_sound(sound_type::effect, 9, dist);
						}
						break;

					case dynamic_object::Fire2:	//  // Crusade buildings burning.
						if (m_data[dX][dY].m_dynamic_object_frame > 27)
							m_data[dX][dY].m_dynamic_object_frame = 0;
						if (m_data[dX][dY].m_dynamic_object_frame == 1)
						{
							audio_manager::get().play_game_sound(sound_type::effect, 9, dist);
						}
						if ((m_data[dX][dY].m_dynamic_object_frame % 6) == 0)
						{
							m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_CASTING, (m_pivot_x + dX) * 32 + (rand() % 10 - 5) + 5, (m_pivot_y + dY) * 32, 0, 0, 0, 0);
							m_game->m_effect_manager->add_effect(EffectType::MS_FIRE_SMOKE, (m_pivot_x + dX) * 32, (m_pivot_y + dY) * 32, 0, 0, 0, 0);
						}
						break;

					case dynamic_object::FishObject:
						if ((rand() % 12) == 1)
							m_game->m_effect_manager->add_effect(EffectType::BUBBLES_DRUNK, (m_pivot_x + dX) * 32 + m_data[dX][dY].m_dynamic_object_data_1, (m_pivot_y + dY) * 32 + m_data[dX][dY].m_dynamic_object_data_2, 0, 0, 0);
						break;

					case dynamic_object::Fish:
						if ((time - m_data[dX][dY].m_dynamic_object_time) < 100) break;
						m_data[dX][dY].m_dynamic_object_time = time;
						if (m_data[dX][dY].m_dynamic_object_frame >= 15) m_data[dX][dY].m_dynamic_object_frame = 0;
						if ((rand() % 15) == 1) m_game->m_effect_manager->add_effect(EffectType::BUBBLES_DRUNK, (m_pivot_x + dX) * 32 + m_data[dX][dY].m_dynamic_object_data_1, (m_pivot_y + dY) * 32 + m_data[dX][dY].m_dynamic_object_data_2, 0, 0, 0);
						dir = CMisc::get_next_move_dir(m_data[dX][dY].m_dynamic_object_data_1, m_data[dX][dY].m_dynamic_object_data_2, 0, 0);
						switch (dir) {
						case 1:
							m_data[dX][dY].m_dynamic_object_data_4 -= 2;
							break;
						case 2:
							m_data[dX][dY].m_dynamic_object_data_4 -= 2;
							m_data[dX][dY].m_dynamic_object_data_3 += 2;
							break;
						case 3:
							m_data[dX][dY].m_dynamic_object_data_3 += 2;
							break;
						case 4:
							m_data[dX][dY].m_dynamic_object_data_3 += 2;
							m_data[dX][dY].m_dynamic_object_data_4 += 2;
							break;
						case 5:
							m_data[dX][dY].m_dynamic_object_data_4 += 2;
							break;
						case 6:
							m_data[dX][dY].m_dynamic_object_data_3 -= 2;
							m_data[dX][dY].m_dynamic_object_data_4 += 2;
							break;
						case 7:
							m_data[dX][dY].m_dynamic_object_data_3 -= 2;
							break;
						case 8:
							m_data[dX][dY].m_dynamic_object_data_3 -= 2;
							m_data[dX][dY].m_dynamic_object_data_4 -= 2;
							break;
						}

						if (m_data[dX][dY].m_dynamic_object_data_3 < -12) m_data[dX][dY].m_dynamic_object_data_3 = -12;
						if (m_data[dX][dY].m_dynamic_object_data_3 > 12) m_data[dX][dY].m_dynamic_object_data_3 = 12;
						if (m_data[dX][dY].m_dynamic_object_data_4 < -12) m_data[dX][dY].m_dynamic_object_data_4 = -12;
						if (m_data[dX][dY].m_dynamic_object_data_4 > 12) m_data[dX][dY].m_dynamic_object_data_4 = 12;

						m_data[dX][dY].m_dynamic_object_data_1 += m_data[dX][dY].m_dynamic_object_data_3;
						m_data[dX][dY].m_dynamic_object_data_2 += m_data[dX][dY].m_dynamic_object_data_4;
						break;

					case dynamic_object::PCloudBegin:
						if (m_data[dX][dY].m_dynamic_object_frame >= 8)
						{
							m_data[dX][dY].m_dynamic_object_type = dynamic_object::PCloudLoop;
							m_data[dX][dY].m_dynamic_object_frame = rand() % 8;
						}
						break;

					case dynamic_object::PCloudLoop:
						if (m_data[dX][dY].m_dynamic_object_frame >= 8)
						{
							m_data[dX][dY].m_dynamic_object_frame = 0;
						}
						break;

					case dynamic_object::PCloudEnd:
						if (m_data[dX][dY].m_dynamic_object_frame >= 8)
						{
							m_data[dX][dY].m_dynamic_object_type = 0;
						}
						break;

					case dynamic_object::AresdenFlag1:
						if (m_data[dX][dY].m_dynamic_object_frame >= 4)
						{
							m_data[dX][dY].m_dynamic_object_frame = 0;
						}
						break;

					case dynamic_object::ElvineFlag1:
						if (m_data[dX][dY].m_dynamic_object_frame >= 8)
						{
							m_data[dX][dY].m_dynamic_object_frame = 4;
						}
						break;
					}
				}
			}

			// Dead think 00496F43
			if (m_data[dX][dY].m_dead_owner_type != 0)
			{
				// Player corpses persist until revive/restart — only NPC corpses auto-fade
				bool is_player_corpse = hb::shared::owner::is_player(m_data[dX][dY].m_dead_owner_type);
				if (!is_player_corpse && (m_data[dX][dY].m_dead_owner_frame == -1) && ((time - m_data[dX][dY].m_dead_owner_time) > corpse_linger_time_ms))
				{
					// Auto-start fade after linger delay (NPCs only)
					m_data[dX][dY].m_dead_owner_frame = 0;
					m_data[dX][dY].m_dead_owner_time = time;
					if (ret == 0)
					{
						ret = -1;
						S_dwUpdateTime = time;
					}
				}
				else if ((m_data[dX][dY].m_dead_owner_frame >= 0) && ((time - m_data[dX][dY].m_dead_owner_time) > 150))
				{
					m_data[dX][dY].m_dead_owner_time = time;
					m_data[dX][dY].m_dead_owner_frame++;
					if (ret == 0)
					{
						ret = -1;
						S_dwUpdateTime = time;
					}
					if (m_data[dX][dY].m_dead_owner_frame > 10)
					{
						m_data[dX][dY].m_dead_object_id = 0;
						m_data[dX][dY].m_dead_owner_type = 0;
						m_data[dX][dY].m_dead_owner_name.clear();
					}
				}
			}

			// Alive thing 00496FD8
			if (m_data[dX][dY].m_owner_type != 0)
			{
				// get base frame time from source (not the already-modified frame_time)
				int16_t baseFrameTime;
				short ownerType = m_data[dX][dY].m_owner_type;
				int8_t ownerAction = m_data[dX][dY].m_animation.m_action;
				if (hb::shared::owner::is_player(ownerType)) {
					baseFrameTime = PlayerAnim::from_action(ownerAction).m_frame_time;
				} else {
					baseFrameTime = m_stFrame[ownerType][ownerAction].m_sFrameTime;
				}

				// Compute effective frame time with status modifiers
				switch (ownerAction) {
				case Type::Attack: // 3
				case Type::AttackMove:	// 8
					delay = m_data[dX][dY].m_status.attack_delay * 12;
					break;
				case Type::Magic: // 4
					if (m_game->m_player->m_skill_mastery[4] == 100) delay = -17;
					else delay = 0;
					break;
				default:
					delay = 0;
					break;
				}
				// v1.42 Frozen
				if (m_data[dX][dY].m_status.frozen)
					delay += baseFrameTime >> 2;

				if (m_data[dX][dY].m_status.haste) { // haste
					int16_t runFrameTime = hb::shared::owner::is_player(ownerType)
						? PlayerAnim::Run.m_frame_time
						: m_stFrame[ownerType][Type::Run].m_sFrameTime;
					delay -= static_cast<int>(runFrameTime / 2.3);
				}

				// Apply computed delay to animation state
				frame_time = baseFrameTime + delay;
				m_data[dX][dY].m_animation.m_frame_time = static_cast<int16_t>(frame_time);

				if (m_data[dX][dY].m_animation.update(time))
				{
					if (ret == 0)
					{
						ret = -1;
						S_dwUpdateTime = time;
					}
					if (m_data[dX][dY].m_owner_name == player_name)
					{
						ret = 1;
						S_dwUpdateTime = time;
						if ((real_time - m_frame_check_time) > frame_time)
							m_frame_adjust_time = ((real_time - m_frame_check_time) - frame_time);
						m_frame_check_time = real_time;
					}
					if (m_data[dX][dY].m_animation.is_finished())
					{
						if ((m_rect_x <= dX) && ((m_rect_x + 25) >= dX)
							&& (m_rect_y <= dY) && ((m_rect_y + 19) >= dY))
							// (!) Ower -> DeadOwner 004971AB
						{
							if (m_data[dX][dY].m_animation.m_action == Type::Dying) //10
							{
								m_data[dX][dY].m_dead_object_id = m_data[dX][dY].m_object_id;
								m_data[dX][dY].m_dead_owner_type = m_data[dX][dY].m_owner_type;
								m_data[dX][dY].m_dead_npc_config_id = m_data[dX][dY].m_npc_config_id;
								m_data[dX][dY].m_dead_dir = m_data[dX][dY].m_animation.m_dir;
								m_data[dX][dY].m_dead_appearance = m_data[dX][dY].m_appearance;
								m_data[dX][dY].m_deadStatus = m_data[dX][dY].m_status;
								m_data[dX][dY].m_dead_chat_msg = m_data[dX][dY].m_chat_msg; // v1.411
								m_data[dX][dY].m_dead_owner_frame = -1;
								m_data[dX][dY].m_dead_owner_time = time;
								m_data[dX][dY].m_dead_owner_name = m_data[dX][dY].m_owner_name;
								m_data[dX][dY].m_object_id = 0;
								m_data[dX][dY].m_owner_type = 0;
								m_data[dX][dY].m_npc_config_id = -1;
								m_data[dX][dY].m_owner_name.clear();
								m_object_id_cache_loc_x[m_data[dX][dY].m_dead_object_id] = -1 * m_object_id_cache_loc_x[m_data[dX][dY].m_dead_object_id];
								m_object_id_cache_loc_y[m_data[dX][dY].m_dead_object_id] = -1 * m_object_id_cache_loc_y[m_data[dX][dY].m_dead_object_id];
							}
							else
							{
								// Transition to STOP: use player or NPC anim defs
								int16_t stopMaxFrame, stopFrameTime;
								bool stopLoop;
								if (hb::shared::owner::is_player(m_data[dX][dY].m_owner_type)) {
									const AnimDef& def = PlayerAnim::from_action(Type::stop);
									stopMaxFrame = def.m_max_frame;
									stopFrameTime = def.m_frame_time;
									stopLoop = def.m_loop;
								} else {
									stopMaxFrame = m_stFrame[m_data[dX][dY].m_owner_type][Type::stop].m_sMaxFrame;
									stopFrameTime = m_stFrame[m_data[dX][dY].m_owner_type][Type::stop].m_sFrameTime;
									stopLoop = false;
								}
								m_data[dX][dY].m_animation.set_action(Type::stop,
									m_data[dX][dY].m_animation.m_dir,
									stopMaxFrame, stopFrameTime, stopLoop);
							}
							if (m_data[dX][dY].m_owner_name == player_name)
							{
								ret = 2;
								S_dwUpdateTime = time;
							}
						}
						else
						{
							m_data[dX][dY].m_object_id = 0;
							m_data[dX][dY].m_owner_type = 0;
							m_data[dX][dY].m_npc_config_id = -1;
							m_data[dX][dY].m_owner_name.clear();
							m_game->get_floating_text().clear(m_data[dX][dY].m_chat_msg);
						}
					}
					if (m_data[dX][dY].m_animation.m_action == Type::stop) { // Type::stop = 1 // 00497334
						switch (m_data[dX][dY].m_owner_type) {
						case 1:
						case 2:
						case 3:
						case 4:
						case 5:
						case 6: // glowing armor/weapon
							if ((m_data[dX][dY].m_animation.m_current_frame == 1) || (m_data[dX][dY].m_animation.m_current_frame == 5))
							{
								if ((((m_data[dX][dY].m_appearance.weapon_glare | m_data[dX][dY].m_appearance.shield_glare) != 0) || (m_data[dX][dY].m_status.gm_mode)) && (!m_data[dX][dY].m_status.invisibility))
								{
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + (rand() % 20 - 10), (m_pivot_y + dY) * 32 - (rand() % 50) - 5, 0, 0, -(rand() % 8), 0);
								}
								// Snoopy: Angels
								if (((m_data[dX][dY].m_status.angel_percent) > rand() % 6) // Angel stars
									&& (m_data[dX][dY].m_status.HasAngelType())
									&& (!m_data[dX][dY].m_status.invisibility))
								{
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + (rand() % 15 + 10), (m_pivot_y + dY) * 32 - (rand() % 30) - 50, 0, 0, -(rand() % 8), 0);
								}
							}
							break;
						case hb::shared::owner::EnergyShield: // ESG
						case hb::shared::owner::GrandMagicGenerator: // GMG
						case hb::shared::owner::ManaStone: // ManaStone
							if ((rand() % 40) == 25)
							{
								m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + (rand() % 60 - 30), (m_pivot_y + dY) * 32 - (rand() % 100) - 5, 0, 0, -(rand() % 12), 0);
								m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + (rand() % 60 - 30), (m_pivot_y + dY) * 32 - (rand() % 100) - 5, 0, 0, -(rand() % 12), 0);
								m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + (rand() % 60 - 30), (m_pivot_y + dY) * 32 - (rand() % 100) - 5, 0, 0, -(rand() % 12), 0);
								m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + (rand() % 60 - 30), (m_pivot_y + dY) * 32 - (rand() % 100) - 5, 0, 0, -(rand() % 12), 0);
							}
							break;
						case hb::shared::owner::IceGolem: // IceGolem
							if (m_data[dX][dY].m_animation.m_current_frame == 3)
							{
								m_game->m_effect_manager->add_effect(EffectType::ICE_GOLEM_EFFECT_1, (m_pivot_x + dX) * 32 + (rand() % 40 - 20), (m_pivot_y + dY) * 32 + (rand() % 40 - 20), 0, 0, 0);
								m_game->m_effect_manager->add_effect(EffectType::ICE_GOLEM_EFFECT_1, (m_pivot_x + dX) * 32 + (rand() % 40 - 20), (m_pivot_y + dY) * 32 + (rand() % 40 - 20), 0, 0, 0);
							}
							if (m_data[dX][dY].m_animation.m_current_frame == 2)
							{
								m_game->m_effect_manager->add_effect(EffectType::ICE_GOLEM_EFFECT_2, (m_pivot_x + dX) * 32 + (rand() % 40 - 20), (m_pivot_y + dY) * 32 + (rand() % 40 - 20), 0, 0, 0);
								m_game->m_effect_manager->add_effect(EffectType::ICE_GOLEM_EFFECT_2, (m_pivot_x + dX) * 32 + (rand() % 40 - 20), (m_pivot_y + dY) * 32 + (rand() % 40 - 20), 0, 0, 0);
							}
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
							{
								m_game->m_effect_manager->add_effect(EffectType::ICE_GOLEM_EFFECT_3, (m_pivot_x + dX) * 32 + (rand() % 40 - 20), (m_pivot_y + dY) * 32 + (rand() % 40 - 20), 0, 0, 0);
								m_game->m_effect_manager->add_effect(EffectType::ICE_GOLEM_EFFECT_3, (m_pivot_x + dX) * 32 + (rand() % 40 - 20), (m_pivot_y + dY) * 32 + (rand() % 40 - 20), 0, 0, 0);
							}
							break;
						}
					}

					if (m_data[dX][dY].m_animation.m_action == Type::Move) { //2 //004977BF
						switch (m_data[dX][dY].m_owner_type) {
						case 1:
						case 2:
						case 3:
						case 4:
						case 5:
						case 6:
						case hb::shared::owner::TempleKnight: // TK
						case hb::shared::owner::Beholder: // Beholder
						case hb::shared::owner::DarkElf: // Dark-Elf
							if ((m_data[dX][dY].m_animation.m_current_frame == 1) || (m_data[dX][dY].m_animation.m_current_frame == 5))
							{
								audio_manager::get().play_game_sound(sound_type::character, 8, dist, lPan);
								if ((((m_data[dX][dY].m_appearance.weapon_glare | m_data[dX][dY].m_appearance.shield_glare) != 0) || (m_data[dX][dY].m_status.gm_mode)) && (!m_data[dX][dY].m_status.invisibility))
								{
									total_frame = 8;
									frame_move_dots = 32 / total_frame;
									dx = dy = 0;
									switch (m_data[dX][dY].m_animation.m_dir) {
									case 1: dy = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
									case 2: dy = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); dx = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
									case 3: dx = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
									case 4: dx = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); dy = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
									case 5: dy = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
									case 6: dy = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); dx = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
									case 7: dx = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
									case 8: dx = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); dy = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
									}
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + dx + (rand() % 20 - 10), (m_pivot_y + dY) * 32 + dy - (rand() % 50) - 5, 0, 0, -(rand() % 8), 0);
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + dx + (rand() % 20 - 10), (m_pivot_y + dY) * 32 + dy - (rand() % 50) - 5, 0, 0, -(rand() % 8), 0);
								}
								// Snoopy: Angels
								if (((m_data[dX][dY].m_status.angel_percent) > rand() % 6) // Angel stars
									&& (m_data[dX][dY].m_status.HasAngelType())
									&& (!m_data[dX][dY].m_status.invisibility))
								{
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + (rand() % 15 + 10), (m_pivot_y + dY) * 32 - (rand() % 30) - 50, 0, 0, -(rand() % 8), 0);
								}
							}
							break;

						case hb::shared::owner::Sorceress: // Snoopy: Sorceress
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 149, dist, lPan);
							break;

						case hb::shared::owner::ATK: // Snoopy: ATK
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 142, dist, lPan);
							break;

						case hb::shared::owner::MasterElf: // Snoopy: MasterElf
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
							{
								if (true) audio_manager::get().play_game_sound(sound_type::character, 10, dist, lPan);
							}
							break;

						case hb::shared::owner::DSK: // Snoopy: DSK
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 147, dist, lPan);
							break;

						case hb::shared::owner::Slime: // Slime
						case hb::shared::owner::TigerWorm: // TW
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 1, dist, lPan);
							break;

						case hb::shared::owner::Skeleton: // SKel
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 13, dist, lPan);
							break;

						case hb::shared::owner::Cyclops: // Cyclops
						case hb::shared::owner::HellClaw: // HC
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 41, dist, lPan);
							break;

						case hb::shared::owner::OrcMage: // Orc
						case hb::shared::owner::Stalker: // SK
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 9, dist, lPan);
							break;

						case hb::shared::owner::GiantAnt: // Ant
						case hb::shared::owner::LightWarBeetle: // LWBeetle
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 29, dist, lPan);
							break;

						case hb::shared::owner::Scorpion: // Scorpion
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 21, dist, lPan);
							break;

						case hb::shared::owner::Zombie: // Zombie
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 17, dist, lPan);
							break;

						case hb::shared::owner::Amphis: // Snake
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 25, dist, lPan);
							break;

						case hb::shared::owner::ClayGolem: // Clay-Golem
						case hb::shared::owner::Gargoyle: // Gargoyle
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 37, dist, lPan);
							break;

						case hb::shared::owner::Hellhound: // HH
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 5, dist, lPan);
							break;

						case hb::shared::owner::Troll: // Troll
						case hb::shared::owner::Minaus: // Snoopy: Ajout Minaus
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 46, dist, lPan);
							break;

						case hb::shared::owner::Ogre: // Ogre
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 51, dist, lPan);
							break;

						case hb::shared::owner::Liche: // Liche
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 55, dist, lPan);
							break;

						case hb::shared::owner::Demon: // DD
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 59, dist, lPan);
							break;

						case hb::shared::owner::Unicorn: // Uni
						case hb::shared::owner::GodsHandKnightCK: // GHKABS
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 63, dist, lPan);
							break;

						case hb::shared::owner::WereWolf: // WW
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 67, dist, lPan);
							break;

						case hb::shared::owner::Bunny://Rabbit
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 71, dist, lPan);
							break;

						case hb::shared::owner::Cat://Cat
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 72, dist, lPan);
							break;

						case hb::shared::owner::GiantFrog://Giant-Frog
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 73, dist, lPan);
							break;

						case hb::shared::owner::MountainGiant://Mountain Giant
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 87, dist, lPan);
							break;

						case hb::shared::owner::Ettin://Ettin
						case hb::shared::owner::MasterOrc: // Snoopy: MasterMageOrc
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 91, dist, lPan);
							break;

						case hb::shared::owner::CannibalPlant://Cannibal Plant
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 95, dist, lPan);
							break;

						case hb::shared::owner::Rudolph://Rudolph
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::character, 11, dist, lPan);
							break;

						case hb::shared::owner::DireBoar: // DireBoar
						case hb::shared::owner::GiantCrayfish: // Snoopy: GiantCrayFish
						case hb::shared::owner::Barbarian: // Snoopy: Barbarian
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 87, dist, lPan);
							break;

						case hb::shared::owner::Frost://Frost
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 25, dist, lPan);
							break;

						case hb::shared::owner::StoneGolem: // Stone-Golem
						case hb::shared::owner::BattleGolem: // BG
						case hb::shared::owner::IceGolem: // Snoopy: IceGolem
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 33, dist, lPan);
							break;

						case hb::shared::owner::FireWyvern: // Snoopy: Fite-Wyvern
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 106, dist, lPan);
							break;

						case hb::shared::owner::Tentocle: // Snoopy: Tentocle
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 110, dist, lPan);
							break;

						case hb::shared::owner::ClawTurtle: // Snoopy: Claw Turtle
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 114, dist, lPan);
							break;

						case hb::shared::owner::Centaur: // Snoopy: Centaurus
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 117, dist, lPan);
							break;

						case hb::shared::owner::GiTree: // Snoopy: Giant Tree
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 122, dist, lPan);
							break;

						case hb::shared::owner::GiLizard: // Snoopy: Giant Lizard
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 126, dist, lPan);
							break;

						case hb::shared::owner::Dragon: // Snoopy: Dragon
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 130, dist, lPan);
							break;

						case hb::shared::owner::Nizie: // Snoopy: Nizie
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 134, dist, lPan);
							break;

						case hb::shared::owner::Abaddon: // void CGame::DrawDruncncity();Abaddon
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 136, dist, lPan);
							break;

						default:
							if ((m_data[dX][dY].m_animation.m_current_frame == 1) || (m_data[dX][dY].m_animation.m_current_frame == 3))
								audio_manager::get().play_game_sound(sound_type::character, 8, dist, lPan);
							break;
						}
					} // Fin du Type::Move

					if (m_data[dX][dY].m_animation.m_action == Type::Run)  // 2   //00497E34
					{
						switch (m_data[dX][dY].m_owner_type) {
						case 1:
						case 2:
						case 3:
						case 4:
						case 5:
						case 6:
						case hb::shared::owner::GodsHandKnight: // GHK
							if ((m_data[dX][dY].m_animation.m_current_frame == 1) || (m_data[dX][dY].m_animation.m_current_frame == 5))
							{
								total_frame = 8;
								frame_move_dots = 32 / total_frame;
								dx = dy = 0;
								switch (m_data[dX][dY].m_animation.m_dir) {
								case 1: dy = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
								case 2: dy = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); dx = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
								case 3: dx = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
								case 4: dx = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); dy = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
								case 5: dy = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
								case 6: dy = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); dx = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
								case 7: dx = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
								case 8: dx = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); dy = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
								}
								if (weather_manager::get().is_raining())
									m_game->m_effect_manager->add_effect(EffectType::FOOTPRINT_RAIN, (m_pivot_x + dX) * 32 + dx, (m_pivot_y + dY) * 32 + dy, 0, 0, 0, 0);
								else m_game->m_effect_manager->add_effect(EffectType::FOOTPRINT, (m_pivot_x + dX) * 32 + dx, (m_pivot_y + dY) * 32 + dy, 0, 0, 0, 0);
								if ((((m_data[dX][dY].m_appearance.weapon_glare | m_data[dX][dY].m_appearance.shield_glare) != 0) || (m_data[dX][dY].m_status.gm_mode)) && (!m_data[dX][dY].m_status.invisibility))
								{
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + dx + (rand() % 20 - 10), (m_pivot_y + dY) * 32 + dy - (rand() % 50) - 5, 0, 0, -(rand() % 8), 0);
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + dx + (rand() % 20 - 10), (m_pivot_y + dY) * 32 + dy - (rand() % 50) - 5, 0, 0, -(rand() % 8), 0);
								}
								// Snoopy: Angels
								if (((m_data[dX][dY].m_status.angel_percent) > rand() % 6) // Angel stars
									&& (m_data[dX][dY].m_status.HasAngelType())
									&& (!m_data[dX][dY].m_status.invisibility))
								{
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + (rand() % 15 + 10), (m_pivot_y + dY) * 32 - (rand() % 30) - 50, 0, 0, -(rand() % 8), 0);
								}
								audio_manager::get().play_game_sound(sound_type::character, 10, dist, lPan);
							}
							break;
						}
					}
					if (m_data[dX][dY].m_animation.m_action == Type::AttackMove)  //8 //004980A5
					{
						switch (m_data[dX][dY].m_owner_type) {
						case 1:
						case 2:
						case 3:
						case 4:
						case 5:
						case 6:
							if (m_data[dX][dY].m_animation.m_current_frame == 2) // vu comme case 2
							{
								if (true) audio_manager::get().play_game_sound(sound_type::character, 4, dist); //bruit fleche
								total_frame = 8;
								frame_move_dots = 32 / total_frame;
								dx = dy = 0;
								switch (m_data[dX][dY].m_animation.m_dir) {
								case 1: dy = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
								case 2: dy = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); dx = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
								case 3: dx = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
								case 4: dx = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); dy = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
								case 5: dy = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
								case 6: dy = -frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); dx = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
								case 7: dx = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
								case 8: dx = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); dy = frame_move_dots * (total_frame - m_data[dX][dY].m_animation.m_current_frame); break;
								}
								if ((((m_data[dX][dY].m_appearance.weapon_glare | m_data[dX][dY].m_appearance.shield_glare) != 0) || (m_data[dX][dY].m_status.gm_mode)) && (!m_data[dX][dY].m_status.invisibility))
								{
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + dx + (rand() % 20 - 10), (m_pivot_y + dY) * 32 + dy - (rand() % 50) - 5, 0, 0, -(rand() % 8), 0);
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + dx + (rand() % 20 - 10), (m_pivot_y + dY) * 32 + dy - (rand() % 50) - 5, 0, 0, -(rand() % 8), 0);
								}
								//Snoopy: Angels						
								if (((m_data[dX][dY].m_status.angel_percent) > rand() % 6) // Angel stars
									&& (m_data[dX][dY].m_status.HasAngelType())
									&& (!m_data[dX][dY].m_status.invisibility))
								{
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + (rand() % 15 + 10), (m_pivot_y + dY) * 32 - (rand() % 30) - 50, 0, 0, -(rand() % 8), 0);
								}
							}
							else if (m_data[dX][dY].m_animation.m_current_frame == 4) // vu comme case 4
							{
								if (weather_manager::get().is_raining())
								{
									m_game->m_effect_manager->add_effect(EffectType::FOOTPRINT_RAIN, (m_pivot_x + dX) * 32 + ((rand() % 20) - 10), (m_pivot_y + dY) * 32 + ((rand() % 20) - 10), 0, 0, 0, 0);
									m_game->m_effect_manager->add_effect(EffectType::FOOTPRINT_RAIN, (m_pivot_x + dX) * 32 + ((rand() % 20) - 10), (m_pivot_y + dY) * 32 + ((rand() % 20) - 10), 0, 0, 0, 0);
									m_game->m_effect_manager->add_effect(EffectType::FOOTPRINT_RAIN, (m_pivot_x + dX) * 32 + ((rand() % 20) - 10), (m_pivot_y + dY) * 32 + ((rand() % 20) - 10), 0, 0, 0, 0);
									m_game->m_effect_manager->add_effect(EffectType::FOOTPRINT_RAIN, (m_pivot_x + dX) * 32 + ((rand() % 20) - 10), (m_pivot_y + dY) * 32 + ((rand() % 20) - 10), 0, 0, 0, 0);
									m_game->m_effect_manager->add_effect(EffectType::FOOTPRINT_RAIN, (m_pivot_x + dX) * 32 + ((rand() % 20) - 10), (m_pivot_y + dY) * 32 + ((rand() % 20) - 10), 0, 0, 0, 0);
								}
								else
								{
									m_game->m_effect_manager->add_effect(EffectType::FOOTPRINT, (m_pivot_x + dX) * 32 + ((rand() % 20) - 10), (m_pivot_y + dY) * 32 + ((rand() % 20) - 10), 0, 0, 0, 0);
									m_game->m_effect_manager->add_effect(EffectType::FOOTPRINT, (m_pivot_x + dX) * 32 + ((rand() % 20) - 10), (m_pivot_y + dY) * 32 + ((rand() % 20) - 10), 0, 0, 0, 0);
									m_game->m_effect_manager->add_effect(EffectType::FOOTPRINT, (m_pivot_x + dX) * 32 + ((rand() % 20) - 10), (m_pivot_y + dY) * 32 + ((rand() % 20) - 10), 0, 0, 0, 0);
									m_game->m_effect_manager->add_effect(EffectType::FOOTPRINT, (m_pivot_x + dX) * 32 + ((rand() % 20) - 10), (m_pivot_y + dY) * 32 + ((rand() % 20) - 10), 0, 0, 0, 0);
									m_game->m_effect_manager->add_effect(EffectType::FOOTPRINT, (m_pivot_x + dX) * 32 + ((rand() % 20) - 10), (m_pivot_y + dY) * 32 + ((rand() % 20) - 10), 0, 0, 0, 0);
								}
								if (true) audio_manager::get().play_game_sound(sound_type::character, 11, dist, lPan);
							}
							else if (m_data[dX][dY].m_animation.m_current_frame == 5) // vu comme case 5
							{
								int16_t wid = m_data[dX][dY].m_appearance.weapon_item_id;
								CItem* wcfg = (wid > 0) ? m_game->get_item_config(wid) : nullptr;
								weapon_type = wcfg ? wcfg->get_weapon_class() : hb::shared::item::weapon_class::none;
								if (weapon_type == hb::shared::item::weapon_class::dagger || weapon_type == hb::shared::item::weapon_class::short_sword)
								{
									audio_manager::get().play_game_sound(sound_type::character, 1, dist, lPan);
								}
								else if (weapon_type == hb::shared::item::weapon_class::long_sword || weapon_type == hb::shared::item::weapon_class::fencing)
								{
									audio_manager::get().play_game_sound(sound_type::character, 2, dist, lPan);
								}
								else if (weapon_type == hb::shared::item::weapon_class::axe || weapon_type == hb::shared::item::weapon_class::hammer || weapon_type == hb::shared::item::weapon_class::wand)
								{
									audio_manager::get().play_game_sound(sound_type::character, 18, dist, lPan);
								}
								else if (weapon_type == hb::shared::item::weapon_class::bow)
								{
									audio_manager::get().play_game_sound(sound_type::character, 3, dist, lPan);
								}
							}
							break;
						}
					}

					if ((m_data[dX][dY].m_animation.m_action == Type::Attack)) { //3 00498685
						switch (m_data[dX][dY].m_owner_type) {
						case hb::shared::owner::IceGolem: // IceGolem
							if (m_data[dX][dY].m_animation.m_current_frame == 2)
							{
								m_game->m_effect_manager->add_effect(EffectType::AURA_EFFECT_1, (m_pivot_x + dX) * 32, (m_pivot_y + dY) * 32, 0, 0, 0, 0);
							}
							break;
						case hb::shared::owner::CT: // void CGame::DrawDruncncity();Crossbow Turret (Heldenian)
							if (m_data[dX][dY].m_animation.m_current_frame == 2)
							{
								m_game->m_effect_manager->add_effect(EffectType::GATE_ROUND, m_pivot_x + m_data[dX][dY].m_v1, m_pivot_y + m_data[dX][dY].m_v2
									, m_pivot_x + m_data[dX][dY].m_v1 + dX, m_pivot_y + m_data[dX][dY].m_v2 + dY, 0, 87);
								//audio_manager::get().play_game_sound(sound_type::effect, 43, dist, lPan); // Son "wouufffff"
							}
							break;
						case hb::shared::owner::AGC: // void CGame::DrawDruncncity();AGT (Heldenian)
							if (m_data[dX][dY].m_animation.m_current_frame == 2)
							{
								m_game->m_effect_manager->add_effect(EffectType::ARROW_FLYING, m_pivot_x + m_data[dX][dY].m_v1, m_pivot_y + m_data[dX][dY].m_v2
									, m_pivot_x + m_data[dX][dY].m_v1 + dX, m_pivot_y + m_data[dX][dY].m_v2 + dY, 0, 89);
								//audio_manager::get().play_game_sound(sound_type::effect, 43, dist, lPan); // Son "wouufffff"
							}
							break;
						case 1:
						case 2:
						case 3:
						case 4:
						case 5:
						case 6: // Humans
							if ((m_data[dX][dY].m_v3 >= 20) && (m_data[dX][dY].m_animation.m_current_frame == 2))
							{
								if (m_game->has_hero_set(m_data[dX][dY].m_appearance, m_data[dX][dY].m_owner_type) == 1) // Warr hero set
								{
									m_game->m_effect_manager->add_effect(EffectType::WAR_HERO_SET, m_pivot_x + dX, m_pivot_y + dY
										, m_pivot_x + dX, m_pivot_y + dY, 0, 1);
								}
								switch (m_data[dX][dY].m_owner_type) {	// Son pour critiques
								case 1:
								case 2:
								case 3:
									if (true) audio_manager::get().play_game_sound(sound_type::character, 23, dist, lPan); // Critical sound
									break;
								case 4:
								case 5:
								case 6:
									if (true) audio_manager::get().play_game_sound(sound_type::character, 24, dist, lPan); // Critical sound
									break;
								}
							}
							if (m_data[dX][dY].m_animation.m_current_frame == 5)
							{
								if (m_data[dX][dY].m_appearance.is_walking) // not Peace mode
								{
									if (m_data[dX][dY].m_v3 != 1) // autre que corp � corp
									{
										m_game->m_effect_manager->add_effect(static_cast<EffectType>(m_data[dX][dY].m_v3), m_pivot_x + dX, m_pivot_y + dY
											, m_pivot_x + dX + m_data[dX][dY].m_v1, m_pivot_y + dY + m_data[dX][dY].m_v2
											, 0, m_data[dX][dY].m_owner_type);
										if (m_data[dX][dY].m_v3 >= 20) audio_manager::get().play_game_sound(sound_type::effect, 43, dist, lPan); // Son "loup�"
									}
									int16_t wid2 = m_data[dX][dY].m_appearance.weapon_item_id;
								if (wid2 == 845) // StormBringer
									{
										m_game->m_effect_manager->add_effect(EffectType::STORM_BLADE, m_pivot_x + dX, m_pivot_y + dY
											, m_pivot_x + dX + m_data[dX][dY].m_v1, m_pivot_y + dY + m_data[dX][dY].m_v2
											, 0, m_data[dX][dY].m_owner_type);
									}
									else
									{
										m_game->m_effect_manager->add_effect(EffectType::GATE_ROUND, m_pivot_x + dX, m_pivot_y + dY
											, m_pivot_x + dX + m_data[dX][dY].m_v1, m_pivot_y + dY + m_data[dX][dY].m_v2
											, 0, m_data[dX][dY].m_owner_type);
									}
								}
								// Weapon Glare from appearance
								if ((((m_data[dX][dY].m_appearance.weapon_glare | m_data[dX][dY].m_appearance.shield_glare) != 0) || (m_data[dX][dY].m_status.gm_mode)) && (!m_data[dX][dY].m_status.invisibility))
								{
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + (rand() % 20 - 10), (m_pivot_y + dY) * 32 - (rand() % 50) - 5, 0, 0, -(rand() % 8), 0);
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + (rand() % 20 - 10), (m_pivot_y + dY) * 32 - (rand() % 50) - 5, 0, 0, -(rand() % 8), 0);
								}
								//Snoopy: Angels
								if (((m_data[dX][dY].m_status.angel_percent) > rand() % 6) // Angel stars
									&& (m_data[dX][dY].m_status.HasAngelType())
									&& (!m_data[dX][dY].m_status.invisibility))
								{
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + (rand() % 15 + 10), (m_pivot_y + dY) * 32 - (rand() % 30) - 50, 0, 0, -(rand() % 8), 0);
								}
							}
							break;

						default:
							if (m_data[dX][dY].m_animation.m_current_frame == 2)
							{
								if (m_data[dX][dY].m_v3 == 2) // Arrow flying...
								{
									m_game->m_effect_manager->add_effect(EffectType::ARROW_FLYING, m_pivot_x + dX, m_pivot_y + dY
										, m_pivot_x + dX + m_data[dX][dY].m_v1
										, m_pivot_y + dY + m_data[dX][dY].m_v2
										, 0, m_data[dX][dY].m_owner_type * 1000);
								}
							}
							break;
						}

						switch (m_data[dX][dY].m_owner_type) {
						case 1:
						case 2:
						case 3:
						case 4:
						case 5:
						case 6:
							if (m_data[dX][dY].m_appearance.is_walking)
							{
								int16_t wid3 = m_data[dX][dY].m_appearance.weapon_item_id;
								CItem* wcfg3 = (wid3 > 0) ? m_game->get_item_config(wid3) : nullptr;
								weapon_type = wcfg3 ? wcfg3->get_weapon_class() : hb::shared::item::weapon_class::none;
								if (weapon_type == hb::shared::item::weapon_class::dagger || weapon_type == hb::shared::item::weapon_class::short_sword)
								{
									if (m_data[dX][dY].m_animation.m_current_frame == 5)
									{
										audio_manager::get().play_game_sound(sound_type::character, 1, dist, lPan);
									}
								}
								else if (weapon_type == hb::shared::item::weapon_class::long_sword || weapon_type == hb::shared::item::weapon_class::fencing)
								{
									if (m_data[dX][dY].m_animation.m_current_frame == 5)
									{
										audio_manager::get().play_game_sound(sound_type::character, 2, dist, lPan);
									}
								}
								else if (weapon_type == hb::shared::item::weapon_class::axe || weapon_type == hb::shared::item::weapon_class::hammer || weapon_type == hb::shared::item::weapon_class::wand)
								{
									if (m_data[dX][dY].m_animation.m_current_frame == 2)
									{
										audio_manager::get().play_game_sound(sound_type::character, 18, dist, lPan);
									}
								}
								else if (weapon_type == hb::shared::item::weapon_class::bow)
								{
									if (m_data[dX][dY].m_animation.m_current_frame == 3)
									{
										audio_manager::get().play_game_sound(sound_type::character, 3, dist, lPan);
									}
								}
							}
							break;

						case hb::shared::owner::ATK: // Snoopy: ATK
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 140, dist, lPan);
							break;

						case hb::shared::owner::MasterElf: // Snoopy: MasterElf
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::character, 8, dist, lPan);
							break;

						case hb::shared::owner::DSK: // Snoopy: DSK
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 145, dist, lPan);
							break;

						case hb::shared::owner::Beholder: // Beholder
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::effect, 46, dist, lPan);
							break;

						case hb::shared::owner::DarkElf: // DE
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
							{
								if (true) audio_manager::get().play_game_sound(sound_type::character, 3, dist, lPan);
							}
							break;

						case hb::shared::owner::TigerWorm: // TW
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
							{
								if (true) audio_manager::get().play_game_sound(sound_type::character, 1, dist, lPan);
							}
							break;

						case hb::shared::owner::Slime: // Slime
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 2, dist, lPan);
							break;

						case hb::shared::owner::Skeleton: // Skell
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 14, dist, lPan);
							break;

						case hb::shared::owner::StoneGolem: // Stone-Golem
						case hb::shared::owner::IceGolem: // ICeGolem
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 34, dist, lPan);
							break;

						case hb::shared::owner::Cyclops: // Cyclops
						case hb::shared::owner::HellClaw: // HC
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 42, dist, lPan);
							break;

						case hb::shared::owner::GodsHandKnight: // GHK
						case hb::shared::owner::GodsHandKnightCK: // GHKABS
						case hb::shared::owner::TempleKnight: // TK
						case hb::shared::owner::Gargoyle: // GG
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
							{
								if (true) audio_manager::get().play_game_sound(sound_type::character, 2, dist, lPan);
							}
							break;

						case hb::shared::owner::OrcMage: // orc
						case hb::shared::owner::Stalker: // SK
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 10, dist, lPan);
							break;

						case hb::shared::owner::GiantAnt: // Ant
						case hb::shared::owner::LightWarBeetle: // LWB
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 30, dist, lPan);
							break;

						case hb::shared::owner::Scorpion: // Scorpion
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 22, dist, lPan);
							break;

						case hb::shared::owner::Zombie: // Zombie
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 18, dist, lPan);
							break;

						case hb::shared::owner::Amphis: // Snake
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 26, dist, lPan);
							break;

						case hb::shared::owner::ClayGolem: // Clay-Golem
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 38, dist, lPan);
							break;

						case hb::shared::owner::Hellhound: // HH
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 6, dist, lPan);
							break;

						case hb::shared::owner::Troll: // Troll
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 47, dist, lPan);
							break;

						case hb::shared::owner::Ogre: // Ogre
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 52, dist, lPan);
							break;

						case hb::shared::owner::Liche: // Liche
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 56, dist, lPan);
							break;

						case hb::shared::owner::Demon: // DD
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 60, dist, lPan);
							break;

						case hb::shared::owner::Unicorn: // Uni
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 64, dist, lPan);
							break;

						case hb::shared::owner::WereWolf: // WW
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
								audio_manager::get().play_game_sound(sound_type::monster, 68, dist, lPan);
							break;

						case hb::shared::owner::Bunny://Rabbit
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 75, dist, lPan);
							break;

						case hb::shared::owner::Cat://Cat
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 76, dist, lPan);
							break;

						case hb::shared::owner::GiantFrog://Giant-Frog
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 77, dist, lPan);
							break;

						case hb::shared::owner::MountainGiant://Mountain Giant
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 88, dist, lPan);
							break;

						case hb::shared::owner::Ettin://Ettin
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 92, dist, lPan);
							break;

						case hb::shared::owner::CannibalPlant://Cannibal Plant
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 96, dist, lPan);
							break;

						case hb::shared::owner::Rudolph://Rudolph
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
							{
								if (true) audio_manager::get().play_game_sound(sound_type::monster, 38, dist, lPan);
							}
							break;

						case hb::shared::owner::DireBoar://DireBoar
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 68, dist, lPan);
							break;

						case hb::shared::owner::Frost://Frost
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
							{
								if (true) audio_manager::get().play_game_sound(sound_type::character, 4, dist, lPan);
							}
							break;

						case hb::shared::owner::MasterOrc: // Snoopy: Master MageOrc
						case hb::shared::owner::Barbarian: // Snoopy: Barbarian
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 78, dist, lPan);
							break;

						case hb::shared::owner::GiantCrayfish: // Snoopy: GiantCrayFish
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 100, dist, lPan);
							break;

						case hb::shared::owner::FireWyvern: // Snoopy: Fire Wyvern
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 107, dist, lPan);
							break;

						case hb::shared::owner::Tentocle: // Snoopy: Tentocle
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 111, dist, lPan);
							break;

						case hb::shared::owner::Abaddon: // Snoopy: Abaddon
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 137, dist, lPan);
							break;

						case hb::shared::owner::ClawTurtle: // Snoopy: Claw-Turtle
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 115, dist, lPan);
							break;

						case hb::shared::owner::Centaur: // Snoopy: Centaurus
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 119, dist, lPan);
							break;

						case hb::shared::owner::GiTree: // Snoopy: Giant-Tree
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 123, dist, lPan);
							break;

						case hb::shared::owner::GiLizard: // Snoopy: GiantLizard
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 127, dist, lPan);
							break;

						case hb::shared::owner::Dragon: // Snoopy: Dragon
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 131, dist, lPan);
							break;

						case hb::shared::owner::Nizie: //Snoopy:  Nizie
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 135, dist, lPan);
							break;

						case hb::shared::owner::Minaus: // Snoopy: Minaus
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 104, dist, lPan);
							break;

						case hb::shared::owner::HBT: // Snoopy: Heavy BattleTank
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 151, dist, lPan);
							break;

						case hb::shared::owner::CT: // Snoopy: Crosbow Turret
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 153, dist, lPan);
							break;

						case hb::shared::owner::AGC: // Snoopy: Cannon Turret
							if ((m_data[dX][dY].m_animation.m_current_frame == 1))
								audio_manager::get().play_game_sound(sound_type::monster, 155, dist, lPan);
							break;

						case hb::shared::owner::Dummy: // Dummy
						case hb::shared::owner::EnergySphere: // Snoopy: EnergySphere
						default:
							if (m_data[dX][dY].m_animation.m_current_frame == 2) {
								if (true) audio_manager::get().play_game_sound(sound_type::character, 2, dist, lPan);
							}
							break;
						}
					}

					if (m_data[dX][dY].m_animation.m_action == Type::Damage)  // 6  00499159
					{
						switch (m_data[dX][dY].m_owner_type) {
						case 1:
						case 2:
						case 3:  // Men
						case hb::shared::owner::GodsHandKnight: // GHK
						case hb::shared::owner::GodsHandKnightCK: // GHKABS
						case hb::shared::owner::TempleKnight: // TK
							if (m_data[dX][dY].m_animation.m_current_frame == 4)
							{
								if (m_data[dX][dY].m_v2 == -1)
									sound_index = 5;
								else if (m_data[dX][dY].m_v2 == 0)
									sound_index = 5;
								else if ((m_data[dX][dY].m_v2 >= 1) && (m_data[dX][dY].m_v2 <= 19))
									sound_index = 6;
								else if ((m_data[dX][dY].m_v2 >= 20) && (m_data[dX][dY].m_v2 <= 39))
									sound_index = 6;
								else if ((m_data[dX][dY].m_v2 >= 40) && (m_data[dX][dY].m_v2 <= 59))
									sound_index = 7;
								else sound_index = 5;
								if (true) audio_manager::get().play_game_sound(sound_type::character, sound_index, dist, lPan);
								m_game->m_effect_manager->add_effect(EffectType::NORMAL_HIT, m_pivot_x + dX, m_pivot_y + dY, 0, 0, 0, 4);
							}
							if (m_data[dX][dY].m_animation.m_current_frame == 5)
							{
								if (true) audio_manager::get().play_game_sound(sound_type::character, 12, dist, lPan);
							}
							break;
						case 4:
						case 5:
						case 6: // Women
							if (m_data[dX][dY].m_animation.m_current_frame == 4)
							{
								if (m_data[dX][dY].m_v2 == -1)
									sound_index = 5;
								else if (m_data[dX][dY].m_v2 == 0)
									sound_index = 5;
								else if ((m_data[dX][dY].m_v2 >= 1) && (m_data[dX][dY].m_v2 <= 19))
									sound_index = 6;
								else if ((m_data[dX][dY].m_v2 >= 20) && (m_data[dX][dY].m_v2 <= 39))
									sound_index = 6;
								else if ((m_data[dX][dY].m_v2 >= 40) && (m_data[dX][dY].m_v2 <= 59))
									sound_index = 7;
								else sound_index = 5;
								if (true) audio_manager::get().play_game_sound(sound_type::character, sound_index, dist, lPan);
								m_game->m_effect_manager->add_effect(EffectType::NORMAL_HIT, m_pivot_x + dX, m_pivot_y + dY, 0, 0, 0, 4);
							}
							if (m_data[dX][dY].m_animation.m_current_frame == 5)
							{
								if (true) audio_manager::get().play_game_sound(sound_type::character, 13, dist, lPan);
							}
							break;

						default:
							if (m_data[dX][dY].m_animation.m_current_frame == 4)
							{
								if (m_data[dX][dY].m_v2 == -1)
									sound_index = 5;  // Hand Attack
								else if (m_data[dX][dY].m_v2 == 0)
									sound_index = 5;  // Hand Attack
								else if ((m_data[dX][dY].m_v2 >= 1) && (m_data[dX][dY].m_v2 <= 19))
									sound_index = 6;  // Blade hit
								else if ((m_data[dX][dY].m_v2 >= 20) && (m_data[dX][dY].m_v2 <= 39))
									sound_index = 6;  // Blade hit
								else if ((m_data[dX][dY].m_v2 >= 40) && (m_data[dX][dY].m_v2 <= 59))
									sound_index = 7; // Arrow hit
								else sound_index = 5;

								if (true) audio_manager::get().play_game_sound(sound_type::character, sound_index, dist, lPan);
								if (sound_index == 7) // Change the effect for Arrows hitting (no more at fixed heigh with arrow flying but on damage)
								{
									m_game->m_effect_manager->add_effect(EffectType::FOOTPRINT, (m_pivot_x + dX) * 32, (m_pivot_y + dY) * 32, 0, 0, 0, m_data[dX][dY].m_owner_type);
								}
								else
								{
									m_game->m_effect_manager->add_effect(EffectType::NORMAL_HIT, m_pivot_x + dX, m_pivot_y + dY, 0, 0, 0, 4);
								}
							}

							switch (m_data[dX][dY].m_owner_type) {
							case hb::shared::owner::Barbarian: // Snoopy: Barbarian
								if (m_data[dX][dY].m_animation.m_current_frame == 1 && true) audio_manager::get().play_game_sound(sound_type::monster, 144, dist, lPan);
								break;

							case hb::shared::owner::ATK: // Snoopy: ATK
								if (m_data[dX][dY].m_animation.m_current_frame == 1 && true) audio_manager::get().play_game_sound(sound_type::monster, 143, dist, lPan);
								break;

							case hb::shared::owner::MasterElf: // Snoopy: MasterElf
								if (m_data[dX][dY].m_animation.m_current_frame == 1) audio_manager::get().play_game_sound(sound_type::character, 7, dist, lPan);
								break;

							case hb::shared::owner::DSK: // Snoopy: DSK
								if (m_data[dX][dY].m_animation.m_current_frame == 1) audio_manager::get().play_game_sound(sound_type::monster, 148, dist, lPan);
								break;

							case hb::shared::owner::DarkElf: // DE
								if (m_data[dX][dY].m_animation.m_current_frame == 5 && true) audio_manager::get().play_game_sound(sound_type::character, 13, dist, lPan);
								break;

							case hb::shared::owner::Slime: // Slime
							case hb::shared::owner::Beholder: // BB
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 3, dist, lPan);
								break;

							case hb::shared::owner::Skeleton: // Skell
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 15, dist, lPan);
								break;

							case hb::shared::owner::StoneGolem: // Stone-Golem
							case hb::shared::owner::IceGolem: // IceGolem
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 35, dist, lPan);
								break;

							case hb::shared::owner::Cyclops: // Cyclops
							case hb::shared::owner::HellClaw: // HC
							case hb::shared::owner::Gargoyle: // GG
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 43, dist, lPan);
								break;

							case hb::shared::owner::OrcMage: // Orc
							case hb::shared::owner::Stalker: // SK
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 11, dist, lPan);
								break;

							case hb::shared::owner::GiantAnt: // Ant
							case hb::shared::owner::LightWarBeetle: // LWB
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 31, dist, lPan);
								break;

							case hb::shared::owner::Scorpion: // Scorp
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 23, dist, lPan);
								break;

							case hb::shared::owner::Zombie: // Zombie
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 19, dist, lPan);
								break;

							case hb::shared::owner::Amphis: // Snake
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 27, dist, lPan);
								break;

							case hb::shared::owner::ClayGolem: // Clay-Golem
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 39, dist, lPan);
								break;

							case hb::shared::owner::Hellhound: // HH
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 7, dist, lPan);
								break;

							case hb::shared::owner::Troll: // Troll
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 48, dist, lPan);
								break;

							case hb::shared::owner::Ogre: // Ogre
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 53, dist, lPan);
								break;

							case hb::shared::owner::Liche: // Liche
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 57, dist, lPan);
								break;

							case hb::shared::owner::Demon: // DD
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 61, dist, lPan);
								break;

							case hb::shared::owner::Unicorn: // Uni
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 65, dist, lPan);
								break;

							case hb::shared::owner::WereWolf: // WW
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 69, dist, lPan);
								break;

							case hb::shared::owner::Dummy: // dummy
							case hb::shared::owner::EnergySphere: // Snoopy: EnergyBall
								if (m_data[dX][dY].m_animation.m_current_frame == 5) audio_manager::get().play_game_sound(sound_type::monster, 2, dist, lPan);
								break;

							case hb::shared::owner::Bunny://Rabbit
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 79, dist, lPan);
								break;

							case hb::shared::owner::Cat://Cat
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 80, dist, lPan);
								break;

							case hb::shared::owner::GiantFrog://Giant-Frog
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 81, dist, lPan);
								break;

							case hb::shared::owner::MountainGiant: // Mountain Giant
							case hb::shared::owner::MasterOrc: // Snoopy: MasterMageOrc
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 89, dist, lPan);
								break;

							case hb::shared::owner::Ettin://Ettin
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 93, dist, lPan);
								break;
							case hb::shared::owner::CannibalPlant://Cannabl Plant
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 97, dist, lPan);
								break;
							case hb::shared::owner::Rudolph://Rudolph
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 69, dist, lPan);
								break;
							case hb::shared::owner::DireBoar://DireBoar
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 78, dist, lPan);
								break;
							case hb::shared::owner::Frost://Frost
								if (m_data[dX][dY].m_animation.m_current_frame == 1) audio_manager::get().play_game_sound(sound_type::character, 13, dist, lPan);
								break;

							case hb::shared::owner::GiantCrayfish: // Snoopy: Giant CrayFish
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 101, dist, lPan);
								break;

							case hb::shared::owner::Minaus: // Snoopy: Minaus
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 102, dist, lPan);
								break;

							case hb::shared::owner::Tentocle: // Snoopy: Tentocle
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 108, dist, lPan);
								break;

							case hb::shared::owner::Abaddon: // Snoopy: Abaddon
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 138, dist, lPan);
								break;

							case hb::shared::owner::ClawTurtle: // Snoopy: ClawTurtle
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 112, dist, lPan);
								break;

							case hb::shared::owner::Centaur: // Snoopy: Centaurus
							case hb::shared::owner::Sorceress: // Snoopy: Sorceress
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 116, dist, lPan);
								break;

							case hb::shared::owner::GiTree: // Snoopy: GiantTree
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 120, dist, lPan);
								break;

							case hb::shared::owner::GiLizard: // Snoopy: GiantLizard
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 124, dist, lPan);
								break;

							case hb::shared::owner::Dragon: // Snoopy: Dragon
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 128, dist, lPan);
								break;

							case hb::shared::owner::Nizie: // Snoopy: Nizie
								if ((m_data[dX][dY].m_animation.m_current_frame == 1)) audio_manager::get().play_game_sound(sound_type::monster, 132, dist, lPan);
								break;
							}
							break;
						}
					}

					if (m_data[dX][dY].m_animation.m_action == Type::DamageMove) { // 7 004997BD
						switch (m_data[dX][dY].m_owner_type) {
						case 1:
						case 2:
						case 3:
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
							{
								if (m_data[dX][dY].m_v2 == -1)
									sound_index = 5;
								else if (m_data[dX][dY].m_v2 == 0)
									sound_index = 5;
								else if ((m_data[dX][dY].m_v2 >= 1) && (m_data[dX][dY].m_v2 <= 19))
									sound_index = 6;
								else if ((m_data[dX][dY].m_v2 >= 20) && (m_data[dX][dY].m_v2 <= 39))
									sound_index = 6;
								else if ((m_data[dX][dY].m_v2 >= 40) && (m_data[dX][dY].m_v2 <= 59))
									sound_index = 7;
								else sound_index = 5;

								if (true) audio_manager::get().play_game_sound(sound_type::character, sound_index, dist, lPan);
								m_game->m_effect_manager->add_effect(EffectType::NORMAL_HIT, m_pivot_x + dX, m_pivot_y + dY, 0, 0, 0, 4);
							}
							if (m_data[dX][dY].m_animation.m_current_frame == 2)
							{
								if (true) audio_manager::get().play_game_sound(sound_type::character, 12, dist, lPan);
							}
							break;

						case 4:
						case 5:
						case 6:
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
							{
								if (m_data[dX][dY].m_v2 == -1)
									sound_index = 5;
								else if (m_data[dX][dY].m_v2 == 0)
									sound_index = 5;
								else if ((m_data[dX][dY].m_v2 >= 1) && (m_data[dX][dY].m_v2 <= 19))
									sound_index = 6;
								else if ((m_data[dX][dY].m_v2 >= 20) && (m_data[dX][dY].m_v2 <= 39))
									sound_index = 6;
								else if ((m_data[dX][dY].m_v2 >= 40) && (m_data[dX][dY].m_v2 <= 59))
									sound_index = 7;
								else sound_index = 5;
								if (true) audio_manager::get().play_game_sound(sound_type::character, sound_index, dist, lPan);
								m_game->m_effect_manager->add_effect(EffectType::NORMAL_HIT, m_pivot_x + dX, m_pivot_y + dY, 0, 0, 0, 4);
							}
							if (m_data[dX][dY].m_animation.m_current_frame == 2)
							{
								if (true) audio_manager::get().play_game_sound(sound_type::character, 13, dist, lPan);
							}
							break;

						default:
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
							{
								if (m_data[dX][dY].m_v2 == -1)
									sound_index = 5;
								else if (m_data[dX][dY].m_v2 == 0)
									sound_index = 5;
								else if ((m_data[dX][dY].m_v2 >= 1) && (m_data[dX][dY].m_v2 <= 19))
									sound_index = 6;
								else if ((m_data[dX][dY].m_v2 >= 20) && (m_data[dX][dY].m_v2 <= 39))
									sound_index = 6;
								else if ((m_data[dX][dY].m_v2 >= 40) && (m_data[dX][dY].m_v2 <= 59))
									sound_index = 7;
								else sound_index = 5;
								if (true) audio_manager::get().play_game_sound(sound_type::character, sound_index, dist, lPan);
								m_game->m_effect_manager->add_effect(EffectType::NORMAL_HIT, m_pivot_x + dX, m_pivot_y + dY, 0, 0, 0, 4);
							}

							switch (m_data[dX][dY].m_owner_type) {
							case hb::shared::owner::ATK: //Snoopy:  ATK
								if (m_data[dX][dY].m_animation.m_current_frame == 1)
									audio_manager::get().play_game_sound(sound_type::monster, 143, dist, lPan);
								break;
							case hb::shared::owner::MasterElf: // Snoopy: MasterElf
								if (m_data[dX][dY].m_animation.m_current_frame == 1)
									audio_manager::get().play_game_sound(sound_type::character, 7, dist, lPan);
								break;
							case hb::shared::owner::Barbarian: // Snoopy: Barbarian
								if (m_data[dX][dY].m_animation.m_current_frame == 1)
									audio_manager::get().play_game_sound(sound_type::monster, 144, dist, lPan);
								break;
							case hb::shared::owner::DSK: // Snoopy: DSK
								if (m_data[dX][dY].m_animation.m_current_frame == 1)
									audio_manager::get().play_game_sound(sound_type::monster, 148, dist, lPan);
								break;

							case hb::shared::owner::Slime: // Slime
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 3, dist, lPan);
								break;

							case hb::shared::owner::Skeleton: // Skell
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 15, dist, lPan);
								break;

							case hb::shared::owner::StoneGolem: // Stone Golem
							case hb::shared::owner::IceGolem: // IceGolem
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 35, dist, lPan);
								break;

							case hb::shared::owner::Cyclops: // Cyclops
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 43, dist, lPan);
								break;

							case hb::shared::owner::OrcMage: // Orc
							case hb::shared::owner::Stalker: // SK
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 11, dist, lPan);
								break;

							case hb::shared::owner::GiantAnt: // Ant
							case hb::shared::owner::LightWarBeetle: // LWB
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 31, dist, lPan);
								break;

							case hb::shared::owner::Scorpion: // Scorpion
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 23, dist, lPan);
								break;

							case hb::shared::owner::Zombie: // Zombie
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 19, dist, lPan);
								break;

							case hb::shared::owner::Amphis: // Snake
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 27, dist, lPan);
								break;

							case hb::shared::owner::ClayGolem: // Clay-Golem
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 39, dist, lPan);
								break;

							case hb::shared::owner::Hellhound: // HH
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 7, dist, lPan);
								break;

							case hb::shared::owner::Troll: // Troll
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 48, dist, lPan);
								break;

							case hb::shared::owner::Ogre: // Ogre
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 53, dist, lPan);
								break;

							case hb::shared::owner::Liche: // Liche
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 57, dist, lPan);
								break;

							case hb::shared::owner::Demon: // DD
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 61, dist, lPan);
								break;

							case hb::shared::owner::Unicorn: // Uni
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 65, dist, lPan);
								break;

							case hb::shared::owner::WereWolf: // WW
								if (m_data[dX][dY].m_animation.m_current_frame == 2)
									audio_manager::get().play_game_sound(sound_type::monster, 69, dist, lPan);
								break;
							case hb::shared::owner::Bunny://Rabbit
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 79, dist, lPan);
								break;

							case hb::shared::owner::Cat://Cat
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 80, dist, lPan);
								break;

							case hb::shared::owner::GiantFrog://Giant-Frog
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 81, dist, lPan);
								break;

							case hb::shared::owner::MountainGiant://Mountain Giant
							case hb::shared::owner::MasterOrc: // Snoopy: MasterMageOrc
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 89, dist, lPan);
								break;

							case hb::shared::owner::Ettin://Ettin
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 93, dist, lPan);
								break;

							case hb::shared::owner::CannibalPlant://Cannibal Plant
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 97, dist, lPan);
								break;

							case hb::shared::owner::Rudolph://Rudolph
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 69, dist, lPan);
								break;
							case hb::shared::owner::DireBoar://DireBoar
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 78, dist, lPan);
								break;

							case hb::shared::owner::GiantCrayfish: //Snoopy:  GiantCrayFish
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 101, dist, lPan);
								break;

							case hb::shared::owner::Minaus: // Snoopy: Minos
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 101, dist, lPan);
								break;

							case hb::shared::owner::Tentocle: // Snoopy: Tentocle
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 108, dist, lPan);
								break;

							case hb::shared::owner::Abaddon: // Snoopy: Abaddon
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 138, dist, lPan);
								break;

							case hb::shared::owner::ClawTurtle: // Snoopy: ClawTurtle
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 112, dist, lPan);
								break;

							case hb::shared::owner::Centaur: // Snoopy: Centaurus
							case hb::shared::owner::Sorceress: // Snoopy: Sorceress
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 116, dist, lPan);
								break;

							case hb::shared::owner::GiTree: // Snoopy: GiantTree
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 120, dist, lPan);
								break;

							case hb::shared::owner::GiLizard: // Snoopy: GiantLizard
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 124, dist, lPan);
								break;

							case hb::shared::owner::Dragon: // Snoopy: Dragon
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 128, dist, lPan);
								break;

							case hb::shared::owner::Nizie: // Snoopy: Nizie
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 132, dist, lPan);
								break;

							default:
								break;
							}
							break;
						}
					}

					if (m_data[dX][dY].m_animation.m_action == Type::Magic)  // 4 00499D51
					{
						switch (m_data[dX][dY].m_owner_type) {
						case 1:
						case 2:
						case 3:
						case 4:
						case 5:
						case 6:
							if (m_data[dX][dY].m_animation.m_current_frame == 1)
							{
								if (true) audio_manager::get().play_game_sound(sound_type::character, 16, dist, lPan);
								if ((((m_data[dX][dY].m_appearance.weapon_glare | m_data[dX][dY].m_appearance.shield_glare) != 0) || (m_data[dX][dY].m_status.gm_mode)) && (!m_data[dX][dY].m_status.invisibility))
								{
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + (rand() % 20 - 10), (m_pivot_y + dY) * 32 - (rand() % 50) - 5, 0, 0, -(rand() % 8), 0);
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + (rand() % 20 - 10), (m_pivot_y + dY) * 32 - (rand() % 50) - 5, 0, 0, -(rand() % 8), 0);
								}
								//Snoopy: Angels
								if (((m_data[dX][dY].m_status.angel_percent) > rand() % 6) // Angel stars
									&& (m_data[dX][dY].m_status.HasAngelType())
									&& (!m_data[dX][dY].m_status.invisibility))
								{
									m_game->m_effect_manager->add_effect(EffectType::STAR_TWINKLE, (m_pivot_x + dX) * 32 + (rand() % 15 + 10), (m_pivot_y + dY) * 32 - (rand() % 30) - 50, 0, 0, -(rand() % 8), 0);
								}
								if (m_game->has_hero_set(m_data[dX][dY].m_appearance, m_data[dX][dY].m_owner_type) == 2) // Mage hero set
								{
									m_game->m_effect_manager->add_effect(EffectType::MAGE_HERO_SET, m_pivot_x + dX, m_pivot_y + dY
										, m_pivot_x + dX, m_pivot_y + dY, 0, 1);
								}
								//if (m_data[dX][dY].m_v1 >= 70) // effet gros sorts autour du caster
								//	m_game->m_effect_manager->add_effect(EffectType::BUFF_EFFECT_LIGHT, (m_pivot_x + dX) * 32, (m_pivot_y + dY) * 32, 0, 0, 0, 0);
								if (m_data[dX][dY].m_v1 == 82) // lumi�re si MassMagicMissile autour du caster
								{
									m_game->m_effect_manager->add_effect(EffectType::MASS_MM_AURA_CASTER, (m_pivot_x + dX) * 32, (m_pivot_y + dY) * 32, 0, 0, 0, 0);
								}
							}
							break;
						}
					}

					if (m_data[dX][dY].m_animation.m_action == Type::Dying)  // 10 // 00499F5D
					{
						switch (m_data[dX][dY].m_owner_type) {
						case 1:
						case 2:
						case 3:
						case hb::shared::owner::GodsHandKnight: // GHK
						case hb::shared::owner::GodsHandKnightCK: // GHKABS
						case hb::shared::owner::TempleKnight: // TK
							if (m_data[dX][dY].m_animation.m_current_frame == 6)
							{
								if (m_data[dX][dY].m_v2 == -1)
									sound_index = 5;
								else if (m_data[dX][dY].m_v2 == 0)
									sound_index = 5;
								else if ((m_data[dX][dY].m_v2 >= 1) && (m_data[dX][dY].m_v2 <= 19))
									sound_index = 6;
								else if ((m_data[dX][dY].m_v2 >= 20) && (m_data[dX][dY].m_v2 <= 39))
									sound_index = 6;
								else if ((m_data[dX][dY].m_v2 >= 40) && (m_data[dX][dY].m_v2 <= 59))
									sound_index = 7;
								else sound_index = 5;
								if (true) audio_manager::get().play_game_sound(sound_type::character, sound_index, dist, lPan);
								m_game->m_effect_manager->add_effect(EffectType::NORMAL_HIT, m_pivot_x + dX, m_pivot_y + dY, 0, 0, 0, 12);
							}
							if (m_data[dX][dY].m_animation.m_current_frame == 7)
							{
								if (true) audio_manager::get().play_game_sound(sound_type::character, 14, dist, lPan);
							}
							break;

						case 4:
						case 5:
						case 6:
						case hb::shared::owner::DarkElf: // DE
							if (m_data[dX][dY].m_animation.m_current_frame == 6)
							{
								if (m_data[dX][dY].m_v2 == -1)
									sound_index = 5;
								else if (m_data[dX][dY].m_v2 == 0)
									sound_index = 5;
								else if ((m_data[dX][dY].m_v2 >= 1) && (m_data[dX][dY].m_v2 <= 19))
									sound_index = 6;
								else if ((m_data[dX][dY].m_v2 >= 20) && (m_data[dX][dY].m_v2 <= 39))
									sound_index = 6;
								else if ((m_data[dX][dY].m_v2 >= 40) && (m_data[dX][dY].m_v2 <= 59))
									sound_index = 7;
								else sound_index = 5;
								if (true) audio_manager::get().play_game_sound(sound_type::character, sound_index, dist, lPan);
								m_game->m_effect_manager->add_effect(EffectType::NORMAL_HIT, m_pivot_x + dX, m_pivot_y + dY, 0, 0, 0, 12);
							}
							if (m_data[dX][dY].m_animation.m_current_frame == 7)
							{
								if (true) audio_manager::get().play_game_sound(sound_type::character, 15, dist, lPan);
							}
							break;

						default:
							if (m_data[dX][dY].m_animation.m_current_frame == 4)
							{
								if (m_data[dX][dY].m_v2 == -1)
									sound_index = 5;
								else if (m_data[dX][dY].m_v2 == 0)
									sound_index = 5;
								else if ((m_data[dX][dY].m_v2 >= 1) && (m_data[dX][dY].m_v2 <= 19))
									sound_index = 6;
								else if ((m_data[dX][dY].m_v2 >= 20) && (m_data[dX][dY].m_v2 <= 39))
									sound_index = 6;
								else if ((m_data[dX][dY].m_v2 >= 40) && (m_data[dX][dY].m_v2 <= 59))
									sound_index = 7;
								else sound_index = 5;
								if (true) audio_manager::get().play_game_sound(sound_type::character, sound_index, dist, lPan);
								m_game->m_effect_manager->add_effect(EffectType::NORMAL_HIT, m_pivot_x + dX, m_pivot_y + dY, 0, 0, 0, 12);
							}

							switch (m_data[dX][dY].m_owner_type) {
							case hb::shared::owner::Beholder: // BB
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 39, dist, lPan);
								break;

							case hb::shared::owner::Slime: // Slime
							case hb::shared::owner::Dummy: // Dummy
							case hb::shared::owner::EnergySphere: // Snoopy: EnergyBall
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 4, dist, lPan);
								break;

							case hb::shared::owner::Skeleton: // Skell
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 16, dist, lPan);
								break;

							case hb::shared::owner::StoneGolem: // Stone-Golem
							case hb::shared::owner::BattleGolem: // BG
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 36, dist, lPan);
								break;

							case hb::shared::owner::IceGolem: // IceGolem
								if (m_data[dX][dY].m_animation.m_current_frame == 5) {
									m_game->m_effect_manager->add_effect(EffectType::AURA_EFFECT_2, (m_pivot_x + dX) * 32, (m_pivot_y + dY) * 32, 0, 0, 0);
									audio_manager::get().play_game_sound(sound_type::monster, 36, dist, lPan);
								}
								break;

							case hb::shared::owner::Cyclops: // Cyclops
							case hb::shared::owner::HellClaw: // HC
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 44, dist, lPan);
								break;

							case hb::shared::owner::OrcMage: // Orc
							case hb::shared::owner::Stalker: // SK
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 12, dist, lPan);
								break;

							case hb::shared::owner::GiantAnt: // Ant
							case hb::shared::owner::LightWarBeetle: // LWB
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 32, dist, lPan);
								break;

							case hb::shared::owner::Scorpion: // Scorp
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 24, dist, lPan);
								break;

							case hb::shared::owner::Zombie: // Zombie
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 20, dist, lPan);
								break;

							case hb::shared::owner::Amphis: // Snake
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 28, dist, lPan);
								break;

							case hb::shared::owner::ClayGolem: // Clay-Golem
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 40, dist, lPan);
								break;

							case hb::shared::owner::Hellhound: // HH
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 8, dist, lPan);
								break;

							case hb::shared::owner::Troll: // Troll
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 49, dist, lPan);
								break;

							case hb::shared::owner::Ogre: // Ogre
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 54, dist, lPan);
								break;

							case hb::shared::owner::Liche: // Liche
							case hb::shared::owner::TigerWorm: // TW
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 58, dist, lPan);
								break;

							case hb::shared::owner::Demon: // DD
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 62, dist, lPan);
								break;

							case hb::shared::owner::Unicorn: // Uni
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 66, dist, lPan);
								break;

							case hb::shared::owner::WereWolf: // WW
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::monster, 70, dist, lPan);
								break;

							case hb::shared::owner::ArrowGuardTower: // AGT
							case hb::shared::owner::CannonGuardTower: // CGT
							case hb::shared::owner::ManaCollector: // MS
							case hb::shared::owner::Detector: // DT
							case hb::shared::owner::EnergyShield: // ESG
							case hb::shared::owner::GrandMagicGenerator: // GMG
							case hb::shared::owner::ManaStone: // ManaStone
								if (m_data[dX][dY].m_animation.m_current_frame == 3)
								{
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_EXPLOSION, (m_pivot_x + dX) * 32, (m_pivot_y + dY) * 32, 0, 0, 0, 0);
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_CASTING, (m_pivot_x + dX) * 32 + 30 - (rand() % 60), (m_pivot_y + dY) * 32 + 30 - (rand() % 60), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_CASTING, (m_pivot_x + dX) * 32 + 30 - (rand() % 60), (m_pivot_y + dY) * 32 + 30 - (rand() % 60), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_CASTING, (m_pivot_x + dX) * 32 + 30 - (rand() % 60), (m_pivot_y + dY) * 32 + 30 - (rand() % 60), 0, 0, -1 * (rand() % 2));
								}
								break;

							case hb::shared::owner::CT: // Snoopy: CrossBowTurret
								if (m_data[dX][dY].m_animation.m_current_frame == 3)
								{
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_EXPLOSION, (m_pivot_x + dX) * 32, (m_pivot_y + dY) * 32, 0, 0, 0, 0);
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_CASTING, (m_pivot_x + dX) * 32 + 30 - (rand() % 60), (m_pivot_y + dY) * 32 + 30 - (rand() % 60), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_CASTING, (m_pivot_x + dX) * 32 + 30 - (rand() % 60), (m_pivot_y + dY) * 32 + 30 - (rand() % 60), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_CASTING, (m_pivot_x + dX) * 32 + 30 - (rand() % 60), (m_pivot_y + dY) * 32 + 30 - (rand() % 60), 0, 0, -1 * (rand() % 2));
								}
								if (m_data[dX][dY].m_animation.m_current_frame == 1)
									audio_manager::get().play_game_sound(sound_type::monster, 154, dist, lPan);
								break;

							case hb::shared::owner::AGC: // Snoopy: CannonTurret
								if (m_data[dX][dY].m_animation.m_current_frame == 3)
								{
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_EXPLOSION, (m_pivot_x + dX) * 32, (m_pivot_y + dY) * 32, 0, 0, 0, 0);
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_CASTING, (m_pivot_x + dX) * 32 + 30 - (rand() % 60), (m_pivot_y + dY) * 32 + 30 - (rand() % 60), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_CASTING, (m_pivot_x + dX) * 32 + 30 - (rand() % 60), (m_pivot_y + dY) * 32 + 30 - (rand() % 60), 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_CASTING, (m_pivot_x + dX) * 32 + 30 - (rand() % 60), (m_pivot_y + dY) * 32 + 30 - (rand() % 60), 0, 0, -1 * (rand() % 2));
								}
								if (m_data[dX][dY].m_animation.m_current_frame == 1)
									audio_manager::get().play_game_sound(sound_type::monster, 156, dist, lPan);
								break;

							case hb::shared::owner::Catapult: // CP
								if (m_data[dX][dY].m_animation.m_current_frame == 1)
								{
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_EXPLOSION, (m_pivot_x + dX) * 32, (m_pivot_y + dY) * 32 - 30, 0, 0, 0, 0);
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10) - 30, 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10) - 30, 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10) - 30, 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10) - 30, 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10) - 30, 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_CASTING, (m_pivot_x + dX) * 32 + 30 - (rand() % 60), (m_pivot_y + dY) * 32 + 30 - (rand() % 60) - 30, 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_CASTING, (m_pivot_x + dX) * 32 + 30 - (rand() % 60), (m_pivot_y + dY) * 32 + 30 - (rand() % 60) - 30, 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_CASTING, (m_pivot_x + dX) * 32 + 30 - (rand() % 60), (m_pivot_y + dY) * 32 + 30 - (rand() % 60) - 30, 0, 0, -1 * (rand() % 2));
								}
								break;

							case hb::shared::owner::Gargoyle: // GG
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
								{
									audio_manager::get().play_game_sound(sound_type::monster, 44, dist, lPan);
								}
								if (m_data[dX][dY].m_animation.m_current_frame == 11)
								{
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_EXPLOSION, (m_pivot_x + dX) * 32, (m_pivot_y + dY) * 32 - 30, 0, 0, 0, 0);
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10) - 30, 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10) - 30, 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10) - 30, 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10) - 30, 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::BURST_LARGE, (m_pivot_x + dX) * 32 + 5 - (rand() % 10), (m_pivot_y + dY) * 32 + 5 - (rand() % 10) - 30, 0, 0, -1 * (rand() % 2));

									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_CASTING, (m_pivot_x + dX) * 32 + 30 - (rand() % 60), (m_pivot_y + dY) * 32 + 30 - (rand() % 60) - 30, 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_CASTING, (m_pivot_x + dX) * 32 + 30 - (rand() % 60), (m_pivot_y + dY) * 32 + 30 - (rand() % 60) - 30, 0, 0, -1 * (rand() % 2));
									m_game->m_effect_manager->add_effect(EffectType::MS_CRUSADE_CASTING, (m_pivot_x + dX) * 32 + 30 - (rand() % 60), (m_pivot_y + dY) * 32 + 30 - (rand() % 60) - 30, 0, 0, -1 * (rand() % 2));
								}
								break;

							case hb::shared::owner::Bunny:// Rabbit
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 83, dist, lPan);
								break;

							case hb::shared::owner::Cat: // Cat
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 84, dist, lPan);
								break;

							case hb::shared::owner::GiantFrog://Giant-Frog
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 85, dist, lPan);
								break;

							case hb::shared::owner::MountainGiant://Mountain Giant
							case hb::shared::owner::MasterOrc: // Snoopy: MasterMageOrc
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 90, dist, lPan);
								break;

							case hb::shared::owner::Ettin://Ettin
							case hb::shared::owner::Barbarian: // Snoopy: Barbarian
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 94, dist, lPan);
								break;

							case hb::shared::owner::ATK: // Snoopy: ATK
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 141, dist, lPan);
								break;

							case hb::shared::owner::DSK: // Snoopy: DSK
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 146, dist, lPan);
								break;

							case hb::shared::owner::Rudolph://Rudolph
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 65, dist, lPan);
								break;

							case hb::shared::owner::DireBoar://DireBoar
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 94, dist, lPan);
								break;

							case hb::shared::owner::Wyvern: // Wyvern
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::effect, 7, dist, lPan);
								break;

							case hb::shared::owner::Dragon: // Snoopy: Dragon
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 129, dist, lPan);
								break;

							case hb::shared::owner::Centaur: // Snoopy: Centaur
							case hb::shared::owner::Sorceress: // Snoopy: Sorceress
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 129, dist, lPan);
								break;

							case hb::shared::owner::ClawTurtle: // Snoopy: ClawTurtle
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 113, dist, lPan);
								break;

							case hb::shared::owner::FireWyvern: // Snoopy: FireWyvern
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 105, dist, lPan);
								break;


							case hb::shared::owner::CannibalPlant: // Cannibal Plant
							case hb::shared::owner::GiantCrayfish: // Snoopy: GiantGrayFish
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 98, dist, lPan);
								break;

							case hb::shared::owner::GiLizard: //Snoopy:
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 125, dist, lPan);
								break;

							case hb::shared::owner::GiTree: // Snoopy:
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 121, dist, lPan);
								break;

							case hb::shared::owner::Minaus: // Snoopy:
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 103, dist, lPan);
								break;

							case hb::shared::owner::Nizie: // Snoopy:
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 133, dist, lPan);
								break;

							case hb::shared::owner::Tentocle: //Snoopy: Tentocle
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 109, dist, lPan);
								break;

							case hb::shared::owner::Abaddon: // Snoopy: Abaddon
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 139, dist, lPan);
								break;

							case hb::shared::owner::MasterElf: // Snoopy: MasterElf
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 150, dist, lPan);
								break;

							case hb::shared::owner::HBT: // Snoopy: HBT
								if ((m_data[dX][dY].m_animation.m_current_frame == 1))
									audio_manager::get().play_game_sound(sound_type::monster, 152, dist, lPan);
								break;

							default:
								if (m_data[dX][dY].m_animation.m_current_frame == 5)
									audio_manager::get().play_game_sound(sound_type::character, 15, dist, lPan);
								break;

							case hb::shared::owner::Frost: // Frost
							case hb::shared::owner::Gate: // Snoopy: Gate
								break;
							}
							break;
						}
					}
				}
			}
		}
	if (auto_update)
	{
		S_dwUpdateTime = time;
		if (ret == 0)
			return -1;
	}
	if (dynObjsNeedUpdate) m_dynamic_object_frame_time = time; //v1.4
	return ret;
}


bool CMapData::set_item(short sX, short sY, short item_id, const hb::shared::item::item_instance_data& data, bool drop_effect)
{
	int dX, dY;
	int abs_x, abs_y, dist;
	if ((sX < m_pivot_x) || (sX >= m_pivot_x + MapDataSizeX) ||
		(sY < m_pivot_y) || (sY >= m_pivot_y + MapDataSizeY))
	{
		return false;
	}

	dX = sX - m_pivot_x;
	dY = sY - m_pivot_y;

	m_data[dX][dY].m_item_id = item_id;
	m_data[dX][dY].m_item = data;

	abs_x = abs(((m_game->m_Camera.get_x() / 32) + VIEW_CENTER_TILE_X()) - sX);
	abs_y = abs(((m_game->m_Camera.get_y() / 32) + VIEW_CENTER_TILE_Y()) - sY);

	if (abs_x > abs_y) dist = abs_x;
	else dist = abs_y;

	if (item_id != 0)
	{
		if (drop_effect == true)
		{
			audio_manager::get().play_game_sound(sound_type::effect, 11, dist);
			m_game->m_effect_manager->add_effect(EffectType::FOOTPRINT, (m_pivot_x + dX) * 32, (m_pivot_y + dY) * 32, 0, 0, 0, 0);
			m_game->m_effect_manager->add_effect(EffectType::FOOTPRINT, (m_pivot_x + dX) * 32 + (10 - (rand() % 20)), (m_pivot_y + dY) * 32 + (10 - (rand() % 20)), 0, 0, (rand() % 2), 0);
			m_game->m_effect_manager->add_effect(EffectType::FOOTPRINT, (m_pivot_x + dX) * 32 + (10 - (rand() % 20)), (m_pivot_y + dY) * 32 + (10 - (rand() % 20)), 0, 0, (rand() % 2), 0);
		}
	}

	return true;
}

bool CMapData::set_dead_owner(uint16_t object_id, short sX, short sY, short type, direction dir, const hb::shared::entity::PlayerAppearance& appearance, const hb::shared::entity::PlayerStatus& status, std::string& name, short npcConfigId)
{
	int  dX, dY;
	std::string tmp_name;
	bool erase_flag = false;

	tmp_name = name;
	if ((sX < m_pivot_x) || (sX >= m_pivot_x + MapDataSizeX) ||
		(sY < m_pivot_y) || (sY >= m_pivot_y + MapDataSizeY))
	{
		for (dX = 0; dX < MapDataSizeX; dX++)
			for (dY = 0; dY < MapDataSizeY; dY++)
			{
				if (m_data[dX][dY].m_dead_owner_name == tmp_name)
				{
					m_data[dX][dY].m_dead_owner_type = 0;
					m_data[dX][dY].m_dead_npc_config_id = -1;
					m_data[dX][dY].m_dead_owner_name.clear();
				}
			}
		return false;
	}

	for (dX = sX - 2; dX <= sX + 2; dX++)
		for (dY = sY - 2; dY <= sY + 2; dY++)
		{
			if (dX < m_pivot_x) break;
			else
				if (dX > m_pivot_x + MapDataSizeX) break;
			if (dY < m_pivot_y) break;
			else
				if (dY > m_pivot_y + MapDataSizeY) break;

			if (m_data[dX - m_pivot_x][dY - m_pivot_y].m_dead_owner_name == tmp_name)
			{
				m_data[dX - m_pivot_x][dY - m_pivot_y].m_dead_owner_type = 0;
				m_data[dX - m_pivot_x][dY - m_pivot_y].m_dead_npc_config_id = -1;
				m_data[dX - m_pivot_x][dY - m_pivot_y].m_dead_owner_name.clear();
				erase_flag = true;
			}
		}

	if (erase_flag != true) {
		for (dX = 0; dX < MapDataSizeX; dX++)
			for (dY = 0; dY < MapDataSizeY; dY++) {

				if (m_data[dX][dY].m_dead_owner_name == tmp_name) {
					m_data[dX][dY].m_dead_owner_type = 0;
					m_data[dX][dY].m_dead_npc_config_id = -1;
					m_data[dX][dY].m_dead_owner_name.clear();
				}

			}
	}

	dX = sX - m_pivot_x;
	dY = sY - m_pivot_y;

	m_data[dX][dY].m_dead_object_id = object_id;
	m_data[dX][dY].m_dead_owner_type = type;
	m_data[dX][dY].m_dead_npc_config_id = npcConfigId;
	m_data[dX][dY].m_dead_dir = dir;
	m_data[dX][dY].m_dead_appearance = appearance;
	m_data[dX][dY].m_deadStatus = status;
	m_data[dX][dY].m_dead_owner_frame = -1;
	m_data[dX][dY].m_dead_owner_time = m_frame_time;
	m_data[dX][dY].m_dead_owner_name = tmp_name;

	m_object_id_cache_loc_x[object_id] = -1 * sX;
	m_object_id_cache_loc_y[object_id] = -1 * sY;



	return true;
}

bool CMapData::set_chat_msg_owner(uint16_t object_id, short sX, short sY, int index)
{
	int dX, dY;

	if ((sX != -10) || (sY != -10))
	{
		if ((sX < m_pivot_x) || (sX >= m_pivot_x + MapDataSizeX) ||
			(sY < m_pivot_y) || (sY >= m_pivot_y + MapDataSizeY))
		{
			return false;
		}
		for (dX = sX - 4; dX <= sX + 4; dX++)
			for (dY = sY - 4; dY <= sY + 4; dY++)
			{
				if (dX < m_pivot_x) break;
				else
					if (dX > m_pivot_x + MapDataSizeX) break;
				if (dY < m_pivot_y) break;
				else
					if (dY > m_pivot_y + MapDataSizeY) break;

				if (m_data[dX - m_pivot_x][dY - m_pivot_y].m_object_id == object_id) {
					m_data[dX - m_pivot_x][dY - m_pivot_y].m_chat_msg = index;
					return true;
				}
				if (m_data[dX - m_pivot_x][dY - m_pivot_y].m_dead_object_id == object_id) {
					m_data[dX - m_pivot_x][dY - m_pivot_y].m_dead_chat_msg = index;
					return true;
				}
			}
	}

	for (dX = 0; dX < MapDataSizeX; dX++)
		for (dY = 0; dY < MapDataSizeY; dY++) {

			if (m_data[dX][dY].m_object_id == object_id) {
				m_data[dX][dY].m_chat_msg = index;
				return true;
			}
			if (m_data[dX][dY].m_dead_object_id == object_id) {
				m_data[dX][dY].m_dead_chat_msg = index;
				return true;
			}
		}

	return false;
}

void CMapData::clear_chat_msg(short sX, short sY)
{
	m_game->get_floating_text().clear(m_data[sX - m_pivot_x][sY - m_pivot_y].m_chat_msg);
	m_data[sX - m_pivot_x][sY - m_pivot_y].m_chat_msg = 0;
}

void CMapData::clear_dead_chat_msg(short sX, short sY)
{
	m_data[sX - m_pivot_x][sY - m_pivot_y].m_dead_chat_msg = 0;
}

bool CMapData::get_owner(short sX, short sY, std::string& name, short* owner_type, hb::shared::entity::PlayerStatus* owner_status, uint16_t* object_id, short* npc_config_id)
{
	int dX, dY;

	if ((sX < m_pivot_x) || (sX > m_pivot_x + MapDataSizeX) ||
		(sY < m_pivot_y) || (sY > m_pivot_y + MapDataSizeY)) {
		name.clear();
		return false;
	}

	dX = sX - m_pivot_x;
	dY = sY - m_pivot_y;

	*owner_type = m_data[dX][dY].m_owner_type;
	name = m_data[dX][dY].m_owner_name;
	*owner_status = m_data[dX][dY].m_status;
	*object_id = m_data[dX][dY].m_object_id;
	if (npc_config_id) *npc_config_id = m_data[dX][dY].m_npc_config_id;

	return true;
}

bool CMapData::set_dynamic_object(short sX, short sY, uint16_t id, short type, bool is_event)
{
	int dX, dY, prev_type;

	if ((sX < m_pivot_x) || (sX >= m_pivot_x + MapDataSizeX) ||
		(sY < m_pivot_y) || (sY >= m_pivot_y + MapDataSizeY))
	{
		return false;
	}

	dX = sX - m_pivot_x;
	dY = sY - m_pivot_y;

	prev_type = m_data[dX][dY].m_dynamic_object_type;

	m_data[dX][dY].m_dynamic_object_type = type;
	m_data[dX][dY].m_dynamic_object_frame = rand() % 5;
	m_data[dX][dY].m_dynamic_object_time = GameClock::get_time_ms();

	m_data[dX][dY].m_dynamic_object_data_1 = 0;
	m_data[dX][dY].m_dynamic_object_data_2 = 0;
	m_data[dX][dY].m_dynamic_object_data_3 = 0;
	m_data[dX][dY].m_dynamic_object_data_4 = 0;

	switch (type) {
	case 0:
		if (prev_type == dynamic_object::Fire)
		{
			m_game->m_effect_manager->add_effect(EffectType::RED_CLOUD_PARTICLES, (m_pivot_x + dX) * 32, (m_pivot_y + dY) * 32, 0, 0, 0, 0);
			m_game->m_effect_manager->add_effect(EffectType::RED_CLOUD_PARTICLES, (m_pivot_x + dX) * 32 + (10 - (rand() % 20)), (m_pivot_y + dY) * 32 + (20 - (rand() % 40)), 0, 0, 0, 0);
			m_game->m_effect_manager->add_effect(EffectType::RED_CLOUD_PARTICLES, (m_pivot_x + dX) * 32 + (10 - (rand() % 20)), (m_pivot_y + dY) * 32 + (20 - (rand() % 40)), 0, 0, 0, 0);
			m_game->m_effect_manager->add_effect(EffectType::RED_CLOUD_PARTICLES, (m_pivot_x + dX) * 32 + (10 - (rand() % 20)), (m_pivot_y + dY) * 32 + (20 - (rand() % 40)), 0, 0, 0, 0);
		}
		else if ((prev_type == dynamic_object::PCloudBegin) || (prev_type == dynamic_object::PCloudLoop))
		{
			m_data[dX][dY].m_dynamic_object_type = dynamic_object::PCloudEnd;
			m_data[dX][dY].m_dynamic_object_frame = 0;
			m_data[dX][dY].m_dynamic_object_time = GameClock::get_time_ms();
		}
		break;

	case dynamic_object::Fish:
		m_data[dX][dY].m_dynamic_object_data_1 = (rand() % 40) - 20;
		m_data[dX][dY].m_dynamic_object_data_2 = (rand() % 40) - 20;
		m_data[dX][dY].m_dynamic_object_data_3 = (rand() % 10) - 5;
		m_data[dX][dY].m_dynamic_object_data_4 = (rand() % 10) - 5;
		break;

	case dynamic_object::PCloudBegin:
		if (is_event == false)
		{
			m_data[dX][dY].m_dynamic_object_type = dynamic_object::PCloudLoop;
			m_data[dX][dY].m_dynamic_object_frame = rand() % 8;
		}
		else m_data[dX][dY].m_dynamic_object_frame = -1 * (rand() % 8);
		break;

	case dynamic_object::AresdenFlag1:
		m_data[dX][dY].m_dynamic_object_frame = (rand() % 4);
		break;

	case dynamic_object::ElvineFlag1:
		m_data[dX][dY].m_dynamic_object_frame = 4 + (rand() % 4);
		break;
	}
	return true;
}

void CMapData::get_owner_status_by_object_id(uint16_t object_id, char* owner_type, direction* dir, hb::shared::entity::PlayerAppearance* appearance, hb::shared::entity::PlayerStatus* status, std::string& name)
{
	int iX, iY;
	for (iX = 0; iX < MapDataSizeX; iX++)
		for (iY = 0; iY < MapDataSizeY; iY++)
			if (m_data[iX][iY].m_object_id == object_id)
			{
				*owner_type = (char)m_data[iX][iY].m_owner_type;
				*dir = m_data[iX][iY].m_animation.m_dir;
				*appearance = m_data[iX][iY].m_appearance;
				*status = m_data[iX][iY].m_status;
				name = m_data[iX][iY].m_owner_name;
				return;
			}
}
