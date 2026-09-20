#include "Game.h"
#include "NetworkMessageManager.h"
#include "TeleportManager.h"
#include "Packet/SharedPackets.h"
#include "lan_eng.h"
#include <cstdio>
#include <cstring>
#include <cmath>

using namespace hb::shared::net;
namespace NetworkMessageHandlers {
	// Stats
	void HandleHP(CGame* game, char* data);
	void HandleMP(CGame* game, char* data);
	void HandleSP(CGame* game, char* data);
	void HandleExp(CGame* game, char* data);
	void HandleLevelUp(CGame* game, char* data);
	void HandleLevelUpPoints(CGame* game, char* data);
	void HandleForceStatRefresh(CGame* game, char* data);

	// Exchange
	void HandleExchangeItemComplete(CGame* game, char* data);
	void HandleCancelExchangeItem(CGame* game, char* data);

	// Bank
	void HandleItemToBank(CGame* game, char* data);
	void HandleCannotItemToBank(CGame* game, char* data);

	// Slates
	void HandleSlateCreateSuccess(CGame* game, char* data);
	void HandleSlateCreateFail(CGame* game, char* data);
	void HandleSlateInvincible(CGame* game, char* data);
	void HandleSlateMana(CGame* game, char* data);
	void HandleSlateExp(CGame* game, char* data);
	void HandleSlateStatus(CGame* game, char* data);
	void HandleSlateBerserk(CGame* game, char* data);

	// Map
	void HandleMapStatusNext(CGame* game, char* data);
	void HandleMapStatusLast(CGame* game, char* data);
	void HandleLockedMap(CGame* game, char* data);
	void HandleShowMap(CGame* game, char* data);

	// Events
	void HandleSpawnEvent(CGame* game, char* data);
	void HandleMonsterCount(CGame* game, char* data);
	void HandleResurrectPlayer(CGame* game, char* data);

	// Agriculture
	void HandleAgricultureNoArea(CGame* game, char* data);
	void HandleAgricultureSkillLimit(CGame* game, char* data);
	void HandleNoMoreAgriculture(CGame* game, char* data);

	// Angels
	void HandleAngelFailed(CGame* game, char* data);
	void HandleAngelReceived(CGame* game, char* data);
	void HandleAngelicStats(CGame* game, char* data);

	// Party
	void HandleParty(CGame* game, char* data);
	void HandleQueryJoinParty(CGame* game, char* data);
	void HandleResponseCreateNewParty(CGame* game, char* data);

	// Quest
	void handle_quest_contents(CGame* game, char* data);
	void handle_quest_reward(CGame* game, char* data);
	void handle_quest_completed(CGame* game, char* data);
	void handle_quest_aborted(CGame* game, char* data);
	void handle_quest_counter(CGame* game, char* data);

	// Fish
	void handle_fish_chance(CGame* game, char* data);
	void handle_event_fish_mode(CGame* game, char* data);
	void handle_fish_canceled(CGame* game, char* data);
	void handle_fish_success(CGame* game, char* data);
	void handle_fish_fail(CGame* game, char* data);

