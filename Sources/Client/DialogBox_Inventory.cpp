#include "DialogBox_Inventory.h"
#include "CursorTarget.h"
#include "DialogBox_Manufacture.h"
#include "Game.h"
#include "InventoryManager.h"
#include "ItemNameFormatter.h"
#include "ItemSpriteMetadata.h"
#include "IInput.h"
#include "lan_eng.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include <format>
#include <string>
#include "Packet/SharedPackets.h"
#include "PacketSendHelpers.h"
#include "Screen_OnGame.h"
#include "AudioManager.h"


using namespace hb::shared::net;
using namespace hb::shared::item;
using namespace hb::client::sprite_id;

// Margin (in pixels) added around item sprites to make small items easier to click.
// Checks if any opaque pixel exists within this distance of the cursor.
constexpr int item_hit_margin = 8;

static bool check_item_collision(auto&& sprite, int sprite_x, int sprite_y,
	int frame, int point_x, int point_y, int margin)
{
	if (sprite->CheckCollision(sprite_x, sprite_y, frame, point_x, point_y))
		return true;

	if (margin <= 0)
		return false;

	for (int dy = -margin; dy <= margin; dy++)
	{
		for (int dx = -margin; dx <= margin; dx++)
		{
			if (dx == 0 && dy == 0) continue;
			if (dx * dx + dy * dy > margin * margin) continue;
			if (sprite->CheckCollision(sprite_x, sprite_y, frame, point_x + dx, point_y + dy))
				return true;
		}
	}
	return false;
}

DialogBox_Inventory::DialogBox_Inventory(CGame* game)
	: IDialogBox(DialogBoxId::Inventory, game)
{
	m_can_close_on_right_click = true;
	set_default_rect(540 , 210 , 225, 185);
}

// Helper: draw a single inventory item with proper coloring and state
void DialogBox_Inventory::draw_inventory_item(CItem* item, int itemIdx, int baseX, int baseY)
{
	CItem* cfg = m_game->get_item_config(item->m_id_num);
	if (cfg == nullptr) return;

	char item_color = item->m_instance.item_color;
	bool disabled = inventory_manager::get().is_locked(itemIdx);

	int drawX = baseX + ITEM_OFFSET_X + item->m_x;
	int drawY = baseY + ITEM_OFFSET_Y + item->m_y;
	auto inv_draw = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
	auto sprite = inv_draw.sprite;
	int16_t frame = inv_draw.frame;
	uint32_t time = m_game->m_cur_time;

	// Unified color palette — index already encodes correct color for weapons and armor
	const auto* colors = m_game->m_color_palette.data();

	if (item_color == 0)
	{
		// No color tint
		if (disabled)
			sprite->draw(drawX, drawY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
		else
			sprite->draw(drawX, drawY, frame);
	}
	else
	{
		// Apply color tint
		int r = colors[item_color].r;
		int g = colors[item_color].g;
		int b = colors[item_color].b;

		if (disabled)
			sprite->draw(drawX, drawY, frame, hb::shared::sprite::DrawParams::tinted_alpha(r, g, b, 0.7f));
		else
			sprite->draw(drawX, drawY, frame, hb::shared::sprite::DrawParams::tint(r, g, b));
	}

	// Show item count for consumables and arrows
	if (cfg->is_stackable())
	{
		std::string countBuf;
		countBuf = m_game->format_comma_number(item->m_instance.count);
		hb::shared::text::draw_text(GameFont::Default, baseX + COUNT_OFFSET_X + item->m_x, baseY + COUNT_OFFSET_Y + item->m_y, countBuf.c_str(), hb::shared::text::TextStyle::with_shadow(GameColors::UIDescription));
	}
}

void DialogBox_Inventory::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	if (!m_game->ensure_item_configs_loaded()) return;
	short sX = m_x;
	short sY = m_y;

	draw_new_dialog_box(InterfaceNdInventory, sX, sY, 0);

	// draw all inventory items
	for (int i = 0; i < hb::shared::limits::MaxItems; i++)
	{
		int itemIdx = m_game->m_item_order[i];
		if (itemIdx == -1) continue;

		if (player().m_item_list[itemIdx] == nullptr)
			continue;

		CItem* item = player().m_item_list[itemIdx].get();
		if (item == nullptr) continue;

		// Skip items that are selected (being dragged) or equipped
		bool selected = (CursorTarget::GetSelectedType() == SelectedObjectType::Item) &&
		                 (CursorTarget::get_selected_id() == itemIdx);
		bool equipped = m_game->m_is_item_equipped[itemIdx];

		if (!selected && !equipped)
		{
			draw_inventory_item(item, itemIdx, sX, sY);
		}
	}

	// Item Upgrade button hover
	if ((mouse_x >= sX + BTN_UPGRADE_X1) && (mouse_x <= sX + BTN_UPGRADE_X2) &&
	    (mouse_y >= sY + BTN_Y1) && (mouse_y <= sY + BTN_Y2))
	{
		draw_new_dialog_box(InterfaceNdInventory, sX + BTN_UPGRADE_X1, sY + BTN_Y1, 1);
	}

	// Manufacture button hover
	if ((mouse_x >= sX + BTN_MANUFACTURE_X1) && (mouse_x <= sX + BTN_MANUFACTURE_X2) &&
	    (mouse_y >= sY + BTN_Y1) && (mouse_y <= sY + BTN_Y2))
	{
		draw_new_dialog_box(InterfaceNdInventory, sX + BTN_MANUFACTURE_X1, sY + BTN_Y1, 2);
	}
}

