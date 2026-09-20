#include "Game.h"
#include "FloatingTextManager.h"
#include "NetworkMessageManager.h"
#include "Packet/SharedPackets.h"
#ifdef TESTER_ONLY
// TESTER MENU — includes (tester builds only)
#include "DialogBox_ItemCreator.h"
#include "DialogBox_TesterMenu.h"
#endif // TESTER_ONLY
#include "lan_eng.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <format>
#include <string>
#include "Screen_OnGame.h"
#include "AudioManager.h"


using namespace hb::shared::action;
using namespace hb::shared::direction;

namespace NetworkMessageHandlers {
	void HandleKilled(CGame* game, char* data)
	{
		char attacker_name[21]{};
		game->m_player->m_Controller.set_command_available(false);
		game->m_player->m_Controller.set_command(Type::stop);
		game->m_player->m_hp = 0;
		game->m_player->m_Controller.set_command(-1);
		// Restart
		game->on_game()->m_item_using_status = false;
		game->clear_skill_using_status();
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyKilled>(
			data, sizeof(hb::net::PacketNotifyKilled));
		if (!pkt) return;
		memcpy(attacker_name, pkt->attacker_name, sizeof(pkt->attacker_name));
		game->add_event_list(NOTIFYMSG_KILLED1, 10);
		game->add_event_list(NOTIFYMSG_KILLED3, 10);
	}

	void HandlePKcaptured(CGame* game, char* data)
	{
		uint32_t exp, reward_gold;
		int     p_kcount, level;
		std::string txt;

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyPKcaptured>(
			data, sizeof(hb::net::PacketNotifyPKcaptured));
		if (!pkt) return;
		p_kcount = pkt->pk_count;
		level = pkt->victim_pk_count;
		std::string name(pkt->victim_name, strnlen(pkt->victim_name, sizeof(pkt->victim_name)));
		reward_gold = pkt->reward_gold;
		exp = pkt->exp;
		txt = std::format(NOTIFYMSG_PK_CAPTURED1, level, name, p_kcount);
		game->add_event_list(txt.c_str(), 10);
		if (exp > static_cast<uint32_t>(game->m_player->m_exp))
		{
			txt = std::format(EXP_INCREASED, exp - game->m_player->m_exp);
			game->add_event_list(txt.c_str(), 10);
			txt = std::format(NOTIFYMSG_PK_CAPTURED3, exp - game->m_player->m_exp);
			game->add_event_list(txt.c_str(), 10);
		}
	}

	void HandlePKpenalty(CGame* game, char* data)
	{
		uint32_t exp;
		int     p_kcount, str, vit, dex, iInt, mag, chr;
		std::string txt;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyPKpenalty>(
			data, sizeof(hb::net::PacketNotifyPKpenalty));
		if (!pkt) return;
		exp = pkt->exp;
		str = pkt->str;
		vit = pkt->vit;
		dex = pkt->dex;
		iInt = pkt->intel;
		mag = pkt->mag;
		chr = pkt->chr;
		p_kcount = pkt->pk_count;
		txt = std::format(NOTIFYMSG_PK_PENALTY1, p_kcount);
		game->add_event_list(txt.c_str(), 10);
		if (game->m_player->m_exp > exp)
		{
			txt = std::format(NOTIFYMSG_PK_PENALTY2, game->m_player->m_exp - exp);
			game->add_event_list(txt.c_str(), 10);
		}
		game->m_player->m_exp = exp;
		game->m_player->m_str = str;
		game->m_player->m_vit = vit;
		game->m_player->m_dex = dex;
		game->m_player->m_int = iInt;
		game->m_player->m_mag = mag;
		game->m_player->m_charisma = chr;
		game->m_player->m_pk_count = p_kcount;
	}

	void HandleEnemyKills(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyEnemyKills>(
			data, sizeof(hb::net::PacketNotifyEnemyKills));
		if (!pkt) return;
		game->m_player->m_enemy_kill_count = pkt->count;
	}

	void HandleContribution(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifySimpleInt>(
			data, sizeof(hb::net::PacketNotifySimpleInt));
		if (!pkt) return;
		game->m_player->m_contribution = pkt->value;
	}

#ifdef TESTER_ONLY
	// TESTER MENU — notification handlers (tester builds only)
	void HandleTesterItemSearchResult(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyTesterItemSearchResult>(
			data, sizeof(hb::net::PacketNotifyTesterItemSearchResult));
		if (!pkt) return;

		auto* dlg = dynamic_cast<DialogBox_ItemCreator*>(
			game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemCreator));
		if (dlg) dlg->receive_search_results(pkt);
	}

	void HandleTesterMapListResult(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyTesterMapListResult>(
			data, sizeof(hb::net::PacketNotifyTesterMapListResult));
		if (!pkt) return;

		auto* dlg = dynamic_cast<DialogBox_TesterMenu*>(
			game->get_dialog_box_manager().get_dialog_box(DialogBoxId::TesterMenu));
		if (dlg) dlg->receive_map_list(pkt);
	}