	// Items
	void HandleItemPurchased(CGame* game, char* data);
	void HandleItemObtained(CGame* game, char* data);
	void HandleItemObtainedBulk(CGame* game, char* data);
	void HandleItemDurabilityEnd(CGame* game, char* data);
	void HandleItemReleased(CGame* game, char* data);
	void HandleSetItemCount(CGame* game, char* data);
	void HandleItemDepleted_EraseItem(CGame* game, char* data);
	void HandleDropItemFin_EraseItem(CGame* game, char* data);
	void HandleGiveItemFin_EraseItem(CGame* game, char* data);
	void HandleDropItemFin_CountChanged(CGame* game, char* data);
	void HandleGiveItemFin_CountChanged(CGame* game, char* data);
	void HandleItemRepaired(CGame* game, char* data);
	void HandleRepairItemPrice(CGame* game, char* data);
	void HandleRepairAllPrices(CGame* game, char* data);
	void HandleSellItemPrice(CGame* game, char* data);
	void HandleCannotRepairItem(CGame* game, char* data);
	void HandleCannotSellItem(CGame* game, char* data);
	void HandleCannotGiveItem(CGame* game, char* data);
	void HandleItemColorChange(CGame* game, char* data);
	void HandleSetExchangeItem(CGame* game, char* data);
	void HandleOpenExchangeWindow(CGame* game, char* data);
	void HandleCurDurability(CGame* game, char* data);
	void HandleNotEnoughGold(CGame* game, char* data);
	void HandleCannotCarryMoreItem(CGame* game, char* data);
	void HandleItemAttributeChange(CGame* game, char* data);
	void HandleItemUpgradeFail(CGame* game, char* data);
	void HandleGizonItemUpgradeLeft(CGame* game, char* data);
	void HandleGizonItemChange(CGame* game, char* data);
	void HandleItemPosList(CGame* game, char* data);
	void HandleItemSold(CGame* game, char* data);

	// Apocalypse
	void HandleApocGateStart(CGame* game, char* data);
	void HandleApocGateEnd(CGame* game, char* data);
	void HandleApocGateOpen(CGame* game, char* data);
	void HandleApocGateClose(CGame* game, char* data);
	void HandleApocForceRecall(CGame* game, char* data);
	void HandleAbaddonKilled(CGame* game, char* data);

	// Heldenian
	void HandleHeldenianTeleport(CGame* game, char* data);
	void HandleHeldenianEnd(CGame* game, char* data);
	void HandleHeldenianStart(CGame* game, char* data);
	void HandleHeldenianVictory(CGame* game, char* data);
	void HandleHeldenianCount(CGame* game, char* data);
	void HandleHeldenianRecall(CGame* game, char* data);


	void HandleCrashHandler(CGame* game, char* data);
	void HandleIpAccountInfo(CGame* game, char* data);
	void HandleRewardGold(CGame* game, char* data);
	void HandleServerShutdown(CGame* game, char* data);
	// Crafting
	void handle_crafting_success(CGame* game, char* data);
	void handle_crafting_fail(CGame* game, char* data);
	void handle_build_item_success(CGame* game, char* data);
	void handle_build_item_fail(CGame* game, char* data);
	void handle_portion_success(CGame* game, char* data);
	void handle_portion_fail(CGame* game, char* data);
	void handle_low_portion_skill(CGame* game, char* data);
	void handle_no_matching_portion(CGame* game, char* data);

	// Combat
	void HandleSpellInterrupted(CGame* game, char* data);
	void HandleKilled(CGame* game, char* data);
	void HandlePKcaptured(CGame* game, char* data);
	void HandlePKpenalty(CGame* game, char* data);
	void HandleEnemyKills(CGame* game, char* data);
	void HandleContribution(CGame* game, char* data);
#ifdef TESTER_ONLY
	// TESTER MENU — notification handlers (tester builds only)
	void HandleTesterItemSearchResult(CGame* game, char* data);
	void HandleTesterMapListResult(CGame* game, char* data);
#endif // TESTER_ONLY
	void HandleEnemyKillReward(CGame* game, char* data);
	void HandleGlobalAttackMode(CGame* game, char* data);
	void HandleDamageMove(CGame* game, char* data);
	void HandleObserverMode(CGame* game, char* data);
	void HandleSuperAttackLeft(CGame* game, char* data);
	void HandleSafeAttackMode(CGame* game, char* data);
	// Skills
	void HandleSkillTrainSuccess(CGame* game, char* data);
	void HandleMagicStudySuccess(CGame* game, char* data);
	void HandleMagicStudyFail(CGame* game, char* data);
	void HandleDownSkillIndexSet(CGame* game, char* data);
	void HandleSkill(CGame* game, char* data);
	void HandleSkillUsingEnd(CGame* game, char* data);
	void HandleMagicEffectOn(CGame* game, char* data);
	void HandleMagicEffectOff(CGame* game, char* data);
	void HandleSpellSkill(CGame* game, char* data);
	void HandleStateChangeSuccess(CGame* game, char* data);
	void HandleStateChangeFailed(CGame* game, char* data);
	void HandleForceMasteryRefresh(CGame* game, char* data);
	void HandleSettingFailed(CGame* game, char* data);
	void HandleSpecialAbilityStatus(CGame* game, char* data);
	void HandleSpecialAbilityEnabled(CGame* game, char* data);
	void HandleSkillTrainFail(CGame* game, char* data);