bool DialogBox_Inventory::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short sX = m_x;
	short sY = m_y;

	// Item Upgrade button
	if ((mouse_x >= sX + BTN_UPGRADE_X1) && (mouse_x <= sX + BTN_UPGRADE_X2) &&
	    (mouse_y >= sY + BTN_Y1) && (mouse_y <= sY + BTN_Y2))
	{
		enable_dialog_box(DialogBoxId::ItemUpgrade, 5, 0, 0);
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	// Manufacture button
	if ((mouse_x >= sX + BTN_MANUFACTURE_X1) && (mouse_x <= sX + BTN_MANUFACTURE_X2) &&
	    (mouse_y >= sY + BTN_Y1) && (mouse_y <= sY + BTN_Y2))
	{
		if (player().m_skill_mastery[13] == 0)
		{
			add_event_list(DLGBOXCLICK_INVENTORY1, 10);
			add_event_list(DLGBOXCLICK_INVENTORY2, 10);
		}
		else if (m_game->on_game()->m_skill_using_status)
		{
			add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY5, 10);
			return true;
		}
		else if (m_game->is_item_on_hand())
		{
			add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY4, 10);
			return true;
		}
		else
		{
			// Look for manufacturing hammer
			for (int i = 0; i < hb::shared::limits::MaxItems; i++)
			{
				CItem* item = player().m_item_list[i].get();
				if (item == nullptr) continue;
				CItem* cfg = m_game->get_item_config(item->m_id_num);
				if (cfg != nullptr &&
				    cfg->get_item_type() == hb::shared::item::item_type::tool &&
				    item->m_id_num == 236 &&
				    item->m_instance.cur_durability > 0)
				{
					enable_dialog_box(DialogBoxId::Manufacture, 3, 0, 0);
					add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY12, 10);
					audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
					return true;
				}
			}
			add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY14, 10);
		}
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_Inventory::on_double_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	if (m_game->on_game()->m_item_using_status)
	{
		add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY1, 10);
		return true;
	}

	if (CursorTarget::GetSelectedType() != SelectedObjectType::Item) return false;
	int item_id = CursorTarget::get_selected_id();
	if (item_id < 0 || item_id >= hb::shared::limits::MaxItems) return false;
	if (player().m_item_list[item_id] == nullptr) return false;

	inventory_manager::get().set_item_order(0, item_id);

	CItem* cfg = m_game->get_item_config(player().m_item_list[item_id]->m_id_num);
	if (cfg == nullptr) return false;

	auto itemInfo = item_name_formatter::get().format(player().m_item_list[item_id].get());

	// Check if at repair shop
	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::SaleMenu) &&
		!m_game->get_dialog_box_manager().is_enabled(DialogBoxId::SellOrRepair) &&
		m_game->get_dialog_box_manager().m_give_item.action_type == 24)
	{
		if (cfg->get_equip_pos() != EquipPos::None)
		{
			{
				auto pkt = hb::net::make_common_command_str(CommonType::ReqRepairItem, player().m_player_x, player().m_player_y);
				pkt.v1 = item_id;
				pkt.v2 = m_game->get_dialog_box_manager().m_give_item.action_type;
				std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg->m_name);
				pkt.v4 = m_game->get_dialog_box_manager().m_give_item.object_id;
				send_game_packet(pkt);
			}
			return true;
		}
	}

	// Bank dialog - drop item there
	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::Bank))
	{
		m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::Bank)->on_item_drop();
		return true;
	}
	// Sell list dialog
	else if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::SellList))
	{
		m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::SellList)->on_item_drop();
		return true;
	}
	// Item upgrade dialog
	else if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::ItemUpgrade))
	{
		m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::ItemUpgrade)->on_item_drop();
		return true;
	}

	// Handle consumable/depletable items (excludes target sub-type which uses pointing mode instead)
	if ((cfg->get_item_type() == hb::shared::item::item_type::consumable ||
		cfg->get_item_sub_type() == hb::shared::item::item_sub_type::ammo) &&
		cfg->get_item_sub_type() != hb::shared::item::item_sub_type::target)
	{
		if (!inventory_manager::get().check_item_operation_enabled(item_id)) return true;

		// Check damage cooldown for scrolls
		if ((m_game->m_cur_time - m_game->m_damaged_time) < 10000)
		{
			if (cfg->get_item_effect_type() == hb::shared::item::ItemEffectType::ShowLocation)
			{
				std::string G_cTxt;
				G_cTxt = std::format(BDLBBOX_DOUBLE_CLICK_INVENTORY3, itemInfo.name.c_str());
				add_event_list(G_cTxt.c_str(), 10);
				return true;
			}
		}

		{
			auto pkt = hb::net::make_common_command(CommonType::ReqUseItem, player().m_player_x, player().m_player_y);
			pkt.v1 = item_id;
			send_game_packet(pkt);
		}

		if (cfg->get_item_type() == hb::shared::item::item_type::consumable)
		{
			inventory_manager::get().lock_item(item_id);
			m_game->on_game()->m_item_using_status = true;
		}
	}

	// Handle skill items (pointing mode)
	if (cfg->get_item_type() == hb::shared::item::item_type::tool && cfg->get_item_sub_type() != hb::shared::item::item_sub_type::target && cfg->get_item_sub_type() != hb::shared::item::item_sub_type::crafting)
	{
		if (m_game->is_item_on_hand())
		{
			add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY4, 10);
			return true;
		}
		if (m_game->on_game()->m_skill_using_status)
		{
			add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY5, 10);
			return true;
		}
		if (player().m_item_list[item_id]->m_instance.cur_durability == 0)
		{
			add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY6, 10);
		}
		else
		{
			m_game->on_game()->m_is_get_pointing_mode = true;
			m_game->on_game()->m_point_command_type = item_id;
			std::string txt;
			txt = std::format(BDLBBOX_DOUBLE_CLICK_INVENTORY7, itemInfo.name.c_str());
			add_event_list(txt.c_str(), 10);
		}
	}

	// Handle deplete-dest items (use on other items)
	if (cfg->get_item_sub_type() == hb::shared::item::item_sub_type::target)
	{
		if (m_game->is_item_on_hand())
		{
			add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY4, 10);
			return true;
		}
		if (m_game->on_game()->m_skill_using_status)
		{
			add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY13, 10);
			return true;
		}
		if (player().m_item_list[item_id]->m_instance.cur_durability == 0)
		{
			add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY6, 10);
		}
		else
		{
			m_game->on_game()->m_is_get_pointing_mode = true;
			m_game->on_game()->m_point_command_type = item_id;
			std::string txt;
			txt = std::format(BDLBBOX_DOUBLE_CLICK_INVENTORY8, itemInfo.name.c_str());
			add_event_list(txt.c_str(), 10);
		}
	}

	// Handle skill items that enable dialog boxes (alchemy pot, anvil, crafting, slates)
	if (cfg->get_item_sub_type() == hb::shared::item::item_sub_type::crafting)
	{
		if (m_game->is_item_on_hand())
		{
			add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY4, 10);
			return true;
		}
		if (m_game->on_game()->m_skill_using_status)
		{
			add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY5, 10);
			return true;
		}
		if (player().m_item_list[item_id]->m_instance.cur_durability == 0)
		{
			add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY6, 10);
		}
		else
		{
			switch (player().m_item_list[item_id]->m_id_num)
			{
			case 227: // Alchemy Bowl
				if (player().m_skill_mastery[12] == 0)
					add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY9, 10);
				else
				{
					enable_dialog_box(DialogBoxId::Manufacture, 1, 0, 0);
					add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY10, 10);
				}
				break;

			case 236: // Smith's Anvil
				if (player().m_skill_mastery[13] == 0)
					add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY11, 10);
				else
				{
					enable_dialog_box(DialogBoxId::Manufacture, 3, 0, 0);
					add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY12, 10);
				}
				break;

			case 1107: // Crafting Vessel
				enable_dialog_box(DialogBoxId::Manufacture, 7, 0, 0);
				add_event_list(BDLBBOX_DOUBLE_CLICK_INVENTORY17, 10);
				break;

			case 868: // Slate UL
			case 869: // Slate LL
			case 870: // Slate UR
			case 871: // Slate LR
				enable_dialog_box(DialogBoxId::Slates, 1, 0, 0);
				break;
			}
		}
	}

	// If alchemy/manufacture/crafting dialog is open, drop item there
	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::Manufacture))
	{
		auto mfg_mode = m_game->get_dialog_box_manager().get_dialog_as<DialogBox_Manufacture>(DialogBoxId::Manufacture)->m_mode;
		if (mfg_mode == DialogBox_Manufacture::mode::alchemy_waiting || mfg_mode == DialogBox_Manufacture::mode::manufacture_waiting || mfg_mode == DialogBox_Manufacture::mode::crafting_waiting)
			m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::Manufacture)->on_item_drop();
	}

	// Auto-equip equipment items
	if (cfg->get_item_type() == hb::shared::item::item_type::equipment)
	{
		CursorTarget::set_selection(SelectedObjectType::Item, static_cast<short>(item_id), 0, 0);
		m_game->get_dialog_box_manager().get_dialog_box(DialogBoxId::CharacterInfo)->on_item_drop();
		CursorTarget::clear_selection();
	}

	return true;
}

