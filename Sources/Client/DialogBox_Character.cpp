#include "DialogBox_Character.h"
#include "ConfigManager.h"
#include "CursorTarget.h"
#include "Game.h"
#include "GameFonts.h"
#include "PacketSendHelpers.h"
#include "TextLibExt.h"

#include "InventoryManager.h"
#include "ItemNameFormatter.h"
#include "ItemSpriteMetadata.h"
#include "lan_eng.h"
#include "SharedCalculations.h"
#include "BalanceConstants.h"
#include <format>
#include <string>
#include "IInput.h"
#include "AudioManager.h"

using namespace hb::shared::net;
using namespace hb::shared::item;

using hb::shared::item::EquipPos;
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

// draw order: first entry drawn first (bottom layer), last entry drawn last (top layer).
// Collision checks iterate in reverse so topmost-drawn item has highest click priority.
static constexpr EquipSlotLayout MaleEquipSlots[] = {
	{ EquipPos::Back,        41,  137 },
	{ EquipPos::Leggings,      171,  290 },
	{ EquipPos::Arms,       171,  290 },
	{ EquipPos::Boots,   171,  290 },
	{ EquipPos::Body,       171,  290 },
	{ EquipPos::FullBody,   171,  290 },
	{ EquipPos::LeftHand,    90,  170 },
	{ EquipPos::RightHand,   57,  186 },
	{ EquipPos::TwoHand,     57,  186 },
	{ EquipPos::Neck,        35,  120 },
	{ EquipPos::RightFinger, 32,  193 },
	{ EquipPos::LeftFinger,  92,  174 },
	{ EquipPos::Head,        72,  135 },
};

static constexpr EquipSlotLayout FemaleEquipSlots[] = {
	{ EquipPos::Back,        45,  143 },
	{ EquipPos::Leggings,      171,  290 },
	{ EquipPos::Arms,       171,  290 },
	{ EquipPos::Boots,   171,  290 },
	{ EquipPos::Body,       171,  290 },
	{ EquipPos::FullBody,   171,  290 },
	{ EquipPos::LeftHand,    84,  175 },
	{ EquipPos::RightHand,   60,  191 },
	{ EquipPos::TwoHand,     60,  191 },
	{ EquipPos::Neck,        35,  120 },
	{ EquipPos::RightFinger, 32,  193 },
	{ EquipPos::LeftFinger,  92,  174 },
	{ EquipPos::Head,        72,  139 },
};

DialogBox_Character::DialogBox_Character(CGame* game)
	: IDialogBox(DialogBoxId::CharacterInfo, game)
{
	set_default_rect(30 , 30 , 270, 376);
}

// Helper: Display stat with optional angelic bonus (blue if boosted)
void DialogBox_Character::draw_stat(int x1, int x2, int y, int baseStat, int angelicBonus)
{
	if (angelicBonus == 0)
	{
		auto buf = std::format("{}", baseStat);
		put_aligned_string(x1, x2, y, buf.c_str(), GameColors::UILabel);
	}
	else
	{
		auto buf = std::format("{}", baseStat + angelicBonus);
		put_aligned_string(x1, x2, y, buf.c_str(), GameColors::UIModifiedStat);
	}
}

// Find the topmost equipped slot colliding with the mouse, using the given table.
// Returns the EquipPos of the topmost hit, or EquipPos::None if nothing collides.
static EquipPos FindHoverSlot(CGame* game, const EquipSlotLayout* slots, int slotCount,
	short sX, short sY, short mouse_x, short mouse_y, const char* equip_poi_status, int spriteOffset)
{
	bool is_female = (spriteOffset == 40);
	for (int i = slotCount - 1; i >= 0; i--)
	{
		int ep = static_cast<int>(slots[i].equipPos);
		int itemIdx = equip_poi_status[ep];
		if (itemIdx == -1) continue;

		CItem* cfg = game->get_item_config(game->m_player->m_item_list[itemIdx]->m_id_num);
		if (cfg == nullptr) continue;

		auto draw = game->get_item_draw(cfg->m_display_id, item_atlas::equip, is_female);
		if (check_item_collision(draw.sprite,
			sX + slots[i].offsetX, sY + slots[i].offsetY, draw.frame, mouse_x, mouse_y, item_hit_margin))
		{
			return slots[i].equipPos;
		}
	}
	return EquipPos::None;
}

