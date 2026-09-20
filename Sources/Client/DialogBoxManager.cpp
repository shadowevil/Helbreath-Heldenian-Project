#include "DialogBoxManager.h"
#include "IDialogBox.h"
#include "DialogBox_WarningMsg.h"
#include "DialogBox_Resurrect.h"
#include "DialogBox_Noticement.h"
#include "DialogBox_RepairAll.h"
#include "DialogBox_ConfirmExchange.h"
#include "DialogBox_Help.h"
#include "DialogBox_ItemDrop.h"
#include "DialogBox_LevelUpSetting.h"
#include "DialogBox_Character.h"
#include "DialogBox_Inventory.h"
#include "DialogBox_Skill.h"
#include "DialogBox_Magic.h"
#include "DialogBox_HudPanel.h"
#include "DialogBox_GuideMap.h"
#include "DialogBox_Fishing.h"
#include "DialogBox_CrusadeJob.h"
#include "DialogBox_ItemDropAmount.h"
#include "DialogBox_Map.h"
#include "DialogBox_NpcActionQuery.h"
#include "DialogBox_SysMenu.h"
#include "DialogBox_Text.h"
#include "DialogBox_MagicShop.h"
#include "DialogBox_NpcTalk.h"
#include "DialogBox_ChatHistory.h"
#include "DialogBox_CityHallMenu.h"
#include "DialogBox_Shop.h"
#include "DialogBox_ItemUpgrade.h"
#include "DialogBox_SellList.h"
#include "DialogBox_GuildMenu.h"
#include "DialogBox_GuildOperation.h"
#include "DialogBox_Bank.h"
#include "DialogBox_Exchange.h"
#include "DialogBox_Party.h"
#include "DialogBox_Quest.h"
#include "DialogBox_Commander.h"
#include "DialogBox_Constructor.h"
#include "DialogBox_Soldier.h"
#include "DialogBox_Slates.h"
#include "DialogBox_ChangeStatsMajestic.h"
#include "DialogBox_CommandHallMenu.h"
#include "DialogBox_SellOrRepair.h"
#include "DialogBox_Manufacture.h"
#include "DialogBox_StatusOverlay.h"
#ifdef TESTER_ONLY
// TESTER MENU — includes (tester builds only)
#include "DialogBox_TesterMenu.h"
#include "DialogBox_ItemCreator.h"
#endif // TESTER_ONLY
#include "Game.h"
#include "lan_eng.h"
#include "TextInputManager.h"
#include "TextFieldRenderer.h"
#include "CursorTarget.h"
#include "IInput.h"
#include "Screen_OnGame.h"

using namespace hb::shared::net;

DialogBoxManager::DialogBoxManager(CGame& game, CPlayer& player)
	: m_game(game)
	, m_player(player)
{
}

void DialogBoxManager::initialize_dialog_boxes()
{
	register_dialog_box(std::make_unique<DialogBox_WarningMsg>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Resurrect>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Noticement>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_RepairAll>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_ConfirmExchange>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Help>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_ItemDrop>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_LevelUpSetting>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Character>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Inventory>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Skill>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Magic>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_HudPanel>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_GuideMap>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Fishing>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_CrusadeJob>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_ItemDropAmount>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Map>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_NpcActionQuery>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_SysMenu>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Text>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_MagicShop>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_NpcTalk>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_ChatHistory>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_CityHallMenu>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Shop>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_ItemUpgrade>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_SellList>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_GuildMenu>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_GuildOperation>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Bank>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Exchange>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Party>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Quest>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Commander>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Constructor>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Soldier>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Slates>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_ChangeStatsMajestic>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_CommandHallMenu>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_SellOrRepair>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_Manufacture>(&m_game));
#ifdef TESTER_ONLY
	// TESTER MENU — dialog registration (tester builds only)
	register_dialog_box(std::make_unique<DialogBox_TesterMenu>(&m_game));
	register_dialog_box(std::make_unique<DialogBox_ItemCreator>(&m_game));
#endif // TESTER_ONLY

	register_dialog_box(std::make_unique<DialogBox_StatusOverlay>(&m_game));

	// StatusOverlay is underlay — drawn first, receives clicks last
	set_z_layer(DialogBoxId::StatusOverlay, z_layer::underlay);
	// HudPanel is always topmost — drawn last, not draggable
	set_z_layer(DialogBoxId::HudPanel, z_layer::topmost);
}

