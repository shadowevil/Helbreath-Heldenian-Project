#include "Game.h"
#include "FloatingTextManager.h"
#include "NetworkMessageManager.h"
#include "Packet/SharedPackets.h"
#include "DialogBox_Skill.h"
#include "lan_eng.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <format>
#include <string>
#include "Screen_OnGame.h"

namespace NetworkMessageHandlers {
	void HandleDownSkillIndexSet(CGame* game, char* data)
	{
		short skill_index;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyDownSkillIndexSet>(
			data, sizeof(hb::net::PacketNotifyDownSkillIndexSet));
		if (!pkt) return;
		skill_index = static_cast<short>(pkt->skill_index);
		game->on_game()->m_down_skill_index = skill_index;
		auto* skill_dlg = game->get_dialog_box_manager().get_dialog_as<DialogBox_Skill>(DialogBoxId::Skill);
		if (skill_dlg) skill_dlg->m_is_down_skill_pending = false;
	}

	void HandleMagicStudyFail(CGame* game, char* data)
	{
		char magic_num, name[31]{}, fail_code;
		std::string txt;
		int cost, req_int;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyMagicStudyFail>(
			data, sizeof(hb::net::PacketNotifyMagicStudyFail));
		if (!pkt) return;
		fail_code = static_cast<char>(pkt->result);
		magic_num = static_cast<char>(pkt->magic_id);
		memcpy(name, pkt->magic_name, 30);
		cost = pkt->cost;
		req_int = pkt->req_int;

		if (cost > 0)
		{
			txt = std::format(NOTIFYMSG_MAGICSTUDY_FAIL1, name);
			game->add_event_list(txt.c_str(), 10);
		}
		else
		{
			txt = std::format(NOTIFYMSG_MAGICSTUDY_FAIL2, name);
			game->add_event_list(txt.c_str(), 10);
			txt = std::format(NOTIFYMSG_MAGICSTUDY_FAIL3, req_int);
			game->add_event_list(txt.c_str(), 10);
		}
	}

	void HandleMagicStudySuccess(CGame* game, char* data)
	{
		char magic_num, name[31]{};
		std::string txt;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyMagicStudySuccess>(
			data, sizeof(hb::net::PacketNotifyMagicStudySuccess));
		if (!pkt) return;
		magic_num = static_cast<char>(pkt->magic_id);
		if (magic_num < 0 || magic_num >= hb::shared::limits::MaxMagicType) return;
		game->m_player->m_magic_mastery[magic_num] = 1;
	  // Magic learned - affects magic list
		memcpy(name, pkt->magic_name, 30);
		txt = std::format(NOTIFYMSG_MAGICSTUDY_SUCCESS1, name);
		game->add_event_list(txt.c_str(), 10);
		game->play_game_sound('E', 23, 0);
	}

	void HandleSkillTrainSuccess(CGame* game, char* data)
	{
		char skill_num, skill_level;
		std::string temp;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifySkillTrainSuccess>(
			data, sizeof(hb::net::PacketNotifySkillTrainSuccess));
		if (!pkt) return;
		skill_num = static_cast<char>(pkt->skill_num);
		if (skill_num < 0 || skill_num >= hb::shared::limits::MaxSkillType) return;
		if (!game->m_skill_cfg_list[skill_num]) return;
		skill_level = static_cast<char>(pkt->skill_level);
		temp = std::format(NOTIFYMSG_SKILL_TRAIN_SUCCESS1, game->m_skill_cfg_list[skill_num]->m_name, static_cast<int>(skill_level));
		game->add_event_list(temp.c_str(), 10);
		game->m_skill_cfg_list[skill_num]->m_level = skill_level;
		game->m_player->m_skill_mastery[skill_num] = static_cast<unsigned char>(skill_level);
		game->play_game_sound('E', 23, 0);
	}

