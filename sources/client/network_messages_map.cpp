#include "game.h"
#include "network_message_manager.h"
#include "packet/shared_packets.h"
#include "lan_eng.h"
#include "dialog_box_ids.h"
#include "dialog_box_map.h"
#include "audio_manager.h"
#include <cstdio>
#include <cstring>
#include <format>
#include <string>

namespace NetworkMessageHandlers {
	void HandleMapStatusNext(CGame* game, char* data)
	{
		game->add_map_status_info(data, false);
	}

	void HandleMapStatusLast(CGame* game, char* data)
	{
		game->add_map_status_info(data, true);
	}

	void HandleLockedMap(CGame* game, char* data)
	{
		int v1;
		char temp[120], txt[120]{};
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyLockedMap>(
			data, sizeof(hb::net::PacketNotifyLockedMap));
		if (!pkt) return;
		v1 = pkt->seconds_left;
		std::memset(temp, 0, sizeof(temp));
		memcpy(txt, pkt->map_name, sizeof(pkt->map_name));

		game->get_official_map_name(txt, temp);
		std::string msgBuf;
		msgBuf = std::format(NOTIFY_MSG_HANDLER3, v1, temp);
		game->set_top_msg(msgBuf.c_str(), 10);
		audio_manager::get().play_game_sound(sound_type::effect, 25, 0, 0);
	}

	void HandleShowMap(CGame* game, char* data)
	{
		uint16_t w1, w2;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyShowMap>(
			data, sizeof(hb::net::PacketNotifyShowMap));
		if (!pkt) return;
		w1 = pkt->map_id;
		w2 = pkt->map_type;
		auto& dialogs = game->get_dialog_box_manager();
		if (w2 != 0)
		{
			dialogs.enable_dialog_box(DialogBoxId::Map, 0, w1, 0);
			return;
		}
		// map_type 0: the server has no frame for this map; the client's map kit may still have it. Check
		// first: an already open dialog stays enabled when on_enable refuses, hiding the no-map message.
		if (DialogBox_Map::kit_has_map(game->m_map_name))
			dialogs.enable_dialog_box(DialogBoxId::Map, 0, w1, 0);
		if (!dialogs.is_enabled(DialogBoxId::Map))
			game->add_event_list(NOTIFYMSG_SHOW_MAP1, 10);
	}
} // namespace NetworkMessageHandlers
