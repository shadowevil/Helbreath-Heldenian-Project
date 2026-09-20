#pragma once

#include <cstddef>

namespace DialogBoxId
{
	enum Type : size_t
	{
		None = 0,
		CharacterInfo = 1,
		Inventory = 2,
		Magic = 3,
		ItemDropConfirm = 4,
		WarningBattleArea = 6,
		GuildMenu = 7,
		GuildOperation = 8,
		GuideMap = 9,
		ChatHistory = 10,
		SaleMenu = 11,
		LevelUpSetting = 12,
		CityHallMenu = 13,
		Bank = 14,
		Skill = 15,
		MagicShop = 16,
		ItemDropExternal = 17,
		Text = 18,
		SystemMenu = 19,
		NpcActionQuery = 20,
		NpcTalk = 21,
		Map = 22,
		SellOrRepair = 23,
		Fishing = 24,
		Noticement = 25,
		Manufacture = 26,
		Exchange = 27,
		Quest = 28,
		HudPanel = 29,
		SellList = 31,
		Party = 32,
		CrusadeJob = 33,
		ItemUpgrade = 34,
		Help = 35,
		CrusadeCommander = 36,
		CrusadeConstructor = 37,
		CrusadeSoldier = 38,
		GiveItem = 39,
		Slates = 40,
		ConfirmExchange = 41,
		ChangeStatsMajestic = 42,
		StatusOverlay = 49,
		Resurrect = 50,
		CommandHallMenu = 51,
		RepairAll = 52,
#ifdef TESTER_ONLY
		// TESTER MENU — dialog IDs (tester builds only)
		TesterMenu = 53,
		ItemCreator = 54
#endif // TESTER_ONLY
	};
}