	void HandleSkill(CGame* game, char* data)
	{
		short skill_index, value;
		std::string txt;

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifySkill>(
			data, sizeof(hb::net::PacketNotifySkill));
		if (!pkt) return;
		skill_index = static_cast<short>(pkt->skill_index);
		if (skill_index < 0 || skill_index >= hb::shared::limits::MaxSkillType) return;
		if (!game->m_skill_cfg_list[skill_index]) return;
		value = static_cast<short>(pkt->skill_value);
		game->get_floating_text().remove_by_object_id(game->m_player->m_player_object_id);
		if (game->m_skill_cfg_list[skill_index]->m_level < value)
		{
			txt = std::format(NOTIFYMSG_SKILL1, game->m_skill_cfg_list[skill_index]->m_name, value - game->m_skill_cfg_list[skill_index]->m_level);
			game->add_event_list(txt.c_str(), 10);
			game->play_game_sound('E', 23, 0);
			txt = std::format("{} +{}%", game->m_skill_cfg_list[skill_index]->m_name, value - game->m_skill_cfg_list[skill_index]->m_level);
			game->get_floating_text().add_notify_text(notify_text_type::skill_change, txt, game->m_cur_time,
				game->m_player->m_player_object_id, game->m_map_data.get());
		}
		else if (game->m_skill_cfg_list[skill_index]->m_level > value) {
			txt = std::format(NOTIFYMSG_SKILL2, game->m_skill_cfg_list[skill_index]->m_name, game->m_skill_cfg_list[skill_index]->m_level - value);
			game->add_event_list(txt.c_str(), 10);
			game->play_game_sound('E', 24, 0);
			txt = std::format("{} -{}%", game->m_skill_cfg_list[skill_index]->m_name, value - game->m_skill_cfg_list[skill_index]->m_level);
			game->get_floating_text().add_notify_text(notify_text_type::skill_change, txt, game->m_cur_time,
				game->m_player->m_player_object_id, game->m_map_data.get());
		}
		game->m_skill_cfg_list[skill_index]->m_level = value;
		game->m_player->m_skill_mastery[skill_index] = static_cast<unsigned char>(value);
	}

	void HandleSkillUsingEnd(CGame* game, char* data)
	{
		uint16_t result;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifySkillUsingEnd>(
			data, sizeof(hb::net::PacketNotifySkillUsingEnd));
		if (!pkt) return;
		result = pkt->result;
		switch (result) {
		case 0:
			game->add_event_list(NOTIFYMSG_SKILL_USINGEND1, 10);
			break;
		case 1:
			game->add_event_list(NOTIFYMSG_SKILL_USINGEND2, 10);
			break;
		}
		game->on_game()->m_skill_using_status = false;
	}