// Helper: render equipped item with optional hover highlight
void DialogBox_Character::draw_equipped_item(hb::shared::item::EquipPos equipPos, int drawX, int drawY,
	const char* equip_poi_status, bool highlight, int spriteOffset)
{
	int itemIdx = equip_poi_status[static_cast<int>(equipPos)];
	if (itemIdx == -1) return;

	CItem* item = player().m_item_list[itemIdx].get();
	CItem* cfg = m_game->get_item_config(item->m_id_num);
	if (cfg == nullptr) return;

	char item_color = item->m_instance.item_color;
	bool disabled = inventory_manager::get().is_locked(itemIdx);

	// Unified color palette — index already encodes correct color for weapons and armor
	const auto* palette = m_game->m_color_palette.data();

	bool is_female = (spriteOffset == 40);
	auto equip_draw = m_game->get_item_draw(cfg->m_display_id, item_atlas::equip, is_female);
	auto sprite = equip_draw.sprite;
	int16_t frame = equip_draw.frame;

	if (!disabled)
	{
		if (item_color == 0)
			sprite->draw(drawX, drawY, frame);
		else
			sprite->draw(drawX, drawY, frame, hb::shared::sprite::DrawParams::tint(palette[item_color].r, palette[item_color].g, palette[item_color].b));
	}
	else
	{
		if (item_color == 0)
			sprite->draw(drawX, drawY, frame, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
		else
			sprite->draw(drawX, drawY, frame, hb::shared::sprite::DrawParams::tinted_alpha(palette[item_color].r, palette[item_color].g, palette[item_color].b, 0.7f));
	}

	if (highlight)
		sprite->draw(drawX, drawY, frame, hb::shared::sprite::DrawParams::additive(0.35f));
}

// Helper: draw hover button
void DialogBox_Character::draw_hover_button(int sX, int sY, int btnX, int btnY,
	short mouse_x, short mouse_y, int hoverFrame, int normalFrame)
{
	bool hover = (mouse_x >= sX + btnX) && (mouse_x <= sX + btnX + ui_layout::btn_size_x) &&
	              (mouse_y >= sY + btnY) && (mouse_y <= sY + btnY + ui_layout::btn_size_y);
	const bool dialogTrans = config_manager::get().is_dialog_transparency_enabled();
	draw_new_dialog_box(InterfaceNdButton, sX + btnX, sY + btnY,
		hover ? hoverFrame : normalFrame, false, dialogTrans);
}

void DialogBox_Character::build_equip_status_array(char (&equip_poi_status)[DEF_MAXITEMEQUIPPOS]) const
{
	std::memset(equip_poi_status, -1, sizeof(equip_poi_status));
	for (int i = 0; i < hb::shared::limits::MaxItems; i++)
	{
		if (player().m_item_list[i] != nullptr && m_game->m_is_item_equipped[i])
		{
			CItem* cfg = m_game->get_item_config(player().m_item_list[i]->m_id_num);
			if (cfg != nullptr)
				equip_poi_status[cfg->m_equip_pos] = i;
		}
	}
}

char DialogBox_Character::find_equip_item_at_point(short mouse_x, short mouse_y, short sX, short sY,
	const char* equip_poi_status) const
{
	const EquipSlotLayout* slots = nullptr;
	int slotCount = 0;
	int spriteOffset = 0;

	if (player().m_player_type >= 1 && player().m_player_type <= 3)
	{
		slots = MaleEquipSlots;
		slotCount = static_cast<int>(std::size(MaleEquipSlots));
	}
	else if (player().m_player_type >= 4 && player().m_player_type <= 6)
	{
		slots = FemaleEquipSlots;
		slotCount = static_cast<int>(std::size(FemaleEquipSlots));
		spriteOffset = 40;
	}

	// Iterate in reverse: topmost drawn item gets highest click priority
	for (int i = slotCount - 1; i >= 0; i--)
	{
		int ep = static_cast<int>(slots[i].equipPos);
		int itemIdx = equip_poi_status[ep];
		if (itemIdx == -1) continue;

		CItem* cfg = m_game->get_item_config(player().m_item_list[itemIdx]->m_id_num);
		if (cfg == nullptr) continue;

		bool is_female = (spriteOffset == 40);
		auto draw = m_game->get_item_draw(cfg->m_display_id, item_atlas::equip, is_female);
		if (check_item_collision(draw.sprite,
			sX + slots[i].offsetX, sY + slots[i].offsetY, draw.frame, mouse_x, mouse_y, item_hit_margin))
		{
			return static_cast<char>(itemIdx);
		}
	}

	return -1;
}

void DialogBox_Character::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	if (!m_game->ensure_item_configs_loaded()) return;
	short sX = m_x;
	short sY = m_y;
	char collison = -1;
	const bool dialogTrans = config_manager::get().is_dialog_transparency_enabled();

	draw_new_dialog_box(InterfaceNdText, sX, sY, 0, false, dialogTrans);

	// Player name and PK/contribution
	std::string txt2;
	std::string infoBuf = player().m_player_name + " : ";

	if (player().m_pk_count > 0) {
		txt2 = std::format(DRAW_DIALOGBOX_CHARACTER1, player().m_pk_count);
		infoBuf += txt2;
	}
	txt2 = std::format(DRAW_DIALOGBOX_CHARACTER2, player().m_contribution);
	infoBuf += txt2;
	put_aligned_string(sX + 24, sX + 252, sY + 52, infoBuf.c_str(), GameColors::UIDarkRed);

	// Citizenship status
	std::string statusBuf;
	if (!player().m_citizen)
	{
		statusBuf = DRAW_DIALOGBOX_CHARACTER7;
	}
	else
	{
		statusBuf = player().m_hunter
			? (player().m_aresden ? DEF_MSG_ARECIVIL : DEF_MSG_ELVCIVIL)
			: (player().m_aresden ? DEF_MSG_ARESOLDIER : DEF_MSG_ELVSOLDIER);
	}
	put_aligned_string(sX, sX + 275, sY + 69, statusBuf.c_str(), GameColors::UILabel);

	// Level, Exp, Next Exp
	std::string statBuf;
	statBuf = std::format("{}", player().m_level);
	put_aligned_string(sX + 180, sX + 250, sY + 106, statBuf.c_str(), GameColors::UILabel);

	statBuf = m_game->format_comma_number(player().m_exp);
	put_aligned_string(sX + 180, sX + 250, sY + 125, statBuf.c_str(), GameColors::UILabel);

	statBuf = m_game->format_comma_number(m_game->get_level_exp(player().m_level + 1));
	put_aligned_string(sX + 180, sX + 250, sY + 142, statBuf.c_str(), GameColors::UILabel);

	// Calculate max stats
	int max_hp = hb::shared::calc::max_hp(m_game->m_formula_engine,
		hb::shared::calc::vit{(double)player().m_vit}, hb::shared::calc::level{(double)player().m_level},
		hb::shared::calc::str{(double)player().m_str}, hb::shared::calc::angelic_str{(double)player().m_angelic_str});
	int max_mp = hb::shared::calc::max_mp(m_game->m_formula_engine,
		hb::shared::calc::mag{(double)player().m_mag}, hb::shared::calc::angelic_mag{(double)player().m_angelic_mag},
		hb::shared::calc::level{(double)player().m_level}, hb::shared::calc::intel{(double)player().m_int},
		hb::shared::calc::angelic_int{(double)player().m_angelic_int});
	int max_sp = hb::shared::calc::max_sp(m_game->m_formula_engine,
		hb::shared::calc::str{(double)player().m_str}, hb::shared::calc::angelic_str{(double)player().m_angelic_str},
		hb::shared::calc::level{(double)player().m_level});
	int max_load = hb::shared::calc::max_load(m_game->m_formula_engine,
		hb::shared::calc::str{(double)player().m_str}, hb::shared::calc::angelic_str{(double)player().m_angelic_str},
		hb::shared::calc::level{(double)player().m_level});

	// HP, MP, SP
	std::string valueBuf;
	valueBuf = std::format("{}/{}", player().m_hp, max_hp);
	put_aligned_string(sX + 180, sX + 250, sY + 173, valueBuf.c_str(), GameColors::UILabel);

	valueBuf = std::format("{}/{}", player().m_mp, max_mp);
	put_aligned_string(sX + 180, sX + 250, sY + 191, valueBuf.c_str(), GameColors::UILabel);

	valueBuf = std::format("{}/{}", player().m_sp, max_sp);
	put_aligned_string(sX + 180, sX + 250, sY + 208, valueBuf.c_str(), GameColors::UILabel);

	// Max load
	int total_weight = inventory_manager::get().calc_total_weight();
	valueBuf = std::format("{:.2f}/{}", CItem::weight_to_stones(total_weight), max_load);
	put_aligned_string(sX + 180, sX + 250, sY + 240, valueBuf.c_str(), GameColors::UILabel);

	// Enemy Kills
	valueBuf = std::format("{}", player().m_enemy_kill_count);
	put_aligned_string(sX + 180, sX + 250, sY + 257, valueBuf.c_str(), GameColors::UILabel);

	// Stats with angelic bonuses
	draw_stat(sX + 44, sX + 86, sY + 285, player().m_str, player().m_angelic_str);   // Str
	draw_stat(sX + 48, sX + 86, sY + 304, player().m_dex, player().m_angelic_dex);   // Dex
	draw_stat(sX + 131, sX + 171, sY + 285, player().m_int, player().m_angelic_int); // Int
	draw_stat(sX + 131, sX + 171, sY + 304, player().m_mag, player().m_angelic_mag); // Mag

	// Vit and Chr (no angelic bonus)
	std::string vitChrBuf;
	vitChrBuf = std::format("{}", player().m_vit);
	put_aligned_string(sX + 214, sX + 255, sY + 285, vitChrBuf.c_str(), GameColors::UILabel);
	vitChrBuf = std::format("{}", player().m_charisma);
	put_aligned_string(sX + 214, sX + 255, sY + 304, vitChrBuf.c_str(), GameColors::UILabel);

	// Build equipment status array
	char equip_poi_status[DEF_MAXITEMEQUIPPOS];
	build_equip_status_array(equip_poi_status);

	// draw character model based on gender
	if (player().m_player_type >= 1 && player().m_player_type <= 3)
	{
		draw_male_character(sX, sY, mouse_x, mouse_y, equip_poi_status, collison);
	}
	else if (player().m_player_type >= 4 && player().m_player_type <= 6)
	{
		draw_female_character(sX, sY, mouse_x, mouse_y, equip_poi_status, collison);
	}

	// draw buttons (Quest, Party, LevelUp)
	draw_hover_button(sX, sY, 15, 340, mouse_x, mouse_y, 5, 4);   // Quest
	draw_hover_button(sX, sY, 98, 340, mouse_x, mouse_y, 45, 44); // Party
	draw_hover_button(sX, sY, 180, 340, mouse_x, mouse_y, 11, 10); // LevelUp
}