	// Player
	void HandleCharisma(CGame* game, char* data);
	void HandleHunger(CGame* game, char* data);
	void HandlePlayerProfile(CGame* game, char* data);
	void HandlePlayerStatus(CGame* game, bool on_game, char* data);
	void HandleWhisperMode(CGame* game, bool active, char* data);
	void HandlePlayerShutUp(CGame* game, char* data);
	void HandleRatingPlayer(CGame* game, char* data);
	void HandleCannotRating(CGame* game, char* data);

	// Crusade
	void HandleCrusade(CGame* game, char* data);
	void HandleGrandMagicResult(CGame* game, char* data);
	void HandleNoMoreCrusadeStructure(CGame* game, char* data);
	void HandleEnergySphereGoalIn(CGame* game, char* data);
	void HandleEnergySphereCreated(CGame* game, char* data);
	void HandleMeteorStrikeComing(CGame* game, char* data);
	void HandleMeteorStrikeHit(CGame* game, char* data);
	void HandleCannotConstruct(CGame* game, char* data);
	void HandleTCLoc(CGame* game, char* data);
	void HandleConstructionPoint(CGame* game, char* data);

	// System
	void HandleWeatherChange(CGame* game, char* data);
	void HandleTimeChange(CGame* game, char* data);
	void HandleNoticeMsg(CGame* game, char* data);
	void HandleStatusText(CGame* game, char* data);
	void HandleForceDisconn(CGame* game, char* data);
	void HandleSettingSuccess(CGame* game, char* data);
	void HandleServerChange(CGame* game, char* data);
	void HandleTotalUsers(CGame* game, char* data);
	void HandleChangePlayMode(CGame* game, char* data);
	void HandleForceRecallTime(CGame* game, char* data);
	void HandleNoRecall(CGame* game, char* data);
	void HandleLoteryLost(CGame* game, char* data);
	void HandleNotFlagSpot(CGame* game, char* data);
	void HandleNpcTalk(CGame* game, char* data);
	void HandleTravelerLimitedLevel(CGame* game, char* data);
	void HandleLimitedLevel(CGame* game, char* data);
	void HandleToBeRecalled(CGame* game, char* data);
}

NetworkMessageManager::NetworkMessageManager(CGame* game)
	: m_game(game)
{
}

