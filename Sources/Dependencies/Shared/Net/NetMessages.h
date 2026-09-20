#pragma once

#include <cstdint>

// NetMessages.h - Shared Network Protocol Definitions
//
// All network message IDs, action types, notification codes, and protocol
// constants shared between client and server.

namespace hb::shared::net
{

// Basic message confirmation/rejection types
namespace MsgType
{
	enum : uint16_t
	{
		Confirm                                 = 0x0F14,
		Reject                                  = 0x0F15,
	};
}

// Message IDs - Top-level protocol message identifiers
namespace MsgId
{
	enum : uint32_t
	{
		RequestInitPlayer                       = 0x05040205,
		ResponseInitPlayer                      = 0x05040206,
		RequestInitData                         = 0x05080404,
		ResponseInitData                        = 0x05080405,
		CommandMotion                           = 0x0FA314D5,
		ResponseMotion                          = 0x0FA314D6,
		EventMotion                             = 0x0FA314D7,
		EventLog                                = 0x0FA314D8,
		EventCommon                             = 0x0FA314DB,
		CommandCommon                           = 0x0FA314DC,
		Notify                                  = 0x0FA314D0,
		ItemConfigContents                      = 0x0FA314D9,
		MagicConfigContents                     = 0x0FA314E0,
		SkillConfigContents                     = 0x0FA314E1,
		NpcConfigContents                       = 0x0FA314E4,
		MapConfigContents                       = 0x0FA314E6,
		BalanceConfigContents                   = 0x0FA314E7,
		ColorPaletteConfigContents              = 0x0FA314E9,
		AttributeTypeConfigContents             = 0x0FA314EA,
		RequestConfigData                       = 0x0FA314E3,
		NotifyConfigReload                      = 0x0FA314E5,
		ServerConfigUpdate                      = 0x0FA314E8,
		PlayerItemListContents                  = 0x0FA314DD,
		PlayerCharacterContents                 = 0x0FA40000,
		CommandCheckConnection                  = 0x03203203,
		CommandChatMsg                          = 0x03203204,
		request_login                            = 0x0FC94201,
		RequestCreateNewAccount                 = 0x0FC94202,
		ResponseLog                             = 0x0FC94203,
		RequestCreateNewCharacter               = 0x0FC94204,
		request_enter_game                        = 0x0FC94205,
		ResponseEnterGame                       = 0x0FC94206,
		RequestDeleteCharacter                  = 0x0FC94207,
		RequestCivilRight                       = 0x0FC9420E,
		ResponseCivilRight                      = 0x0FC9420F,
		RequestChangePassword                   = 0x0FC94210,
		ResponseChangePassword                  = 0x0FC94211,
		RequestInputKeyCode                     = 0x0FC94212,
		ResponseInputKeyCode                    = 0x0FC94213,
		RequestResurrectYes                     = 0x0FC94214,
		RequestResurrectNo                      = 0x0FC94215,
		RequestAngel                            = 0x0FC9421E,
		RequestRetrieveItem                     = 0x0DF30751,
		ResponseRetrieveItem                    = 0x0DF30752,
		request_full_object_data                   = 0x0DF40000,
		RequestTeleport                         = 0x0EA03201,
		RequestTeleportAuth                     = 0x0EA03203,
		RequestHeldenianScroll                  = 0x3D001244,
		LevelUpSettings                         = 0x11A01000,
		DynamicObject                           = 0x12A01001,
		RequestSetItemPos                       = 0x180ACE0A,
		RequestNoticement                       = 0x220B2F00,
		ResponseNoticement                      = 0x220B2F01,
		RequestPanning                          = 0x27B314D0,
		ResponsePanning                         = 0x27B314D1,
		RequestRestart                          = 0x28010EEE,
		RequestSellItemList                     = 0x2900AD30,
		StateChangePoint                        = 0x11A01001,
	};
}

// Common Action Types - Client sends action requests, Server processes them
namespace CommonType
{
	enum : uint16_t
	{
		ItemDrop                                = 0x0A01,
		equip_item                               = 0x0A02,
		ReqListContents                         = 0x0A03,
		ReqPurchaseItem                         = 0x0A04,
		GiveItemToChar                          = 0x0A05,
		ReleaseItem                             = 0x0A0A,
		ToggleCombatMode                        = 0x0A0B,
		SetItem                                 = 0x0A0C,
		Magic                                   = 0x0A0D,
		ReqStudyMagic                           = 0x0A0E,
		ReqTrainSkill                           = 0x0A0F,
		ReqGetRewardMoney                       = 0x0A10,
		ReqUseItem                              = 0x0A11,
		ReqUseSkill                             = 0x0A12,
		ReqSellItem                             = 0x0A13,
		ReqRepairItem                           = 0x0A14,
		ReqSellItemConfirm                      = 0x0A15,
		ReqRepairItemConfirm                    = 0x0A16,
		ReqGetFishThisTime                      = 0x0A17,
		ToggleSafeAttackMode                    = 0x0A18,
		ReqCreatePortion                        = 0x0A19,
		TalkToNpc                               = 0x0A1A,
		ReqSetDownSkillIndex                    = 0x0A1B,
		ReqGetOccupyFlag                        = 0x0A1C,
		ReqGetHeroMantle                        = 0x0A1D,
		ExchangeItemToChar                      = 0x0A1E,
		set_exchange_item                         = 0x0A1F,
		confirm_exchange_item                     = 0x0A20,
		cancel_exchange_item                      = 0x0A21,
		QuestAccepted                           = 0x0A22,
		BuildItem                               = 0x0A23,
		GetMagicAbility                         = 0x0A24,
		CraftItem                               = 0x0A28,
		RequestAcceptJoinParty                  = 0x0A30,
		RequestJoinParty                        = 0x0A31,
		ResponseJoinParty                       = 0x0A32,
		RequestActivateSpecAbility              = 0x0A40,
		RequestCancelQuest                      = 0x0A50,
		RequestSelectCrusadeDuty                = 0x0A51,
		RequestMapStatus                        = 0x0A52,
		RequestHelp                             = 0x0A53,
		SummonWarUnit                           = 0x0A56,
		UpgradeItem                             = 0x0A58,
		RequestHuntMode                         = 0x0A60,
		ReqCreateSlate                          = 0x0A61,
		EnchantShard                            = 0x0A71,
		EnchantFragment                         = 0x0A72,
		EnchantItem                             = 0x0A73,
		UpgradeEnchant                          = 0x0A74,
		DisenchantItem                          = 0x0A75,
#ifdef TESTER_ONLY
		// TESTER MENU — tester-only commands
		TesterAction                            = 0x0A76,
		TesterItemSearch                        = 0x0A77,
		TesterCreateItem                        = 0x0A78,
		TesterMapList                           = 0x0A79,
#endif // TESTER_ONLY
		ReqRepairAll                            = 0x0F10,
		ReqRepairAllDelete                      = 0x0F12,
		ReqRepairAllConfirm                     = 0x0F13,
	};
}

// Notification Types - Server sends notifications to Client
namespace Notify
{
	enum : uint16_t
	{
		ItemObtained                            = 0x0B01,
		CannotCarryMoreItem                     = 0x0B05,
		ItemPurchased                           = 0x0B06,
		Hp                                      = 0x0B07,
		NotEnoughGold                           = 0x0B08,
		Killed                                  = 0x0B09,
		Exp                                     = 0x0B0A,
		EventMsgString                          = 0x0B0C,
		MagicStudySuccess                       = 0x0B10,
		MagicStudyFail                          = 0x0B11,
		SkillTrainSuccess                       = 0x0B12,
		SkillTrainFail                          = 0x0B13,
		Mp                                      = 0x0B14,
		Sp                                      = 0x0B15,
		LevelUp                                 = 0x0B16,
		ItemDurabilityEnd                       = 0x0B17,
		LimitedLevel                            = 0x0B18,
		ItemToBank                              = 0x0B19,
		PkPenalty                               = 0x0B1A,
		PkCaptured                              = 0x0B1B,
		EnemyKillReward                         = 0x0B1C,
		GiveItemFinEraseItem                    = 0x0B1D,
		DropItemFinEraseItem                    = 0x0B1F,
		ItemDepletedEraseItem                   = 0x0B20,
		NewDynamicObject                        = 0x0B21,
		DelDynamicObject                        = 0x0B22,
		Skill                                   = 0x0B23,
		ServerChange                            = 0x0B24,
		set_item_count                            = 0x0B25,
		CannotItemToBank                        = 0x0B26,
		MagicEffectOn                           = 0x0B27,
		MagicEffectOff                          = 0x0B28,
		TotalUsers                              = 0x0B29,
		SkillUsingEnd                           = 0x0B2A,
		ShowMap                                 = 0x0B2B,
		CannotSellItem                          = 0x0B2C,
		SellItemPrice                           = 0x0B2D,
		CannotRepairItem                        = 0x0B2E,
		RepairItemPrice                         = 0x0B2F,
		ItemRepaired                            = 0x0B30,
		ItemSold                                = 0x0B31,
		Charisma                                = 0x0B32,
		PlayerOnGame                            = 0x0B33,
		PlayerNotOnGame                         = 0x0B34,
		WhisperModeOn                           = 0x0B35,
		WhisperModeOff                          = 0x0B36,
		PlayerProfile                           = 0x0B37,
		TravelerLimitedLevel                    = 0x0B38,
		Hunger                                  = 0x0B39,
		ToBeRecalled                            = 0x0B40,
		TimeChange                              = 0x0B41,
		PlayerShutUp                            = 0x0B42,
		StatusText                              = 0x0B43,
		CannotRating                            = 0x0B44,
		RatingPlayer                            = 0x0B45,
		NoticeMsg                               = 0x0B46,
		EventFishMode                           = 0x0B47,
		FishChance                              = 0x0B48,
		DebugMsg                                = 0x0B49,
		FishSuccess                             = 0x0B4A,
		FishFail                                = 0x0B4B,
		FishCanceled                            = 0x0B4C,
		WhetherChange                           = 0x0B4D,
		ServerShutdown                          = 0x0B4E,
		RewardGold                              = 0x0B4F,
		IpAccountInfo                           = 0x0B50,
		SafeAttackMode                          = 0x0B51,
		SuperAttackLeft                         = 0x0B52,
		NoMatchingPortion                       = 0x0B53,
		LowPortionSkill                         = 0x0B54,
		PortionFail                             = 0x0B55,
		PortionSuccess                          = 0x0B56,
		NpcTalk                                 = 0x0B57,
		DownSkillIndexSet                       = 0x0B59,
		EnemyKills                              = 0x0B5A,
		ItemPosList                             = 0x0B5B,
		ItemReleased                            = 0x0B5C,
		NotFlagSpot                             = 0x0B5D,
		OpenExchangeWindow                      = 0x0B5E,
		set_exchange_item                         = 0x0B5F,
		cancel_exchange_item                      = 0x0B60,
		ExchangeItemComplete                    = 0x0B61,
		CannotGiveItem                          = 0x0B62,
		GiveItemFinCountChanged                 = 0x0B63,
		DropItemFinCountChanged                 = 0x0B64,
		ItemColorChange                         = 0x0B65,
		QuestContents                           = 0x0B66,
		QuestAborted                            = 0x0B67,
		QuestCompleted                          = 0x0B68,
		QuestReward                             = 0x0B69,
		BuildItemSuccess                        = 0x0B70,
		BuildItemFail                           = 0x0B71,
		ObserverMode                            = 0x0B72,
		GlobalAttackMode                        = 0x0B73,
		DamageMove                              = 0x0B74,
		ForceDisconn                            = 0x0B75,
		ResponseCreateNewParty                  = 0x0B80,
		QueryJoinParty                          = 0x0B81,
		EnergySphereCreated                     = 0x0B90,
		EnergySphereGoalIn                      = 0x0B91,
		SpecialAbilityEnabled                   = 0x0B92,
		SpecialAbilityStatus                    = 0x0B93,
		Crusade                                 = 0x0B94,
		LockedMap                               = 0x0B95,
		DutySelected                            = 0x0B96,
		MapStatusNext                           = 0x0B97,
		MapStatusLast                           = 0x0B98,
		Help                                    = 0x0B99,
		HelpFailed                              = 0x0B9A,
		meteor_strike_coming                      = 0x0B9B,
		MeteorStrikeHit                         = 0x0B9C,
		grand_magic_result                        = 0x0B9D,
		NoMoreCrusadeStructure                  = 0x0B9E,
		ConstructionPoint                       = 0x0B9F,
		TcLoc                                   = 0x0BA0,
		cannot_construct                         = 0x0BA1,
		Party                                   = 0x0BA2,
		ItemAttributeChange                     = 0x0BA3,
		GizonItemUpgradeLeft                    = 0x0BA4,
		GizoneItemChange                        = 0x0BA5,
		ForceRecallTime                         = 0x0BA7,
		ItemUpgradeFail                         = 0x0BA8,
		ChangePlayMode                          = 0x0BA9,
		SpawnEvent                              = 0x0BAA,
		NoMoreAgriculture                       = 0x0BB0,
		AgricultureSkillLimit                   = 0x0BB1,
		AgricultureNoArea                       = 0x0BB2,
		SettingSuccess                          = 0x0BB3,
		SettingFailed                           = 0x0BB4,
		StateChangeSuccess                      = 0x0BB5,
		StateChangeFailed                       = 0x0BB6,
		SlateCreateSuccess                      = 0x0BC1,
		SlateCreateFail                         = 0x0BC2,
		NoRecall                                = 0x0BD1,
		ApocGateStartMsg                        = 0x0BD2,
		ApocGateEndMsg                          = 0x0BD3,
		ApocGateOpen                            = 0x0BD4,
		ApocGateClose                           = 0x0BD5,
		AbaddonKilled                           = 0x0BD6,
		ApocForceRecallPlayers                  = 0x0BD7,
		SlateInvincible                         = 0x0BD8,
		SlateMana                               = 0x0BD9,
		SlateExp                                = 0x0BE0,
		SlateStatus                             = 0x0BE1,
		QuestCounter                            = 0x0BE2,
		MonsterCount                            = 0x0BE3,
		HeldenianTeleport                       = 0x0BE6,
		HeldenianEnd                            = 0x0BE7,
		Unknown0BE8                             = 0x0BE8,
		ResurrectPlayer                         = 0x0BE9,
		HeldenianStart                          = 0x0BEA,
		HeldenianVictory                        = 0x0BEB,
		HeldenianCount                          = 0x0BEC,
		SlateBerserk                            = 0x0BED,
		LoteryLost                              = 0x0BEE,
		Unknown0BEF                             = 0x0BEF,
		CraftingSuccess                         = 0x0BF0,
		CraftingFail                            = 0x0BF1,
		AngelicStats                            = 0x0BF2,
		CurDurability                           = 0x0BF3,
		AngelFailed                             = 0x0BF4,
		AngelReceived                           = 0x0BF5,
		SpellSkill                              = 0x0BF6,
		SpellInterrupted                        = 0x0BF7,
		ItemObtainedBulk                        = 0x0BF8,
		TeleportApproved                        = 0x0BF9,
		LevelUpPoints                           = 0x0BFA,
		ForceStatRefresh                        = 0x0BFB,
		ForceMasteryRefresh                     = 0x0BFC,
		RepairAllPrices                         = 0x0F11,
		MobKills                                = 0x0A68,
#ifdef TESTER_ONLY
		// TESTER MENU — tester-only notifications
		Contribution                            = 0x0A69,
		TesterItemSearchResult                  = 0x0A6A,
		TesterMapListResult                     = 0x0A6B,
#endif // TESTER_ONLY
	};
}

// Login Server Response Types
namespace LogResMsg
{
	enum : uint16_t
	{
		Confirm                                 = 0x0F14,
		Reject                                  = 0x0F15,
		PasswordMismatch                        = 0x0F16,
		NotExistingAccount                      = 0x0F17,
		NewAccountCreated                       = 0x0F18,
		NewAccountFailed                        = 0x0F19,
		AlreadyExistingAccount                  = 0x0F1A,
		NotExistingCharacter                    = 0x0F1B,
		NewCharacterCreated                     = 0x0F1C,
		NewCharacterFailed                      = 0x0F1D,
		AlreadyExistingCharacter                = 0x0F1E,
		CharacterDeleted                        = 0x0F1F,
		NotEnoughPoint                          = 0x0F30,
		AccountLocked                           = 0x0F31,
		ServiceNotAvailable                     = 0x0F32,
		PasswordChangeSuccess                   = 0x0A00,
		PasswordChangeFail                      = 0x0A01,
		NotExistingWorldServer                  = 0x0A02,
		InputKeyCode                            = 0x0A03,
		RealAccount                             = 0x0A04,
		ForceChangePassword                     = 0x0A05,
		InvalidKoreanSsn                        = 0x0A06,
		LessThenFifteen                         = 0x0A07,
		VersionMismatch                         = 0x0A08,
	};
}

// Enter Game Message Types
namespace EnterGameMsg
{
	enum : uint16_t
	{
		New                                     = 0x0F1C,
		NoEnterForceDisconn                     = 0x0F1D,
		ChangingServer                          = 0x0F1E,
		NewToWlsButMls                          = 0x0F1F,
	};
}

// Enter Game Response Types
namespace EnterGameRes
{
	enum : uint16_t
	{
		Playing                                 = 0x0F20,
		Reject                                  = 0x0F21,
		Confirm                                 = 0x0F22,
		ForceDisconn                            = 0x0F23,
	};
}

// Stat IDs for level-up point allocation
namespace StatId
{
	enum : uint8_t
	{
		Str                                     = 0x01,
		Dex                                     = 0x02,
		Int                                     = 0x03,
		Vit                                     = 0x04,
		Mag                                     = 0x05,
		Chr                                     = 0x06,
	};
}

} // namespace hb::shared::net