void DialogBox_Character::draw_male_character(short sX, short sY, short mouse_x, short mouse_y,
	const char* equip_poi_status, char& collison)
{
	// Base body
	m_game->m_sprite[ItemEquipPivotPoint + 0]->draw(sX + 171, sY + 290, player().m_player_type - 1);

	// Hair (if no helmet)
	if (equip_poi_status[to_int(EquipPos::Head)] == -1)
	{
		const auto& hc = m_game->m_color_palette[player().m_playerAppearance.hair_color];
		m_game->m_sprite[ItemEquipPivotPoint + 18]->draw(sX + 171, sY + 290, player().m_playerAppearance.hair_style, hb::shared::sprite::DrawParams::tint(hc.r, hc.g, hc.b));
	}

	// Underwear
	m_game->m_sprite[ItemEquipPivotPoint + 19]->draw(sX + 171, sY + 290, player().m_playerAppearance.underwear_type);

	// Find topmost hovered slot (reverse scan) before drawing
	EquipPos hoverSlot = FindHoverSlot(m_game, MaleEquipSlots, static_cast<int>(std::size(MaleEquipSlots)),
		sX, sY, mouse_x, mouse_y, equip_poi_status, 0);
	if (hoverSlot != EquipPos::None)
		collison = static_cast<char>(hoverSlot);

	// Equipment slots (draw order from table)
	for (const auto& slot : MaleEquipSlots)
	{
		draw_equipped_item(slot.equipPos, sX + slot.offsetX, sY + slot.offsetY,
			equip_poi_status, slot.equipPos == hoverSlot);
	}

}