void DialogBoxManager::register_dialog_box(std::unique_ptr<IDialogBox> dialog_box)
{
	if (!dialog_box) return;
	dialog_box->set_manager(*this);
	int id = static_cast<int>(dialog_box->get_id());
	m_dialogs[id] = std::move(dialog_box);
}

IDialogBox* DialogBoxManager::get_dialog_box(DialogBoxId::Type id) const
{
	return get_dialog_box(static_cast<int>(id));
}

IDialogBox* DialogBoxManager::get_dialog_box(int box_id) const
{
	auto it = m_dialogs.find(box_id);
	if (it != m_dialogs.end())
		return it->second.get();
	return nullptr;
}

// ============================================================
// Z-layer management
// ============================================================

void DialogBoxManager::set_z_layer(DialogBoxId::Type id, z_layer::type layer)
{
	m_z_layers[static_cast<int>(id)] = layer;
}

z_layer::type DialogBoxManager::get_z_layer(int id) const
{
	auto it = m_z_layers.find(id);
	if (it != m_z_layers.end())
		return it->second;
	return z_layer::normal;
}

void DialogBoxManager::bring_to_front(int id)
{
	auto layer = get_z_layer(id);
	auto& order = (layer == z_layer::topmost) ? m_topmost_order
	            : (layer == z_layer::underlay) ? m_underlay_order
	            : m_normal_order;

	// Remove if already present
	for (auto it = order.begin(); it != order.end(); ++it)
	{
		if (*it == static_cast<uint8_t>(id))
		{
			order.erase(it);
			break;
		}
	}
	// Add to back (topmost in draw order)
	order.push_back(static_cast<uint8_t>(id));
}

void DialogBoxManager::remove_from_order(int id)
{
	auto remove_from = [id](std::vector<uint8_t>& order) {
		for (auto it = order.begin(); it != order.end(); ++it)
		{
			if (*it == static_cast<uint8_t>(id))
			{
				order.erase(it);
				return;
			}
		}
	};
	remove_from(m_underlay_order);
	remove_from(m_normal_order);
	remove_from(m_topmost_order);
}

void DialogBoxManager::clamp_position(IDialogBox* dlg)
{
	if (!dlg) return;
	int maxX = LOGICAL_WIDTH() - 20;
	int maxY = LOGICAL_HEIGHT() - ICON_PANEL_HEIGHT() - 20;
	if (dlg->m_y > maxY) dlg->m_y = static_cast<short>(maxY);
	if (dlg->m_x > maxX) dlg->m_x = static_cast<short>(maxX);
	if ((dlg->m_x + dlg->m_size_x) < 10) dlg->m_x += 20;
	if ((dlg->m_y + dlg->m_size_y) < 10) dlg->m_y += 20;
}

template<typename Func>
bool DialogBoxManager::for_each_top_to_bottom(Func&& fn) const
{
	// Topmost layer first (back to front = reverse iterate)
	for (int i = static_cast<int>(m_topmost_order.size()) - 1; i >= 0; i--)
	{
		auto it = m_dialogs.find(m_topmost_order[i]);
		if (it != m_dialogs.end() && it->second->is_enabled())
		{
			if (fn(it->second.get()))
				return true;
		}
	}
	// Then normal layer (back to front = reverse iterate)
	for (int i = static_cast<int>(m_normal_order.size()) - 1; i >= 0; i--)
	{
		auto it = m_dialogs.find(m_normal_order[i]);
		if (it != m_dialogs.end() && it->second->is_enabled())
		{
			if (fn(it->second.get()))
				return true;
		}
	}
	// Then underlay layer last (back to front = reverse iterate)
	for (int i = static_cast<int>(m_underlay_order.size()) - 1; i >= 0; i--)
	{
		auto it = m_dialogs.find(m_underlay_order[i]);
		if (it != m_dialogs.end() && it->second->is_enabled())
		{
			if (fn(it->second.get()))
				return true;
		}
	}
	return false;
}