PressResult DialogBox_Inventory::on_press()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	// Don't allow item selection if certain dialogs are open
	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::ItemDropExternal)) return PressResult::Normal;
	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::ItemDropConfirm)) return PressResult::Normal;

	short sX = m_x;
	short sY = m_y;

	// Check items in reverse order (topmost first)
	for (int i = 0; i < hb::shared::limits::MaxItems; i++)
	{
		int item_id = m_game->m_item_order[hb::shared::limits::MaxItems - 1 - i];
		if (item_id == -1) continue;

		CItem* item = player().m_item_list[item_id].get();
		if (item == nullptr) continue;

		CItem* cfg = m_game->get_item_config(item->m_id_num);
		if (cfg == nullptr) continue;

		// Skip disabled or equipped items
		if (inventory_manager::get().is_locked(item_id)) continue;
		if (m_game->m_is_item_equipped[item_id]) continue;

		// Calculate item bounds using atlas
		auto inv_draw = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
		int itemDrawX = sX + ITEM_OFFSET_X + item->m_x;
		int itemDrawY = sY + ITEM_OFFSET_Y + item->m_y;

		inv_draw.sprite->CalculateBounds(itemDrawX, itemDrawY, inv_draw.frame);
		auto bounds = inv_draw.sprite->GetBoundRect();

		// Check if click is within item bounds (expanded by margin for small items)
		if (mouse_x > bounds.left - item_hit_margin && mouse_x < bounds.right + item_hit_margin &&
			mouse_y > bounds.top - item_hit_margin && mouse_y < bounds.bottom + item_hit_margin)
		{
			// Pixel-perfect collision check with margin for small items
			if (check_item_collision(inv_draw.sprite, itemDrawX, itemDrawY, inv_draw.frame, mouse_x, mouse_y, item_hit_margin))
			{
				// Bring item to top of order
				inventory_manager::get().set_item_order(0, item_id);

				// Handle pointing mode (using items on other items)
				bool handled_pointing = false;
				if (m_game->on_game()->m_is_get_pointing_mode &&
					m_game->on_game()->m_point_command_type >= 0 &&
					m_game->on_game()->m_point_command_type < 100 &&
					player().m_item_list[m_game->on_game()->m_point_command_type] != nullptr &&
					m_game->on_game()->m_point_command_type != item_id)
				{
					CItem* point_cfg = m_game->get_item_config(player().m_item_list[m_game->on_game()->m_point_command_type]->m_id_num);
					if (point_cfg != nullptr &&
						point_cfg->get_item_sub_type() == hb::shared::item::item_sub_type::target)
					{
						m_game->point_command_handler(0, 0, item_id);
						m_game->on_game()->m_is_get_pointing_mode = false;
						handled_pointing = true;
					}
				}
				if (!handled_pointing)
				{
					// Select the item for dragging
					CursorTarget::set_selection(SelectedObjectType::Item, item_id,
						mouse_x - itemDrawX, mouse_y - itemDrawY);
				}
				return PressResult::ItemSelected;
			}
		}
	}

	return PressResult::Normal;
}