void DialogBox_Character::draw_female_character(short sX, short sY, short mouse_x, short mouse_y,
	const char* equip_poi_status, char& collison)
{
	// Base body (female uses +40 offset from male sprites)
	m_game->m_sprite[ItemEquipPivotPoint + 40]->draw(sX + 171, sY + 290, player().m_player_type - 4);

	// Hair (if no helmet) - female hair is at +18+40 = +58
	if (equip_poi_status[to_int(EquipPos::Head)] == -1)
	{
		const auto& hc = m_game->m_color_palette[player().m_playerAppearance.hair_color];
		m_game->m_sprite[ItemEquipPivotPoint + 18 + 40]->draw(sX + 171, sY + 290, player().m_playerAppearance.hair_style, hb::shared::sprite::DrawParams::tint(hc.r, hc.g, hc.b));
	}

	// Underwear - female underwear is at +19+40 = +59
	m_game->m_sprite[ItemEquipPivotPoint + 19 + 40]->draw(sX + 171, sY + 290, player().m_playerAppearance.underwear_type);

	// Check for skirt in pants slot (sprite 12, frame 0 = skirt)
	bool skirt = false;
	if (equip_poi_status[to_int(EquipPos::Leggings)] != -1)
	{
		CItem* cfg = m_game->get_item_config(player().m_item_list[equip_poi_status[to_int(EquipPos::Leggings)]]->m_id_num);
		if (cfg != nullptr && player().m_item_list[equip_poi_status[to_int(EquipPos::Leggings)]]->m_id_num == 479) // Skirt (W)
			skirt = true;
	}

	// Find topmost hovered slot (reverse scan) before drawing
	EquipPos hoverSlot = FindHoverSlot(m_game, FemaleEquipSlots, static_cast<int>(std::size(FemaleEquipSlots)),
		sX, sY, mouse_x, mouse_y, equip_poi_status, 40);
	if (hoverSlot != EquipPos::None)
		collison = static_cast<char>(hoverSlot);

	// If wearing skirt, pre-draw boots under the skirt
	if (skirt)
		draw_equipped_item(EquipPos::Boots, sX + 171, sY + 290, equip_poi_status, hoverSlot == EquipPos::Boots, 40);

	// Equipment slots (draw order from table)
	for (const auto& slot : FemaleEquipSlots)
	{
		if (skirt && slot.equipPos == EquipPos::Boots) continue; // already drawn
		draw_equipped_item(slot.equipPos, sX + slot.offsetX, sY + slot.offsetY,
			equip_poi_status, slot.equipPos == hoverSlot, 40);
	}

}

