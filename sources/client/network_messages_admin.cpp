#include "game.h"
#include "network_message_manager.h"
#include "packet/shared_packets.h"
#include "dialog_box_ids.h"
#include "dialog_box_noticement.h"
#include "audio_manager.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace NetworkMessageHandlers {

void HandleCrashHandler(CGame* game, char* data)
{
	// 0x0BEF: Crash or closes the client? (Calls SE entry !)
	// I'm not sure at all of this function's result, so let's quit game...
	// empty handler - just acknowledge the message
}

void HandleIpAccountInfo(CGame* game, char* data)
{
	std::string temp;
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyIpAccountInfo>(
		data, sizeof(hb::net::PacketNotifyIpAccountInfo));
	if (!pkt) return;
	temp = pkt->text;
	game->add_event_list(temp.c_str());
}

void HandleRewardGold(CGame* game, char* data)
{
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyRewardGold>(
		data, sizeof(hb::net::PacketNotifyRewardGold));
	if (!pkt) return;
	game->m_player->m_reward_gold = pkt->gold;
}

void HandleServerShutdown(CGame* game, char* data)
{
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyServerShutdown>(
		data, sizeof(hb::net::PacketNotifyServerShutdown));
	if (!pkt) return;

	// Enable or update the noticement dialog
	if (!game->get_dialog_box_manager().is_enabled(DialogBoxId::Noticement))
		game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Noticement, pkt->mode, pkt->seconds, 0);

	// Pass shutdown info to the dialog (seconds + custom message)
	auto* dlg = game->get_dialog_box_manager().get_dialog_as<DialogBox_Noticement>(DialogBoxId::Noticement);
	if (dlg != nullptr)
	{
		dlg->m_mode = pkt->mode;
		dlg->m_countdown_seconds = pkt->seconds;
		dlg->set_shutdown_info(pkt->seconds, pkt->message);
	}

	audio_manager::get().play_game_sound(sound_type::effect, 27, 0);
}

} // namespace NetworkMessageHandlers
