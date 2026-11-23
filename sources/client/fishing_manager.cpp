// fishing_manager.cpp: Client-side fishing network message handlers.
// Extracted from network_messages_fish.cpp (Phase B1).

#include "fishing_manager.h"
#include "game.h"
#include "packet/shared_packets.h"
#include "lan_eng.h"
#include "dialog_box_ids.h"
#include "dialog_box_fishing.h"
#include "audio_manager.h"
#include <cstdio>
#include <cstring>

void fishing_manager::handle_fish_chance(char* data)
{
	if (!m_game) return;
	int fish_chance;
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyFishChance>(
		data, sizeof(hb::net::PacketNotifyFishChance));
	if (!pkt) return;
	fish_chance = pkt->chance;
	auto* fish_dlg = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_Fishing>(DialogBoxId::Fishing);
	if (fish_dlg) fish_dlg->m_catch_chance = fish_chance;
}

void fishing_manager::handle_event_fish_mode(char* data)
{
	if (!m_game) return;
	char name[hb::shared::limits::ItemNameLen]{};
	uint16_t price;

	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyEventFishMode>(
		data, sizeof(hb::net::PacketNotifyEventFishMode));
	if (!pkt) return;

	price = pkt->price;

	static_assert(sizeof(pkt->name) <= sizeof(name), "Packet name field exceeds local buffer");
	memcpy(name, pkt->name, sizeof(pkt->name));

	m_game->get_dialog_box_manager().enable_dialog_box(DialogBoxId::Fishing, 0, 0, price, name);
	// m_v3/m_v4 were unused sprite fields — removed

	m_game->add_event_list(NOTIFYMSG_EVENTFISHMODE1, 10);
}

void fishing_manager::handle_fish_canceled(char* data)
{
	if (!m_game) return;
	const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyFishCanceled>(
		data, sizeof(hb::net::PacketNotifyFishCanceled));
	if (!pkt) return;
	switch (pkt->reason) {
	case 0:
		m_game->add_event_list(NOTIFY_MSG_HANDLER52, 10);
		m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Fishing);
		break;
	case 1:
		m_game->add_event_list(NOTIFY_MSG_HANDLER53, 10);
		m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Fishing);
		break;
	case 2:
		m_game->add_event_list(NOTIFY_MSG_HANDLER54, 10);
		m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Fishing);
		break;
	}
}

void fishing_manager::handle_fish_success(char* data)
{
	if (!m_game) return;
	m_game->add_event_list(NOTIFY_MSG_HANDLER55, 10);
	audio_manager::get().play_game_sound(sound_type::effect, 23, 5);
	audio_manager::get().play_game_sound(sound_type::effect, 17, 5);
	switch (m_game->m_player->m_player_type) {
	case 1:
	case 2:
	case 3:
		audio_manager::get().play_game_sound(sound_type::character, 21, 0);
		break;
	case 4:
	case 5:
	case 6:
		audio_manager::get().play_game_sound(sound_type::character, 22, 0);
		break;
	}
}

void fishing_manager::handle_fish_fail(char* data)
{
	if (!m_game) return;
	m_game->add_event_list(NOTIFY_MSG_HANDLER56, 10);
	audio_manager::get().play_game_sound(sound_type::effect, 24, 5);
}