bool DialogBox_Character::on_click()
{
	// Quest button
	if (mouse_in(btn_quest)) {
		enable_dialog_box(DialogBoxId::Quest, 1, 0, 0);
		disable_this_dialog();
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}
	// Party button
	if (mouse_in(btn_party)) {
		enable_dialog_box(DialogBoxId::Party, 0, 0, 0);
		disable_this_dialog();
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}
	// LevelUp button
	if (mouse_in(btn_levelup)) {
		enable_dialog_box(DialogBoxId::LevelUpSetting, 0, 0, 0);
		disable_this_dialog();
		audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
		return true;
	}

	return false;
}

bool DialogBox_Character::on_double_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::ItemDropExternal))
		return false;

	short sX = m_x;
	short sY = m_y;

	// Build equipment position status array
	char equip_poi_status[DEF_MAXITEMEQUIPPOS];
	build_equip_status_array(equip_poi_status);

	// Find clicked item
	char item_id = find_equip_item_at_point(mouse_x, mouse_y, sX, sY, equip_poi_status);
	if (item_id == -1 || player().m_item_list[item_id] == nullptr)
		return false;

	CItem* item = player().m_item_list[item_id].get();
	CItem* cfg = m_game->get_item_config(item->m_id_num);
	if (cfg == nullptr)
		return false;

	// Skip consumables, arrows, and stacked items
	if (cfg->get_item_type() == hb::shared::item::item_type::consumable ||
		cfg->get_item_sub_type() == hb::shared::item::item_sub_type::ammo ||
		item->m_instance.count > 1)
		return false;

	// Check if at repair shop
	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::SaleMenu) &&
		!m_game->get_dialog_box_manager().is_enabled(DialogBoxId::SellOrRepair) &&
		m_game->get_dialog_box_manager().m_give_item.action_type == 24)
	{
		{
			auto pkt = hb::net::make_common_command_str(CommonType::ReqRepairItem, player().m_player_x, player().m_player_y);
			pkt.v1 = item_id;
			pkt.v2 = m_game->get_dialog_box_manager().m_give_item.action_type;
			std::snprintf(pkt.text, sizeof(pkt.text), "%s", cfg->m_name);
			pkt.v4 = m_game->get_dialog_box_manager().m_give_item.object_id;
			send_game_packet(pkt);
		}
	}
	else
	{
		// Release (unequip) the item — server will send Notify::ItemReleased with message + sound
		if (m_game->m_is_item_equipped[item_id])
		{
			// Remove Angelic Stats
			if (cfg->get_equip_pos() >= EquipPos::LeftFinger &&
				cfg->get_item_type() == hb::shared::item::item_type::equipment)
			{
				if (item->m_id_num == hb::shared::item::ItemId::AngelicPendantSTR)
					player().m_angelic_str = 0;
				else if (item->m_id_num == hb::shared::item::ItemId::AngelicPendantDEX)
					player().m_angelic_dex = 0;
				else if (item->m_id_num == hb::shared::item::ItemId::AngelicPendantINT)
					player().m_angelic_int = 0;
				else if (item->m_id_num == hb::shared::item::ItemId::AngelicPendantMAG)
					player().m_angelic_mag = 0;
			}

			{
				auto pkt = hb::net::make_common_command(CommonType::ReleaseItem, player().m_player_x, player().m_player_y);
				pkt.v1 = item_id;
				send_game_packet(pkt);
			}
			m_game->m_is_item_equipped[item_id] = false;
			m_game->m_item_equipment_status[cfg->m_equip_pos] = -1;
			CursorTarget::clear_selection();
		}
	}

	return true;
}

PressResult DialogBox_Character::on_press()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	if (m_game->get_dialog_box_manager().is_enabled(DialogBoxId::ItemDropExternal))
		return PressResult::Normal;

	short sX = m_x;
	short sY = m_y;

	char equip_poi_status[DEF_MAXITEMEQUIPPOS];
	build_equip_status_array(equip_poi_status);

	char itemIdx = find_equip_item_at_point(mouse_x, mouse_y, sX, sY, equip_poi_status);
	if (itemIdx != -1)
	{
		CursorTarget::set_selection(SelectedObjectType::Item, static_cast<short>(itemIdx), 0, 0);
		return PressResult::ItemSelected;
	}

	return PressResult::Normal;
}

bool DialogBox_Character::on_item_drop()
{
	inventory_manager::get().equip_item(static_cast<char>(CursorTarget::get_selected_id()));
	return true;
}