#endif // TESTER_ONLY

	void HandleEnemyKillReward(CGame* game, char* data)
	{
		uint32_t exp;
		std::string txt;

		int   enemy_kill_count, war_contribution;

		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyEnemyKillReward>(
			data, sizeof(hb::net::PacketNotifyEnemyKillReward));
		if (!pkt) return;

		exp = pkt->exp;
		enemy_kill_count = static_cast<int>(pkt->kill_count);
		std::string name(pkt->killer_name, strnlen(pkt->killer_name, sizeof(pkt->killer_name)));
		war_contribution = pkt->war_contribution;

		if (war_contribution > game->m_player->m_war_contribution && game->m_game_msg_list[21])
		{
			std::string warBuf;
			warBuf = std::format("{} +{}!", game->m_game_msg_list[21]->m_pMsg, war_contribution - game->m_player->m_war_contribution);
			game->set_top_msg(warBuf.c_str(), 5);
		}
		else if (war_contribution < game->m_player->m_war_contribution)
		{
		}
		game->m_player->m_war_contribution = war_contribution;

		txt = std::format(NOTIFYMSG_ENEMYKILL_REWARD1, name);
		game->add_event_list(txt.c_str(), 10);

		if (game->m_player->m_enemy_kill_count != enemy_kill_count)
		{
			if (game->m_player->m_enemy_kill_count > enemy_kill_count)
			{
				txt = std::format(NOTIFYMSG_ENEMYKILL_REWARD5, game->m_player->m_enemy_kill_count - enemy_kill_count);
				game->add_event_list(txt.c_str(), 10);
			}
			else
			{
				txt = std::format(NOTIFYMSG_ENEMYKILL_REWARD6, enemy_kill_count - game->m_player->m_enemy_kill_count);
				game->add_event_list(txt.c_str(), 10);
			}
		}

		game->m_player->m_exp = exp;
		game->m_player->m_enemy_kill_count = enemy_kill_count;
		audio_manager::get().play_game_sound(sound_type::effect, 23, 0);

		game->get_floating_text().remove_by_object_id(game->m_player->m_player_object_id);
		game->get_floating_text().add_notify_text(notify_text_type::enemy_kill, "Enemy Kill!", game->m_cur_time,
			game->m_player->m_player_object_id, game->m_map_data.get());
		game->create_screen_shot();
	}

	void HandleGlobalAttackMode(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyGlobalAttackMode>(
			data, sizeof(hb::net::PacketNotifyGlobalAttackMode));
		if (!pkt) return;
		switch (pkt->mode) {
		case 0:
			game->add_event_list(NOTIFYMSG_GLOBAL_ATTACK_MODE1, 10);
			game->add_event_list(NOTIFYMSG_GLOBAL_ATTACK_MODE2, 10);
			break;

		case 1:
			game->add_event_list(NOTIFYMSG_GLOBAL_ATTACK_MODE3, 10);
			break;
		}
	}

	void HandleDamageMove(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyDamageMove>(
			data, sizeof(hb::net::PacketNotifyDamageMove));
		if (!pkt) return;
		game->m_player->m_damage_move = pkt->dir;
		game->m_player->m_damage_move_amount = pkt->amount;
	}

	void HandleObserverMode(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyObserverMode>(
			data, sizeof(hb::net::PacketNotifyObserverMode));
		if (!pkt) return;
		if (pkt->enabled == 1)
		{
			game->add_event_list(NOTIFY_MSG_HANDLER40); // "Observer Mode On. Press 'SHIFT + ESC' to Log Out..."
			game->on_game()->m_is_observer_mode = true;
			game->m_observer_cam_time = GameClock::get_time_ms();
			std::string name = game->m_player->m_player_name;
			game->m_map_data->set_owner(game->m_player->m_player_object_id, -1, -1, 0, direction{}, hb::shared::entity::PlayerAppearance{}, hb::shared::entity::PlayerStatus{}, name, 0, 0, 0, 0);
		}
		else
		{
			game->add_event_list(NOTIFY_MSG_HANDLER41); // "Observer Mode Off"
			game->on_game()->m_is_observer_mode = false;
			game->m_map_data->set_owner(game->m_player->m_player_object_id, game->m_player->m_player_x, game->m_player->m_player_y, game->m_player->m_player_type, game->m_player->m_player_dir, game->m_player->m_playerAppearance, game->m_player->m_playerStatus, game->m_player->m_player_name, Type::stop, 0, 0, 0);
		}
	}

	void HandleSuperAttackLeft(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifySuperAttackLeft>(
			data, sizeof(hb::net::PacketNotifySuperAttackLeft));
		if (!pkt) return;
		game->m_player->m_super_attack_left = pkt->left;
	}

	void HandleSafeAttackMode(CGame* game, char* data)
	{
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifySafeAttackMode>(
			data, sizeof(hb::net::PacketNotifySafeAttackMode));
		if (!pkt) return;
		switch (pkt->enabled) {
		case 1:
			if (!game->m_player->m_is_safe_attack_mode) game->add_event_list(NOTIFY_MSG_HANDLER50, 10);
			game->m_player->m_is_safe_attack_mode = true;
			break;
		case 0:
			if (game->m_player->m_is_safe_attack_mode) game->add_event_list(NOTIFY_MSG_HANDLER51, 10);
			game->m_player->m_is_safe_attack_mode = false;
			break;
		}
	}

	void HandleSpellInterrupted(CGame* game, char* data)
	{
		if (game->m_player->m_Controller.get_command() == Type::Magic)
			game->m_player->m_Controller.set_command(Type::stop);
		game->on_game()->m_is_get_pointing_mode = false;
		game->on_game()->m_point_command_type = -1;
	}
}