bool NetworkMessageManager::process_message(uint32_t msg_id, char* data, uint32_t msg_size)
{
	const auto* header = hb::net::PacketCast<hb::net::PacketHeader>(data, sizeof(hb::net::PacketHeader));
	if (!header) return false;

	if (msg_id == MsgId::Notify)
	{
		switch (header->msg_type)
		{
		// Stats
		case Notify::Hp: NetworkMessageHandlers::HandleHP(m_game, data); return true;
		case Notify::Mp: NetworkMessageHandlers::HandleMP(m_game, data); return true;
		case Notify::Sp: NetworkMessageHandlers::HandleSP(m_game, data); return true;
		case Notify::Exp: NetworkMessageHandlers::HandleExp(m_game, data); return true;
		case Notify::LevelUp: NetworkMessageHandlers::HandleLevelUp(m_game, data); return true;
		case Notify::LevelUpPoints: NetworkMessageHandlers::HandleLevelUpPoints(m_game, data); return true;
		case Notify::ForceStatRefresh: NetworkMessageHandlers::HandleForceStatRefresh(m_game, data); return true;

		// Items - Purchased/Obtained
		case Notify::ItemPurchased: NetworkMessageHandlers::HandleItemPurchased(m_game, data); return true;
		case Notify::ItemObtained: NetworkMessageHandlers::HandleItemObtained(m_game, data); return true;
		case Notify::ItemObtainedBulk: NetworkMessageHandlers::HandleItemObtainedBulk(m_game, data); return true;

		// Items - Durability/Released
		case Notify::ItemDurabilityEnd: NetworkMessageHandlers::HandleItemDurabilityEnd(m_game, data); return true;
		case Notify::ItemReleased: NetworkMessageHandlers::HandleItemReleased(m_game, data); return true;

		// Items - Count/Depleted
		case Notify::set_item_count: NetworkMessageHandlers::HandleSetItemCount(m_game, data); return true;
		case Notify::ItemDepletedEraseItem: NetworkMessageHandlers::HandleItemDepleted_EraseItem(m_game, data); return true;

		// Items - Drop/Give
		case Notify::DropItemFinEraseItem: NetworkMessageHandlers::HandleDropItemFin_EraseItem(m_game, data); return true;
		case Notify::GiveItemFinEraseItem: NetworkMessageHandlers::HandleGiveItemFin_EraseItem(m_game, data); return true;
		case Notify::DropItemFinCountChanged: NetworkMessageHandlers::HandleDropItemFin_CountChanged(m_game, data); return true;
		case Notify::GiveItemFinCountChanged: NetworkMessageHandlers::HandleGiveItemFin_CountChanged(m_game, data); return true;

		// Items - Repair/Sell
		case Notify::ItemRepaired: NetworkMessageHandlers::HandleItemRepaired(m_game, data); return true;
		case Notify::RepairItemPrice: NetworkMessageHandlers::HandleRepairItemPrice(m_game, data); return true;
		case Notify::RepairAllPrices: NetworkMessageHandlers::HandleRepairAllPrices(m_game, data); return true;
		case Notify::SellItemPrice: NetworkMessageHandlers::HandleSellItemPrice(m_game, data); return true;
		case Notify::CannotRepairItem: NetworkMessageHandlers::HandleCannotRepairItem(m_game, data); return true;
		case Notify::CannotSellItem: NetworkMessageHandlers::HandleCannotSellItem(m_game, data); return true;

		// Items - Give/Cannot
		case Notify::CannotGiveItem: NetworkMessageHandlers::HandleCannotGiveItem(m_game, data); return true;

		// Items - hb::shared::render::Color/Exchange
		case Notify::ItemColorChange: NetworkMessageHandlers::HandleItemColorChange(m_game, data); return true;
		case Notify::set_exchange_item: NetworkMessageHandlers::HandleSetExchangeItem(m_game, data); return true;
		case Notify::OpenExchangeWindow: NetworkMessageHandlers::HandleOpenExchangeWindow(m_game, data); return true;
		case Notify::CurDurability: NetworkMessageHandlers::HandleCurDurability(m_game, data); return true;

		// Items - Upgrade/Attribute/Errors
		case Notify::NotEnoughGold: NetworkMessageHandlers::HandleNotEnoughGold(m_game, data); return true;
		case Notify::CannotCarryMoreItem: NetworkMessageHandlers::HandleCannotCarryMoreItem(m_game, data); return true;
		case Notify::ItemAttributeChange: NetworkMessageHandlers::HandleItemAttributeChange(m_game, data); return true;
		case 0x0BC0: NetworkMessageHandlers::HandleItemAttributeChange(m_game, data); return true; // Same handler as ITEMATTRIBUTECHANGE
		case Notify::ItemUpgradeFail: NetworkMessageHandlers::HandleItemUpgradeFail(m_game, data); return true;
		case Notify::GizonItemUpgradeLeft: NetworkMessageHandlers::HandleGizonItemUpgradeLeft(m_game, data); return true;
		case Notify::GizoneItemChange: NetworkMessageHandlers::HandleGizonItemChange(m_game, data); return true;
		case Notify::ItemPosList: NetworkMessageHandlers::HandleItemPosList(m_game, data); return true;
		case Notify::ItemSold: NetworkMessageHandlers::HandleItemSold(m_game, data); return true;

		// Bank
		case Notify::ItemToBank: NetworkMessageHandlers::HandleItemToBank(m_game, data); return true;
		case Notify::CannotItemToBank: NetworkMessageHandlers::HandleCannotItemToBank(m_game, data); return true;

		// Exchange
		case Notify::ExchangeItemComplete: NetworkMessageHandlers::HandleExchangeItemComplete(m_game, data); return true;
		case Notify::cancel_exchange_item: NetworkMessageHandlers::HandleCancelExchangeItem(m_game, data); return true;

		// Combat
		case Notify::Killed: NetworkMessageHandlers::HandleKilled(m_game, data); return true;
		case Notify::PkCaptured: NetworkMessageHandlers::HandlePKcaptured(m_game, data); return true;
		case Notify::PkPenalty: NetworkMessageHandlers::HandlePKpenalty(m_game, data); return true;
		case Notify::EnemyKills: NetworkMessageHandlers::HandleEnemyKills(m_game, data); return true;
		case Notify::Contribution: NetworkMessageHandlers::HandleContribution(m_game, data); return true;
		case Notify::EnemyKillReward: NetworkMessageHandlers::HandleEnemyKillReward(m_game, data); return true;
		case Notify::GlobalAttackMode: NetworkMessageHandlers::HandleGlobalAttackMode(m_game, data); return true;
		case Notify::DamageMove: NetworkMessageHandlers::HandleDamageMove(m_game, data); return true;
		case Notify::ObserverMode: NetworkMessageHandlers::HandleObserverMode(m_game, data); return true;
		case Notify::SuperAttackLeft: NetworkMessageHandlers::HandleSuperAttackLeft(m_game, data); return true;
		case Notify::SafeAttackMode: NetworkMessageHandlers::HandleSafeAttackMode(m_game, data); return true;
		// Skills
		case Notify::SkillTrainSuccess: NetworkMessageHandlers::HandleSkillTrainSuccess(m_game, data); return true;
		case Notify::MagicStudySuccess: NetworkMessageHandlers::HandleMagicStudySuccess(m_game, data); return true;
		case Notify::MagicStudyFail: NetworkMessageHandlers::HandleMagicStudyFail(m_game, data); return true;
		case Notify::DownSkillIndexSet: NetworkMessageHandlers::HandleDownSkillIndexSet(m_game, data); return true;
		case Notify::Skill: NetworkMessageHandlers::HandleSkill(m_game, data); return true;
		case Notify::SkillUsingEnd: NetworkMessageHandlers::HandleSkillUsingEnd(m_game, data); return true;
		case Notify::MagicEffectOn: NetworkMessageHandlers::HandleMagicEffectOn(m_game, data); return true;
		case Notify::MagicEffectOff: NetworkMessageHandlers::HandleMagicEffectOff(m_game, data); return true;
		case Notify::SpellSkill: NetworkMessageHandlers::HandleSpellSkill(m_game, data); return true;
		case Notify::SpellInterrupted: NetworkMessageHandlers::HandleSpellInterrupted(m_game, data); return true;
		case Notify::StateChangeSuccess: NetworkMessageHandlers::HandleStateChangeSuccess(m_game, data); return true;
		case Notify::ForceMasteryRefresh: NetworkMessageHandlers::HandleForceMasteryRefresh(m_game, data); return true;
		case Notify::StateChangeFailed: NetworkMessageHandlers::HandleStateChangeFailed(m_game, data); return true;
		case Notify::SettingFailed: NetworkMessageHandlers::HandleSettingFailed(m_game, data); return true;
		case Notify::SpecialAbilityStatus: NetworkMessageHandlers::HandleSpecialAbilityStatus(m_game, data); return true;
		case Notify::SpecialAbilityEnabled: NetworkMessageHandlers::HandleSpecialAbilityEnabled(m_game, data); return true;
		case Notify::SkillTrainFail: NetworkMessageHandlers::HandleSkillTrainFail(m_game, data); return true;

		// Player
		case Notify::Charisma: NetworkMessageHandlers::HandleCharisma(m_game, data); return true;
		case Notify::Hunger: NetworkMessageHandlers::HandleHunger(m_game, data); return true;
		case Notify::PlayerProfile: NetworkMessageHandlers::HandlePlayerProfile(m_game, data); return true;
		case Notify::PlayerOnGame: NetworkMessageHandlers::HandlePlayerStatus(m_game, true, data); return true;
		case Notify::PlayerNotOnGame: NetworkMessageHandlers::HandlePlayerStatus(m_game, false, data); return true;
		case Notify::WhisperModeOn: NetworkMessageHandlers::HandleWhisperMode(m_game, true, data); return true;
		case Notify::WhisperModeOff: NetworkMessageHandlers::HandleWhisperMode(m_game, false, data); return true;
		case Notify::PlayerShutUp: NetworkMessageHandlers::HandlePlayerShutUp(m_game, data); return true;
		case Notify::RatingPlayer: NetworkMessageHandlers::HandleRatingPlayer(m_game, data); return true;
		case Notify::CannotRating: NetworkMessageHandlers::HandleCannotRating(m_game, data); return true;
		case Notify::ChangePlayMode: NetworkMessageHandlers::HandleChangePlayMode(m_game, data); return true;

		// Quest
		case Notify::QuestContents: NetworkMessageHandlers::handle_quest_contents(m_game, data); return true;
		case Notify::QuestReward: NetworkMessageHandlers::handle_quest_reward(m_game, data); return true;
		case Notify::QuestCounter: NetworkMessageHandlers::handle_quest_counter(m_game, data); return true;
		case Notify::QuestCompleted: NetworkMessageHandlers::handle_quest_completed(m_game, data); return true;
		case Notify::QuestAborted: NetworkMessageHandlers::handle_quest_aborted(m_game, data); return true;

		// Party
		case Notify::Party: NetworkMessageHandlers::HandleParty(m_game, data); return true;
		case Notify::QueryJoinParty: NetworkMessageHandlers::HandleQueryJoinParty(m_game, data); return true;
		case Notify::ResponseCreateNewParty: NetworkMessageHandlers::HandleResponseCreateNewParty(m_game, data); return true;

		// Apocalypse
		case Notify::ApocGateStartMsg: NetworkMessageHandlers::HandleApocGateStart(m_game, data); return true;
		case Notify::ApocGateEndMsg: NetworkMessageHandlers::HandleApocGateEnd(m_game, data); return true;
		case Notify::ApocGateOpen: NetworkMessageHandlers::HandleApocGateOpen(m_game, data); return true;
		case Notify::ApocGateClose: NetworkMessageHandlers::HandleApocGateClose(m_game, data); return true;
		case Notify::ApocForceRecallPlayers: NetworkMessageHandlers::HandleApocForceRecall(m_game, data); return true;
		case Notify::AbaddonKilled: NetworkMessageHandlers::HandleAbaddonKilled(m_game, data); return true;

		// Heldenian
		case Notify::HeldenianTeleport: NetworkMessageHandlers::HandleHeldenianTeleport(m_game, data); return true;
		case Notify::HeldenianEnd: NetworkMessageHandlers::HandleHeldenianEnd(m_game, data); return true;
		case Notify::HeldenianStart: NetworkMessageHandlers::HandleHeldenianStart(m_game, data); return true;
		case Notify::HeldenianVictory: NetworkMessageHandlers::HandleHeldenianVictory(m_game, data); return true;
		case Notify::HeldenianCount: NetworkMessageHandlers::HandleHeldenianCount(m_game, data); return true;
		case Notify::Unknown0BE8: NetworkMessageHandlers::HandleHeldenianRecall(m_game, data); return true;

		// Slates
		case Notify::SlateCreateSuccess: NetworkMessageHandlers::HandleSlateCreateSuccess(m_game, data); return true;
		case Notify::SlateCreateFail: NetworkMessageHandlers::HandleSlateCreateFail(m_game, data); return true;
		case Notify::SlateInvincible: NetworkMessageHandlers::HandleSlateInvincible(m_game, data); return true;
		case Notify::SlateMana: NetworkMessageHandlers::HandleSlateMana(m_game, data); return true;
		case Notify::SlateExp: NetworkMessageHandlers::HandleSlateExp(m_game, data); return true;
		case Notify::SlateStatus: NetworkMessageHandlers::HandleSlateStatus(m_game, data); return true;
		case Notify::SlateBerserk: NetworkMessageHandlers::HandleSlateBerserk(m_game, data); return true;

		// Events (Generic)
		case Notify::SpawnEvent: NetworkMessageHandlers::HandleSpawnEvent(m_game, data); return true;
		case Notify::MonsterCount: NetworkMessageHandlers::HandleMonsterCount(m_game, data); return true;
		case Notify::ResurrectPlayer: NetworkMessageHandlers::HandleResurrectPlayer(m_game, data); return true;

		// Crusade
		case Notify::Crusade: NetworkMessageHandlers::HandleCrusade(m_game, data); return true;
		case Notify::grand_magic_result: NetworkMessageHandlers::HandleGrandMagicResult(m_game, data); return true;
		case Notify::NoMoreCrusadeStructure: NetworkMessageHandlers::HandleNoMoreCrusadeStructure(m_game, data); return true;
		case Notify::EnergySphereGoalIn: NetworkMessageHandlers::HandleEnergySphereGoalIn(m_game, data); return true;
		case Notify::EnergySphereCreated: NetworkMessageHandlers::HandleEnergySphereCreated(m_game, data); return true;
		case Notify::meteor_strike_coming: NetworkMessageHandlers::HandleMeteorStrikeComing(m_game, data); return true;
		case Notify::MeteorStrikeHit: NetworkMessageHandlers::HandleMeteorStrikeHit(m_game, data); return true;
		case Notify::cannot_construct: NetworkMessageHandlers::HandleCannotConstruct(m_game, data); return true;
		case Notify::TcLoc: NetworkMessageHandlers::HandleTCLoc(m_game, data); return true;
		case Notify::ConstructionPoint: NetworkMessageHandlers::HandleConstructionPoint(m_game, data); return true;

		// Map
		case Notify::MapStatusNext: NetworkMessageHandlers::HandleMapStatusNext(m_game, data); return true;
		case Notify::MapStatusLast: NetworkMessageHandlers::HandleMapStatusLast(m_game, data); return true;
		case Notify::LockedMap: NetworkMessageHandlers::HandleLockedMap(m_game, data); return true;
		case Notify::ShowMap: NetworkMessageHandlers::HandleShowMap(m_game, data); return true;

		// Crafting
		case Notify::CraftingSuccess: NetworkMessageHandlers::handle_crafting_success(m_game, data); return true;
		case Notify::CraftingFail: NetworkMessageHandlers::handle_crafting_fail(m_game, data); return true;
		case Notify::BuildItemSuccess: NetworkMessageHandlers::handle_build_item_success(m_game, data); return true;
		case Notify::BuildItemFail: NetworkMessageHandlers::handle_build_item_fail(m_game, data); return true;
		case Notify::PortionSuccess: NetworkMessageHandlers::handle_portion_success(m_game, data); return true;
		case Notify::PortionFail: NetworkMessageHandlers::handle_portion_fail(m_game, data); return true;
		case Notify::LowPortionSkill: NetworkMessageHandlers::handle_low_portion_skill(m_game, data); return true;
		case Notify::NoMatchingPortion: NetworkMessageHandlers::handle_no_matching_portion(m_game, data); return true;

		// Fish
		case Notify::FishChance: NetworkMessageHandlers::handle_fish_chance(m_game, data); return true;
		case Notify::EventFishMode: NetworkMessageHandlers::handle_event_fish_mode(m_game, data); return true;
		case Notify::FishCanceled: NetworkMessageHandlers::handle_fish_canceled(m_game, data); return true;
		case Notify::FishSuccess: NetworkMessageHandlers::handle_fish_success(m_game, data); return true;
		case Notify::FishFail: NetworkMessageHandlers::handle_fish_fail(m_game, data); return true;

		// Agriculture
		case Notify::AgricultureNoArea: NetworkMessageHandlers::HandleAgricultureNoArea(m_game, data); return true;
		case Notify::AgricultureSkillLimit: NetworkMessageHandlers::HandleAgricultureSkillLimit(m_game, data); return true;
		case Notify::NoMoreAgriculture: NetworkMessageHandlers::HandleNoMoreAgriculture(m_game, data); return true;
		
		// Angels
		case Notify::AngelFailed: NetworkMessageHandlers::HandleAngelFailed(m_game, data); return true;
		case Notify::AngelReceived: NetworkMessageHandlers::HandleAngelReceived(m_game, data); return true;
		case Notify::AngelicStats: NetworkMessageHandlers::HandleAngelicStats(m_game, data); return true;

		case Notify::Unknown0BEF: NetworkMessageHandlers::HandleCrashHandler(m_game, data); return true;
		case Notify::IpAccountInfo: NetworkMessageHandlers::HandleIpAccountInfo(m_game, data); return true;
		case Notify::RewardGold: NetworkMessageHandlers::HandleRewardGold(m_game, data); return true;
		case Notify::ServerShutdown: NetworkMessageHandlers::HandleServerShutdown(m_game, data); return true;

		// System (Generic)
		case Notify::WhetherChange: NetworkMessageHandlers::HandleWeatherChange(m_game, data); return true;
		case Notify::TimeChange: NetworkMessageHandlers::HandleTimeChange(m_game, data); return true;
		case Notify::NoticeMsg: NetworkMessageHandlers::HandleNoticeMsg(m_game, data); return true;
		case Notify::StatusText: NetworkMessageHandlers::HandleStatusText(m_game, data); return true;
		case Notify::ForceDisconn: NetworkMessageHandlers::HandleForceDisconn(m_game, data); return true;
		case Notify::SettingSuccess: NetworkMessageHandlers::HandleSettingSuccess(m_game, data); return true;
		case Notify::ServerChange: NetworkMessageHandlers::HandleServerChange(m_game, data); return true;
		case Notify::TotalUsers: NetworkMessageHandlers::HandleTotalUsers(m_game, data); return true;
		case Notify::ForceRecallTime: NetworkMessageHandlers::HandleForceRecallTime(m_game, data); return true;
		case Notify::NoRecall: NetworkMessageHandlers::HandleNoRecall(m_game, data); return true;
		case Notify::TeleportApproved: teleport_manager::get().on_auth_approved(); return true;
		case Notify::LoteryLost: NetworkMessageHandlers::HandleLoteryLost(m_game, data); return true;
		case Notify::NotFlagSpot: NetworkMessageHandlers::HandleNotFlagSpot(m_game, data); return true;
		case Notify::NpcTalk: NetworkMessageHandlers::HandleNpcTalk(m_game, data); return true;
		case Notify::TravelerLimitedLevel: NetworkMessageHandlers::HandleTravelerLimitedLevel(m_game, data); return true;
		case Notify::LimitedLevel: NetworkMessageHandlers::HandleLimitedLevel(m_game, data); return true;
		case Notify::ToBeRecalled: NetworkMessageHandlers::HandleToBeRecalled(m_game, data); return true;

#ifdef TESTER_ONLY
		// TESTER MENU — notification handlers (tester builds only)
		case Notify::TesterItemSearchResult: NetworkMessageHandlers::HandleTesterItemSearchResult(m_game, data); return true;
		case Notify::TesterMapListResult: NetworkMessageHandlers::HandleTesterMapListResult(m_game, data); return true;
#endif // TESTER_ONLY

		}
		return false;
	}

	return false;
}