int DialogBoxManager::get_top_id() const
{
	// Return topmost normal-layer dialog (topmost layer excluded)
	for (int i = static_cast<int>(m_normal_order.size()) - 1; i >= 0; i--)
	{
		auto it = m_dialogs.find(m_normal_order[i]);
		if (it != m_dialogs.end() && it->second->is_enabled())
			return m_normal_order[i];
	}
	return 0;
}

// ============================================================
// Update + Draw
// ============================================================

void DialogBoxManager::update_all()
{
	for (auto& [id, dlg] : m_dialogs)
	{
		if (dlg && dlg->is_enabled())
			dlg->on_update();
	}
}

void DialogBoxManager::draw_all()
{
	if (m_game.on_game() && m_game.on_game()->m_is_observer_mode) return;

	// Underlay layer: drawn first (underneath everything)
	for (uint8_t id : m_underlay_order)
	{
		auto it = m_dialogs.find(id);
		if (it != m_dialogs.end() && it->second->is_enabled())
			it->second->on_draw();
	}

	// Normal layer: drawn bottom to top
	for (uint8_t id : m_normal_order)
	{
		auto it = m_dialogs.find(id);
		if (it != m_dialogs.end() && it->second->is_enabled())
			it->second->on_draw();
	}

	// Topmost layer: drawn after all normal dialogs
	for (uint8_t id : m_topmost_order)
	{
		auto it = m_dialogs.find(id);
		if (it != m_dialogs.end() && it->second->is_enabled())
			it->second->on_draw();
	}
}

// ============================================================
// Enable / Disable / Toggle
// ============================================================

void DialogBoxManager::enable_dialog_box(int box_id, int type, int64_t v1, int v2, const char* string)
{
	auto* dlg = get_dialog_box(box_id);
	if (!dlg) return;

	if (dlg->cancels_text_input_on_enable())
		text_input_manager::get().end_input();

	if (!dlg->on_enable(type, v1, v2, string))
		return;  // on_enable returned false to cancel

	// Clamp position (skip topmost/underlay dialogs like HudPanel, StatusOverlay)
	if (get_z_layer(box_id) == z_layer::normal)
	{
		if (dlg->is_enabled() == false)
			clamp_position(dlg);
	}

	dlg->set_enabled(true);

	bring_to_front(box_id);
}

void DialogBoxManager::enable_dialog_box(DialogBoxId::Type id, int type, int64_t v1, int v2, const char* string)
{
	enable_dialog_box(static_cast<int>(id), type, v1, v2, string);
}

void DialogBoxManager::disable_dialog_box(int box_id)
{
	auto* dlg = get_dialog_box(box_id);
	if (!dlg || !dlg->is_enabled()) return;

	if (!dlg->on_disable()) return;  // on_disable returns false to cancel

	dlg->set_enabled(false);
	remove_from_order(box_id);
}

void DialogBoxManager::disable_dialog_box(DialogBoxId::Type id)
{
	disable_dialog_box(static_cast<int>(id));
}

void DialogBoxManager::toggle_dialog_box(DialogBoxId::Type id, int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled(id))
		disable_dialog_box(id);
	else
		enable_dialog_box(id, type, v1, v2, string);
}

// ============================================================
// Short aliases
// ============================================================

void DialogBoxManager::enable(DialogBoxId::Type id, int type, int64_t v1, int v2, const char* string)
{
	enable_dialog_box(id, type, v1, v2, string);
}

void DialogBoxManager::enable(int id, int type, int64_t v1, int v2, const char* string)
{
	enable_dialog_box(id, type, v1, v2, string);
}

void DialogBoxManager::disable(DialogBoxId::Type id)
{
	disable_dialog_box(id);
}