	void HandleMagicEffectOn(CGame* game, char* data)
	{
		short  magic_type, magic_effect, owner_h;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyMagicEffect>(
			data, sizeof(hb::net::PacketNotifyMagicEffect));
		if (!pkt) return;
		magic_type = static_cast<short>(pkt->magic_type);
		magic_effect = static_cast<short>(pkt->effect);
		owner_h = static_cast<short>(pkt->owner);
		switch (magic_type) {
		case hb::shared::magic::Protect:
			switch (magic_effect) {
			case 1: // "You are completely protected from arrows!"
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_ON1, 10);
				break;
			case 2: // "You are protected from magic!"
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_ON2, 10);
				break;
			case 3: // "Defense ratio increased by a magic shield!"
			case 4: // "Defense ratio increased by a magic shield!"
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_ON3, 10);
				break;
			case 5: // "You are completely protected from magic!"
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_ON14, 10);
				break;
			}
			break;

		case hb::shared::magic::HoldObject:
			switch (magic_effect) {
			case 1: // "You were bounded by a Hold Person spell! Unable to move!"
				game->m_player->m_paralyze = true;
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_ON4, 10);
				break;
			case 2: // "You were bounded by a Paralysis spell! Unable to move!"
				game->m_player->m_paralyze = true;
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_ON5, 10);
				break;
			}
			break;

		case hb::shared::magic::Invisibility:
			switch (magic_effect) {
			case 1: // "You are now invisible, no one can see you!"
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_ON6, 10);
				break;
			}
			break;

		case hb::shared::magic::Confuse:
			switch (magic_effect) {
			case 1:	// Confuse Language "No one understands you because of language confusion magic!"
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_ON7, 10);
				break;

			case 2: // Confusion "Confusion magic casted, impossible to determine player allegience."
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_ON8, 10);
				game->m_player->m_is_confusion = true;
				break;

			case 3:	// Illusion "Illusion magic casted, impossible to tell who is who!"
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_ON9, 10);
				game->set_ilusion_effect(owner_h);
				break;

			case 4:	// IllusionMouvement "You are thrown into confusion, and you are flustered yourself." // snoopy
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_ON15, 10);
				game->m_illusion_mvt = true;
				break;
			}
			break;

		case hb::shared::magic::Poison:
			game->add_event_list(NOTIFYMSG_MAGICEFFECT_ON10, 10);
			game->m_player->m_is_poisoned = true;
			break;

		case hb::shared::magic::Berserk:
			switch (magic_effect) {
			case 1:
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_ON11, 10);
				break;
			}
			break;

		case hb::shared::magic::Polymorph:
			switch (magic_effect) {
			case 1:
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_ON12, 10);
				break;
			}
			break;

		case hb::shared::magic::Ice:
			game->add_event_list(NOTIFYMSG_MAGICEFFECT_ON13, 10);
			break;
		}
	}

	void HandleMagicEffectOff(CGame* game, char* data)
	{
		short  magic_type, magic_effect;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyMagicEffect>(
			data, sizeof(hb::net::PacketNotifyMagicEffect));
		if (!pkt) return;
		magic_type = static_cast<short>(pkt->magic_type);
		magic_effect = static_cast<short>(pkt->effect);
		switch (magic_type) {
		case hb::shared::magic::Protect:
			switch (magic_effect) {
			case 1: // "Protection from arrows has vanished."
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_OFF1, 10);
				break;
			case 2:	// "Protection from magic has vanished."
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_OFF2, 10);
				break;
			case 3:	// "Defense shield effect has vanished."
			case 4:	// "Defense shield effect has vanished."
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_OFF3, 10);
				break;
			case 5:	// "Absolute Magic Protection has been vanished."
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_OFF14, 10);
				break;
			}
			break;

		case hb::shared::magic::HoldObject:
			switch (magic_effect) {
			case 1:	// "Hold person magic effect has vanished."
				game->m_player->m_paralyze = false;
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_OFF4, 10);
				break;

			case 2:	// "Paralysis magic effect has vanished."
				game->m_player->m_paralyze = false;
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_OFF5, 10);
				break;
			}
			break;

		case hb::shared::magic::Invisibility:
			switch (magic_effect) {
			case 1:	// "Invisibility magic effect has vanished."
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_OFF6, 10);
				break;
			}
			break;

		case hb::shared::magic::Confuse:
			switch (magic_effect) {
			case 1:	// "Language confuse magic effect has vanished."
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_OFF7, 10);
				break;
			case 2:	// "Confusion magic has vanished."
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_OFF8, 10);
				game->m_player->m_is_confusion = false;
				break;
			case 3:	// "Illusion magic has vanished."
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_OFF9, 10);
				game->on_game()->m_ilusion_owner_h = 0;
				break;
			case 4:	// "At last, you gather your senses." // snoopy
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_OFF15, 10);
				game->m_illusion_mvt = false;
				break;
			}
			break;

		case hb::shared::magic::Poison:
			game->m_player->m_is_poisoned = false;
			break;

		case hb::shared::magic::Berserk:
			switch (magic_effect) {
			case 1:
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_OFF11, 10);
				break;
			}
			break;

		case hb::shared::magic::Polymorph:
			switch (magic_effect) {
			case 1:
				game->add_event_list(NOTIFYMSG_MAGICEFFECT_OFF12, 10);
				break;
			}
			break;

		case hb::shared::magic::Ice:
			game->add_event_list(NOTIFYMSG_MAGICEFFECT_OFF13, 10);
			break;
		}
	}

	void HandleSpellSkill(CGame* game, char* data)
	{
		int i;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifySpellSkill>(
			data, sizeof(hb::net::PacketNotifySpellSkill));
		if (!pkt) return;
		for (i = 0; i < hb::shared::limits::MaxMagicType; i++) {
			game->m_player->m_magic_mastery[i] = pkt->magic_mastery[i];
		}
		for (i = 0; i < hb::shared::limits::MaxSkillType; i++) {
			game->m_player->m_skill_mastery[i] = pkt->skill_mastery[i];
			if (game->m_skill_cfg_list[i] != 0)
				game->m_skill_cfg_list[i]->m_level = pkt->skill_mastery[i];
		}
	}

	void HandleStateChangeSuccess(CGame* game, char* data)
	{
		int i;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyStateChangeSuccess>(
			data, sizeof(hb::net::PacketNotifyStateChangeSuccess));
		if (!pkt) return;
		for (i = 0; i < hb::shared::limits::MaxMagicType; i++) {
			game->m_player->m_magic_mastery[i] = pkt->magic_mastery[i];
		}
		for (i = 0; i < hb::shared::limits::MaxSkillType; i++) {
			game->m_player->m_skill_mastery[i] = pkt->skill_mastery[i];
			if (game->m_skill_cfg_list[i] != 0)
				game->m_skill_cfg_list[i]->m_level = pkt->skill_mastery[i];
		}
		// Calculate majestic cost before applying (m_wLU_* are negative for reductions)
		int total_reduction = -(game->m_player->m_lu_str + game->m_player->m_lu_vit +
			game->m_player->m_lu_dex + game->m_player->m_lu_int +
			game->m_player->m_lu_mag + game->m_player->m_lu_char);
		int majestic_cost = total_reduction / 3;

		// Apply pending stat changes (adds negative values = reduces stats)
		game->m_player->m_str += game->m_player->m_lu_str;
		game->m_player->m_vit += game->m_player->m_lu_vit;
		game->m_player->m_dex += game->m_player->m_lu_dex;
		game->m_player->m_int += game->m_player->m_lu_int;
		game->m_player->m_mag += game->m_player->m_lu_mag;
		game->m_player->m_charisma += game->m_player->m_lu_char;
		game->m_player->m_lu_point = (game->m_player->m_level - 1) * 3 - ((game->m_player->m_str + game->m_player->m_vit + game->m_player->m_dex + game->m_player->m_int + game->m_player->m_mag + game->m_player->m_charisma) - 70);
		game->on_game()->m_gizon_item_upgrade_left -= majestic_cost;
		game->m_player->m_lu_str = game->m_player->m_lu_vit = game->m_player->m_lu_dex = game->m_player->m_lu_int = game->m_player->m_lu_mag = game->m_player->m_lu_char = 0;
		game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::ChangeStatsMajestic);
		game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::LevelUpSetting, 0, 0, 0);
		game->add_event_list("Your stat has been changed.", 10);
	}

	void HandleStateChangeFailed(CGame* game, char* data)
	{
		game->m_player->m_lu_str = game->m_player->m_lu_vit = game->m_player->m_lu_dex = game->m_player->m_lu_int = game->m_player->m_lu_mag = game->m_player->m_lu_char = 0;
		game->m_player->m_lu_point = (game->m_player->m_level - 1) * 3 - ((game->m_player->m_str + game->m_player->m_vit + game->m_player->m_dex + game->m_player->m_int + game->m_player->m_mag + game->m_player->m_charisma) - 70);
		game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::ChangeStatsMajestic);
		game->add_event_list("Your stat has not been changed.", 10);
	}

	void HandleSettingFailed(CGame* game, char* data)
	{
		game->add_event_list("Your stat has not been changed.", 10);
		game->m_player->m_lu_str = game->m_player->m_lu_vit = game->m_player->m_lu_dex = game->m_player->m_lu_int = game->m_player->m_lu_mag = game->m_player->m_lu_char = 0;
		game->m_player->m_lu_point = (game->m_player->m_level - 1) * 3 - ((game->m_player->m_str + game->m_player->m_vit + game->m_player->m_dex + game->m_player->m_int + game->m_player->m_mag + game->m_player->m_charisma) - 70);
	}

	void HandleSpecialAbilityStatus(CGame* game, char* data)
	{
		std::string G_cTxt;
		short v1, v2, v3;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifySpecialAbilityStatus>(
			data, sizeof(hb::net::PacketNotifySpecialAbilityStatus));
		if (!pkt) return;
		v1 = pkt->status_type;
		v2 = pkt->ability_type;
		v3 = pkt->seconds_left;

		if (v1 == 1) // Use SA
		{
			game->play_game_sound('E', 35, 0);
			game->add_event_list(NOTIFY_MSG_HANDLER4, 10); 
			switch (v2) {
			case 1: G_cTxt = std::format(NOTIFY_MSG_HANDLER5, v3); break;
			case 2: G_cTxt = std::format(NOTIFY_MSG_HANDLER6, v3); break;
			case 3: G_cTxt = std::format(NOTIFY_MSG_HANDLER7, v3); break;
			case 4: G_cTxt = std::format(NOTIFY_MSG_HANDLER8, v3); break;
			case 5: G_cTxt = std::format(NOTIFY_MSG_HANDLER9, v3); break;
			case 50:G_cTxt = std::format(NOTIFY_MSG_HANDLER10, v3); break;
			case 51:G_cTxt = std::format(NOTIFY_MSG_HANDLER11, v3); break;
			case 52:G_cTxt = std::format(NOTIFY_MSG_HANDLER12, v3); break;
			case 55: 
				if (v3 > 90)
					G_cTxt = std::format("You cast a powerfull incantation, you can't use it again before {} minutes.", v3 / 60);
				else
					G_cTxt = std::format("You cast a powerfull incantation, you can't use it again before {} seconds.", v3);
				break;
			}
			game->add_event_list(G_cTxt.c_str(), 10);
		}
		else if (v1 == 2) // Finished using
		{
			if (game->m_player->m_special_ability_type != static_cast<int>(v2))
			{
				game->play_game_sound('E', 34, 0);
				game->add_event_list(NOTIFY_MSG_HANDLER13, 10);
				if (v3 >= 60)
				{
					switch (v2) {
					case 1: G_cTxt = std::format(NOTIFY_MSG_HANDLER14, v3 / 60); game->add_event_list(G_cTxt.c_str(), 10); break;
					case 2: G_cTxt = std::format(NOTIFY_MSG_HANDLER15, v3 / 60); game->add_event_list(G_cTxt.c_str(), 10); break;
					case 3: G_cTxt = std::format(NOTIFY_MSG_HANDLER16, v3 / 60); game->add_event_list(G_cTxt.c_str(), 10); break;
					case 4: G_cTxt = std::format(NOTIFY_MSG_HANDLER17, v3 / 60); game->add_event_list(G_cTxt.c_str(), 10); break;
					case 5: G_cTxt = std::format(NOTIFY_MSG_HANDLER18, v3 / 60); game->add_event_list(G_cTxt.c_str(), 10); break;
					case 50:G_cTxt = std::format(NOTIFY_MSG_HANDLER19, v3 / 60); game->add_event_list(G_cTxt.c_str(), 10); break;
					case 51:G_cTxt = std::format(NOTIFY_MSG_HANDLER20, v3 / 60); game->add_event_list(G_cTxt.c_str(), 10); break;
					case 52:G_cTxt = std::format(NOTIFY_MSG_HANDLER21, v3 / 60); game->add_event_list(G_cTxt.c_str(), 10); break;
					}
				}
				else
				{
					switch (v2) {
					case 1: G_cTxt = std::format(NOTIFY_MSG_HANDLER22, v3); game->add_event_list(G_cTxt.c_str(), 10); break;
					case 2: G_cTxt = std::format(NOTIFY_MSG_HANDLER23, v3); game->add_event_list(G_cTxt.c_str(), 10); break;
					case 3: G_cTxt = std::format(NOTIFY_MSG_HANDLER24, v3); game->add_event_list(G_cTxt.c_str(), 10); break;
					case 4: G_cTxt = std::format(NOTIFY_MSG_HANDLER25, v3); game->add_event_list(G_cTxt.c_str(), 10); break;
					case 5: G_cTxt = std::format(NOTIFY_MSG_HANDLER26, v3); game->add_event_list(G_cTxt.c_str(), 10); break;
					case 50:G_cTxt = std::format(NOTIFY_MSG_HANDLER27, v3); game->add_event_list(G_cTxt.c_str(), 10); break;
					case 51:G_cTxt = std::format(NOTIFY_MSG_HANDLER28, v3); game->add_event_list(G_cTxt.c_str(), 10); break;
					case 52:G_cTxt = std::format(NOTIFY_MSG_HANDLER29, v3); game->add_event_list(G_cTxt.c_str(), 10); break;
					}
				}
			}
			game->m_player->m_special_ability_type = static_cast<int>(v2);
			game->on_game()->m_special_ability_setting_time = game->m_cur_time;
			game->m_player->m_special_ability_time_left_sec = static_cast<int>(v3);
		}
		else if (v1 == 3)  // End of using time
		{
			game->m_player->m_is_special_ability_enabled = false;
			game->on_game()->m_special_ability_setting_time = game->m_cur_time;
			if (v3 == 0)
			{
				game->m_player->m_special_ability_time_left_sec = 1200;
				game->add_event_list(NOTIFY_MSG_HANDLER30, 10);
			}
			else
			{
				game->m_player->m_special_ability_time_left_sec = static_cast<int>(v3);
				if (v3 > 90)
					G_cTxt = std::format("Special ability has run out! Will be available in {} minutes.", v3 / 60);
				else G_cTxt = std::format("Special ability has run out! Will be available in {} seconds.", v3);
				game->add_event_list(G_cTxt.c_str(), 10);
			}
		}
		else if (v1 == 4) // Unequiped the SA item
		{
			game->add_event_list(NOTIFY_MSG_HANDLER31, 10);
			game->m_player->m_special_ability_type = 0;
		}
		else if (v1 == 5) // Angel
		{
			game->play_game_sound('E', 52, 0); 
		}
	}

	void HandleSpecialAbilityEnabled(CGame* game, char* data)
	{
		if (game->m_player->m_is_special_ability_enabled == false) {
			game->play_game_sound('E', 30, 5);
			game->add_event_list(NOTIFY_MSG_HANDLER32, 10);
		}
		game->m_player->m_is_special_ability_enabled = true;
	}

	void HandleSkillTrainFail(CGame* game, char* data)
	{
		game->add_event_list("You failed to train skill.", 10);
		game->play_game_sound('E', 24, 0);
	}
}
