#include "Game.h"
#include "NetworkMessageManager.h"
#include "Packet/SharedPackets.h"
#include "lan_eng.h"
#include "DialogBoxIDs.h"
#include "DialogBox_Bank.h"
#include <cstring>
#include <cstdio>
#include <format>
#include <string>


namespace NetworkMessageHandlers {
	void HandleItemToBank(CGame* game, char* data)
	{
		int index;
		uint64_t count;
		char  name[hb::shared::limits::ItemNameLen]{}, item_type, equip_pos, gender_limit, item_color;
		bool  is_equipped;
		short level_limit, item_effect_value2, item_spec_effect_value2;
		uint16_t weight, cur_durability;
		std::string txt;
		const auto* pkt = hb::net::PacketCast<hb::net::PacketNotifyItemToBank>(
			data, sizeof(hb::net::PacketNotifyItemToBank));
		if (!pkt) return;

		index = static_cast<int>(pkt->bank_index);
		if (index < 0 || index >= hb::shared::limits::MaxBankItems) return;
		memcpy(name, pkt->name, sizeof(pkt->name));
		count = pkt->count;
		item_type = static_cast<char>(pkt->item_type);
		equip_pos = static_cast<char>(pkt->equip_pos);
		is_equipped = (pkt->is_equipped != 0);
		level_limit = static_cast<short>(pkt->level_limit);
		gender_limit = static_cast<char>(pkt->gender_limit);
		cur_durability = pkt->cur_durability;
		weight = pkt->weight;
		item_color = static_cast<char>(pkt->item_color);
		item_effect_value2 = static_cast<short>(pkt->item_effect_value2);
		item_spec_effect_value2 = static_cast<short>(pkt->spec_effect_value2);

		std::string str1;


		char str2[64], str3[64];
		str1 = name;
		str2[0] = 0;
		str3[0] = 0;

		if (game->m_player->m_bank_list[index] == 0)
		{
			game->m_player->m_bank_list[index] = std::make_unique<CItem>();
			game->m_player->m_bank_list[index]->m_id_num = static_cast<short>(pkt->item_id);
			game->m_player->m_bank_list[index]->m_instance.count = count;
			game->m_player->m_bank_list[index]->m_instance.cur_durability = cur_durability;
			game->m_player->m_bank_list[index]->m_instance.item_color = item_color;
			game->m_player->m_bank_list[index]->load_attributes_from(*pkt);
			game->m_player->m_bank_list[index]->m_instance.special_effect_value2 = item_spec_effect_value2;

			if (count == 1) txt = std::format(NOTIFYMSG_ITEMTOBANK3, str1.c_str());
			else txt = std::format(NOTIFYMSG_ITEMTOBANK2, count, str1.c_str());

			if (game->get_dialog_box_manager().is_enabled(DialogBoxId::Bank) == true)
				{
				auto* bank_dlg = game->get_dialog_box_manager().get_dialog_as<DialogBox_Bank>(DialogBoxId::Bank);
				if (bank_dlg) bank_dlg->m_scroll_offset = hb::shared::limits::MaxBankItems - 12;
			}
			game->add_event_list(txt.c_str(), 10);
		}
		else if (pkt->is_new == 0)
		{
			// Update existing bank item in-place (e.g., hero item sex swap)
			game->m_player->m_bank_list[index]->m_id_num = static_cast<short>(pkt->item_id);
			game->m_player->m_bank_list[index]->m_instance.count = count;
			game->m_player->m_bank_list[index]->m_instance.cur_durability = cur_durability;
			game->m_player->m_bank_list[index]->m_instance.item_color = item_color;
			game->m_player->m_bank_list[index]->load_attributes_from(*pkt);
			game->m_player->m_bank_list[index]->m_instance.special_effect_value2 = item_spec_effect_value2;
		}
	}

	void HandleCannotItemToBank(CGame* game, char* data)
	{
		game->add_event_list(NOTIFY_MSG_HANDLER63, 10);
	}
} // namespace NetworkMessageHandlers
