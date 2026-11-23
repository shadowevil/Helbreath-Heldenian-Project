#include "game.h"
#include "inventory_manager.h"
#include "network_message_manager.h"
#include "packet/shared_packets.h"
#include "lan_eng.h"
#include "dialog_box_ids.h"
#include "dialog_box_exchange.h"
#include "audio_manager.h"
#include <cstring>
#include <cstdio>

namespace NetworkMessageHandlers {
	void HandleExchangeItemComplete(CGame* game, char* data)
	{
		game->add_event_list(NOTIFYMSG_EXCHANGEITEM_COMPLETE1, 10);
		game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Exchange);
		//Snoopy: MultiTrade
		game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::ConfirmExchange);
		audio_manager::get().play_game_sound(sound_type::effect, 23, 5);
	}

	void HandleCancelExchangeItem(CGame* game, char* data)
	{
		audio_manager::get().play_game_sound(sound_type::effect, 24, 5);
		game->add_event_list(NOTIFYMSG_CANCEL_EXCHANGEITEM1, 10);
		game->add_event_list(NOTIFYMSG_CANCEL_EXCHANGEITEM2, 10);

		// Explicitly clear item disabled flags before disabling dialogs.
		// disable_dialog_box(Exchange) also clears these, but if exchange info
		// was partially cleared by item removal, flags could get stuck.
		auto* exDlg = game->get_dialog_box_manager().get_dialog_as<DialogBox_Exchange>(DialogBoxId::Exchange);
		for (int i = 0; i < 4; i++)
		{
			int slot = exDlg->m_slots[i].inv_slot;
			if (slot >= 0 && slot < hb::shared::limits::MaxItems)
				inventory_manager::get().unlock_item(slot);
		}

		//Snoopy: MultiTrade
		game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::ConfirmExchange);
		game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::Exchange);
	}
} // namespace NetworkMessageHandlers