void DialogBoxManager::disable(int id)
{
	disable_dialog_box(id);
}

void DialogBoxManager::toggle(DialogBoxId::Type id, int type, int64_t v1, int v2, const char* string)
{
	toggle_dialog_box(id, type, v1, v2, string);
}

// ============================================================
// Lifecycle methods
// ============================================================

void DialogBoxManager::reset_all_for_map_change()
{
	for (auto& [id, dlg] : m_dialogs)
	{
		if (!dlg) continue;
		dlg->set_enabled(false);
		dlg->m_is_scroll_selected = false;
	}

	m_underlay_order.clear();
	m_normal_order.clear();
	m_topmost_order.clear();

	enable_dialog_box(DialogBoxId::StatusOverlay, 0, 0, 0);
	enable_dialog_box(DialogBoxId::HudPanel, 0, 0, 0);
}

void DialogBoxManager::disable_npc_dialogs()
{
	disable_dialog_box(DialogBoxId::GuildMenu);
	disable_dialog_box(DialogBoxId::SaleMenu);
	disable_dialog_box(DialogBoxId::CityHallMenu);
	disable_dialog_box(DialogBoxId::Bank);
	disable_dialog_box(DialogBoxId::MagicShop);
	disable_dialog_box(DialogBoxId::Map);
	disable_dialog_box(DialogBoxId::NpcActionQuery);
	disable_dialog_box(DialogBoxId::NpcTalk);
	disable_dialog_box(DialogBoxId::SellOrRepair);
	disable_dialog_box(DialogBoxId::CommandHallMenu);
	disable_dialog_box(DialogBoxId::ItemUpgrade);
	disable_dialog_box(DialogBoxId::Manufacture);
	disable_dialog_box(DialogBoxId::Exchange);
	disable_dialog_box(DialogBoxId::Slates);
}

void DialogBoxManager::disable_crusade_dialogs()
{
	disable_dialog_box(DialogBoxId::CrusadeCommander);
	disable_dialog_box(DialogBoxId::CrusadeConstructor);
	disable_dialog_box(DialogBoxId::CrusadeSoldier);
}

void DialogBoxManager::disable_crafting_dialogs()
{
	disable_dialog_box(DialogBoxId::Fishing);
	disable_dialog_box(DialogBoxId::Manufacture);
}

// ============================================================
// Enabled state
// ============================================================

bool DialogBoxManager::is_enabled(DialogBoxId::Type id) const
{
	return is_enabled(static_cast<int>(id));
}

bool DialogBoxManager::is_enabled(int box_id) const
{
	auto* dlg = get_dialog_box(box_id);
	return dlg && dlg->is_enabled();
}

bool DialogBoxManager::is_blocking_operation_active() const
{
	static constexpr DialogBoxId::Type ids[] = {
		DialogBoxId::ItemDropExternal, DialogBoxId::NpcActionQuery,
		DialogBoxId::SellOrRepair, DialogBoxId::Manufacture,
		DialogBoxId::Exchange, DialogBoxId::SellList,
		DialogBoxId::ItemDropConfirm
	};
	for (auto id : ids)
		if (is_enabled(id)) return true;
	return false;
}

void DialogBoxManager::set_enabled(DialogBoxId::Type id, bool enabled)
{
	set_enabled(static_cast<int>(id), enabled);
}

void DialogBoxManager::set_enabled(int box_id, bool enabled)
{
	auto* dlg = get_dialog_box(box_id);
	if (dlg) dlg->set_enabled(enabled);
}

// ============================================================
// ============================================================
// Input handlers (no mouse params — read from hb::shared::input)
// ============================================================

bool DialogBoxManager::handle_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());

	return for_each_top_to_bottom([&](IDialogBox* dlg) -> bool {
		if (mouse_x > dlg->m_x && mouse_x < dlg->m_x + dlg->m_size_x &&
			mouse_y > dlg->m_y && mouse_y < dlg->m_y + dlg->m_size_y)
		{
			dlg->on_click();
			return true;
		}
		return false;
	});
}