bool DialogBox_Inventory::on_item_drop()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	if (player().m_Controller.get_command() < 0) return false;

	int selected_id = CursorTarget::get_selected_id();
	if (selected_id < 0 || selected_id >= hb::shared::limits::MaxItems) return false;
	if (player().m_item_list[selected_id] == nullptr) return false;

	// Can't move equipped items while using a skill
	if (m_game->on_game()->m_skill_using_status && m_game->m_is_item_equipped[selected_id])
	{
		add_event_list(BITEMDROP_INVENTORY1, 10);
		return false;
	}
	if (inventory_manager::get().warn_if_locked(selected_id)) return false;

	// Calculate new position in inventory grid
	short sX = m_x;
	short sY = m_y;
	short dX = mouse_x - sX - ITEM_OFFSET_X - CursorTarget::get_drag_dist_x();
	short dY = mouse_y - sY - ITEM_OFFSET_Y - CursorTarget::get_drag_dist_y();

	// clamp to valid inventory area
	if (dY < -10) dY = -10;
	if (dX < 0) dX = 0;
	if (dX > 170) dX = 170;
	if (dY > 95) dY = 95;

	player().m_item_list[selected_id]->m_x = dX;
	player().m_item_list[selected_id]->m_y = dY;

	// Shift+drop: move all items with the same name to this position
	if (hb::shared::input::is_shift_down())
	{
		for (int i = 0; i < hb::shared::limits::MaxItems; i++)
		{
			if (m_game->m_item_order[hb::shared::limits::MaxItems - 1 - i] != -1)
			{
				int item_id = m_game->m_item_order[hb::shared::limits::MaxItems - 1 - i];
				if (player().m_item_list[item_id] != nullptr &&
					player().m_item_list[item_id]->m_id_num == player().m_item_list[selected_id]->m_id_num)
				{
					player().m_item_list[item_id]->m_x = dX;
					player().m_item_list[item_id]->m_y = dY;
					{
				hb::net::PacketRequestSetItemPos req{};
				req.header.msg_id = MsgId::RequestSetItemPos;
				req.header.msg_type = 0;
				req.dir = static_cast<uint8_t>(item_id);
				req.x = static_cast<int16_t>(dX);
				req.y = static_cast<int16_t>(dY);
				send_game_packet(req, false);
			}
				}
			}
		}
	}
	else
	{
		{
				hb::net::PacketRequestSetItemPos req{};
				req.header.msg_id = MsgId::RequestSetItemPos;
				req.header.msg_type = 0;
				req.dir = static_cast<uint8_t>(selected_id);
				req.x = static_cast<int16_t>(dX);
				req.y = static_cast<int16_t>(dY);
				send_game_packet(req, false);
			}
	}

	// If item was equipped, unequip it — server will send Notify::ItemReleased with message + sound
	if (m_game->m_is_item_equipped[selected_id])
	{
		CItem* cfg = m_game->get_item_config(player().m_item_list[selected_id]->m_id_num);
		if (cfg == nullptr) return false;

		// Remove Angelic Stats
		if (cfg->get_equip_pos() >= EquipPos::LeftFinger &&
			cfg->get_item_type() == hb::shared::item::item_type::equipment)
		{
			if (player().m_item_list[selected_id]->m_id_num == hb::shared::item::ItemId::AngelicPendantSTR)
				player().m_angelic_str = 0;
			else if (player().m_item_list[selected_id]->m_id_num == hb::shared::item::ItemId::AngelicPendantDEX)
				player().m_angelic_dex = 0;
			else if (player().m_item_list[selected_id]->m_id_num == hb::shared::item::ItemId::AngelicPendantINT)
				player().m_angelic_int = 0;
			else if (player().m_item_list[selected_id]->m_id_num == hb::shared::item::ItemId::AngelicPendantMAG)
				player().m_angelic_mag = 0;
		}

		{
			auto pkt = hb::net::make_common_command(CommonType::ReleaseItem, player().m_player_x, player().m_player_y);
			pkt.v1 = selected_id;
			send_game_packet(pkt);
		}
		m_game->m_is_item_equipped[selected_id] = false;
		m_game->m_item_equipment_status[cfg->m_equip_pos] = -1;
	}

	return true;
}