bool DialogBoxManager::handle_double_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());

	bool consumed = false;
	for_each_top_to_bottom([&](IDialogBox* dlg) -> bool {
		if (mouse_x > dlg->m_x && mouse_x < dlg->m_x + dlg->m_size_x &&
			mouse_y > dlg->m_y && mouse_y < dlg->m_y + dlg->m_size_y)
		{
			consumed = dlg->on_double_click();
			return true;
		}
		return false;
	});
	return consumed;
}

PressResult DialogBoxManager::handle_press(int dlg_id)
{
	auto* dlg = get_dialog_box(dlg_id);
	if (dlg)
		return dlg->on_press();
	return PressResult::Normal;
}

bool DialogBoxManager::handle_item_drop(int dlg_id)
{
	auto* dlg = get_dialog_box(dlg_id);
	if (dlg)
		return dlg->on_item_drop();
	return false;
}

bool DialogBoxManager::handle_dragging_item_release()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());

	return for_each_top_to_bottom([&](IDialogBox* dlg) -> bool {
		if (mouse_x > dlg->m_x && mouse_x < dlg->m_x + dlg->m_size_x &&
			mouse_y > dlg->m_y && mouse_y < dlg->m_y + dlg->m_size_y)
		{
			int id = static_cast<int>(dlg->get_id());
			enable_dialog_box(id, 0, 0, 0);
			handle_item_drop(id);
			return true;
		}
		return false;
	});
}

int DialogBoxManager::handle_mouse_down()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());

	int result = 0;

	for_each_top_to_bottom([&](IDialogBox* dlg) -> bool {
		if (mouse_x >= dlg->m_x && mouse_x <= dlg->m_x + dlg->m_size_x &&
			mouse_y >= dlg->m_y && mouse_y <= dlg->m_y + dlg->m_size_y)
		{
			int id = static_cast<int>(dlg->get_id());

			// Bring dialog to front (don't re-trigger on_enable which resets dialog state)
			bring_to_front(id);

			// Set up drag tracking
			CursorTarget::set_prev_position(mouse_x, mouse_y);
			short dragDistX = mouse_x - dlg->m_x;
			short dragDistY = mouse_y - dlg->m_y;

			// Let the dialog handle the press
			PressResult press_result = handle_press(id);

			if (press_result == PressResult::ScrollClaimed)
			{
				dlg->m_is_scroll_selected = true;
				result = -1;
			}
			else if (press_result == PressResult::Normal)
			{
				if (dlg->is_draggable())
					CursorTarget::set_selection(SelectedObjectType::DialogBox, id, dragDistX, dragDistY);
			}
			// ItemSelected means item was selected, on_press already set up CursorTarget

			if (result == 0) result = 1;
			return true;
		}
		return false;
	});

	return result;
}

bool DialogBoxManager::handle_right_click(uint32_t time)
{
	if ((time - m_close_debounce_time) < 300) return true;

	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());

	bool hit = false;
	for_each_top_to_bottom([&](IDialogBox* dlg) -> bool {
		if (mouse_x > dlg->m_x && mouse_x < dlg->m_x + dlg->m_size_x &&
			mouse_y > dlg->m_y && mouse_y < dlg->m_y + dlg->m_size_y)
		{
			bool can_close = dlg->m_can_close_on_right_click;
			int id = static_cast<int>(dlg->get_id());

			// Special mode-dependent cases
			switch (id)
			{
			case DialogBoxId::SellOrRepair:
				can_close = (static_cast<DialogBox_SellOrRepair*>(dlg)->m_mode < DialogBox_SellOrRepair::mode::sell_pending);
				break;
			case DialogBoxId::Exchange:
				{ auto* exDlg = static_cast<DialogBox_Exchange*>(dlg);
				if (exDlg->m_mode == DialogBox_Exchange::mode::pending) can_close = false; }
				break;
			}

			if (can_close)
				disable_dialog_box(id);

			m_close_debounce_time = time;
			hit = true;
			return true;
		}
		return false;
	});

	return hit;
}
