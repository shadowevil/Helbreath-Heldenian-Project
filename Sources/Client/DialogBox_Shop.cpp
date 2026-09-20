#include "DialogBox_Shop.h"
#include "Game.h"
#include "PacketSendHelpers.h"

#include "ShopManager.h"
#include "InventoryManager.h"
#include "ItemNameFormatter.h"
#include "ItemSpriteMetadata.h"
#include "IInput.h"
#include "lan_eng.h"
#include <format>
#include "GameFonts.h"
#include "TextLibExt.h"
#include "AudioManager.h"
#include "BalanceConstants.h"

using namespace hb::shared::net;
using namespace hb::shared::item;
using namespace hb::client::sprite_id;

DialogBox_Shop::DialogBox_Shop(CGame* game)
    : IDialogBox(DialogBoxId::SaleMenu, game)
{
    set_default_rect(70 , 50 , 258, 339);
}

void DialogBox_Shop::on_draw()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
	char lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;
    short sX = m_x;
    short sY = m_y;

    m_game->draw_new_dialog_box(InterfaceNdGame2, sX, sY, 2);
    m_game->draw_new_dialog_box(InterfaceNdText, sX, sY, 11);

    switch (m_mode) {
    case 0:
        draw_item_list(sX, sY);
        break;
    default:
        draw_item_details(sX, sY, mouse_x, mouse_y, z);
        break;
    }
}

void DialogBox_Shop::draw_item_list(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
	short z = static_cast<short>(hb::shared::input::get_mouse_wheel_delta());
	char lb = hb::shared::input::is_mouse_button_down(hb::shared::input::MouseButton::Left) ? 1 : 0;
    int total_lines = 0;
    int pointer_loc;
    double d1, d2, d3;
    char temp[255];
    int cost, discount_cost, discount_ratio;
    double tmp1, tmp2, tmp3;

    for (int i = 0; i < game_limits::max_menu_items; i++)
        if (shop_manager::get().get_item_list()[i] != 0) total_lines++;

    if (total_lines > 13) {
        d1 = static_cast<double>(m_scroll_offset);
        d2 = static_cast<double>(total_lines - 13);
        d3 = (274.0f * d1) / d2;
        pointer_loc = static_cast<int>(d3);
        m_game->draw_new_dialog_box(InterfaceNdGame2, sX, sY, 3);
        m_game->draw_new_dialog_box(InterfaceNdGame2, sX + 242, sY + pointer_loc + 35, 7);
    }
    else pointer_loc = 0;

    if (lb != 0 && total_lines > 13) {
        if ((m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::SaleMenu)) {
            if ((mouse_x >= sX + 235) && (mouse_x <= sX + 260) && (mouse_y >= sY + 10) && (mouse_y <= sY + 330)) {
                d1 = static_cast<double>(mouse_y - (sY + 35));
                d2 = static_cast<double>(total_lines - 13);
                d3 = (d1 * d2) / 274.0f;
                m_scroll_offset = static_cast<int>(d3 + 0.5);
            }
        }
    }
    else m_is_scroll_selected = false;

    if (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::SaleMenu && z != 0) {
        m_scroll_offset = m_scroll_offset - z / 60;

    }

    if (total_lines > 13 && m_scroll_offset > total_lines - 13)
        m_scroll_offset = total_lines - 13;
    if (m_scroll_offset < 0 || total_lines < 13)
        m_scroll_offset = 0;

    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 22, sY + 45, (sX + 165) - (sX + 22), 15, DRAW_DIALOGBOX_SHOP1, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter); // "ITEM"
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 23, sY + 45, (sX + 166) - (sX + 23), 15, DRAW_DIALOGBOX_SHOP1, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 153, sY + 45, (sX + 250) - (sX + 153), 15, DRAW_DIALOGBOX_SHOP3, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 154, sY + 45, (sX + 251) - (sX + 154), 15, DRAW_DIALOGBOX_SHOP3, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);

    // draw item names
    for (int i = 0; i < 13; i++)
        if (((i + m_scroll_offset) < game_limits::max_menu_items) &&
            (shop_manager::get().get_item_list()[i + m_scroll_offset] != 0)) {
            auto itemInfo = item_name_formatter::get().format(shop_manager::get().get_item_list()[i + m_scroll_offset].get());
            if ((mouse_x >= sX + 20) && (mouse_x <= sX + 220) && (mouse_y >= sY + i * 18 + 65) && (mouse_y <= sY + i * 18 + 79)) {
                hb::shared::text::draw_text_aligned(GameFont::Default, sX + 10, sY + i * 18 + 65, (sX + 190) - (sX + 10), 15, itemInfo.name.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
            }
            else hb::shared::text::draw_text_aligned(GameFont::Default, sX + 10, sY + i * 18 + 65, (sX + 190) - (sX + 10), 15, itemInfo.name.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
        }

    // draw prices
    for (int i = 0; i < 13; i++)
        if (((i + m_scroll_offset) < game_limits::max_menu_items) &&
            (shop_manager::get().get_item_list()[i + m_scroll_offset] != 0)) {
            discount_ratio = ((player().m_charisma - 10) / 4);
            tmp1 = static_cast<double>(discount_ratio);
            tmp2 = tmp1 / 100.0f;
            tmp1 = static_cast<double>(shop_manager::get().get_item_list()[i + m_scroll_offset]->m_sell_price);
            tmp3 = tmp1 * tmp2;
            discount_cost = static_cast<int>(tmp3);
            cost = static_cast<int>(shop_manager::get().get_item_list()[i + m_scroll_offset]->m_sell_price * ((100 + m_game->m_discount) / 100.));
            cost = cost - discount_cost;

            if (cost < static_cast<int>(shop_manager::get().get_item_list()[i + m_scroll_offset]->m_sell_price / 2))
                cost = static_cast<int>(shop_manager::get().get_item_list()[i + m_scroll_offset]->m_sell_price / 2) - 1;

            std::snprintf(temp, sizeof(temp), "%6d", cost);
            if ((mouse_x >= sX + 20) && (mouse_x <= sX + 220) && (mouse_y >= sY + i * 18 + 65) && (mouse_y <= sY + i * 18 + 79))
                hb::shared::text::draw_text_aligned(GameFont::Default, sX + 148, sY + i * 18 + 65, (sX + 260) - (sX + 148), 15, temp, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
            else hb::shared::text::draw_text_aligned(GameFont::Default, sX + 148, sY + i * 18 + 65, (sX + 260) - (sX + 148), 15, temp, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);
        }
}

int DialogBox_Shop::calculate_discounted_price(int item_index)
{
    int discount_ratio = ((player().m_charisma - 10) / 4);
    double tmp1 = static_cast<double>(discount_ratio);
    double tmp2 = tmp1 / 100.0f;
    tmp1 = static_cast<double>(shop_manager::get().get_item_list()[item_index]->m_sell_price);
    double tmp3 = tmp1 * tmp2;
    int discount_cost = static_cast<int>(tmp3);
    int cost = static_cast<int>(shop_manager::get().get_item_list()[item_index]->m_sell_price * ((100 + m_game->m_discount) / 100.));
    cost = cost - discount_cost;

    if (cost < static_cast<int>(shop_manager::get().get_item_list()[item_index]->m_sell_price / 2))
        cost = static_cast<int>(shop_manager::get().get_item_list()[item_index]->m_sell_price / 2) - 1;

    return cost;
}

void DialogBox_Shop::draw_item_details(short sX, short sY, short mouse_x, short mouse_y, short z)
{
    uint32_t time = m_game->m_cur_time;
    int item_index = m_mode - 1;
    bool flag_stat_low = false;
    bool flag_red_shown = false;

    auto shop_draw = m_game->get_item_draw(shop_manager::get().get_item_list()[item_index]->m_display_id, item_atlas::pack, shop_manager::get().get_item_list()[item_index]->sprite_is_female());
    shop_draw.sprite->draw(sX + 62 + 30 - 35, sY + 84 + 30 - 10, shop_draw.frame);

    auto itemInfo2 = item_name_formatter::get().format(shop_manager::get().get_item_list()[item_index].get());

    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 50, (sX + 240) - (sX + 25), 15, itemInfo2.name.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 50, (sX + 241) - (sX + 26), 15, itemInfo2.name.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);

    hb::shared::text::draw_text(GameFont::Default, sX + 90, sY + 78 + 30 - 10, DRAW_DIALOGBOX_SHOP3, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    hb::shared::text::draw_text(GameFont::Default, sX + 91, sY + 78 + 30 - 10, DRAW_DIALOGBOX_SHOP3, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    hb::shared::text::draw_text(GameFont::Default, sX + 90, sY + 93 + 30 - 10, DRAW_DIALOGBOX_SHOP6, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    hb::shared::text::draw_text(GameFont::Default, sX + 91, sY + 93 + 30 - 10, DRAW_DIALOGBOX_SHOP6, hb::shared::text::TextStyle::from_color(GameColors::UILabel));

    int cost = calculate_discounted_price(item_index);
    auto shopPrice = std::format(DRAW_DIALOGBOX_SHOP7, cost);
    hb::shared::text::draw_text(GameFont::Default, sX + 140, sY + 98, shopPrice.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel));

    float weight = CItem::weight_to_stones(shop_manager::get().get_item_list()[item_index]->m_weight);
    auto shopWeight = std::format(DRAW_DIALOGBOX_SHOP8, weight);
    hb::shared::text::draw_text(GameFont::Default, sX + 140, sY + 113, shopWeight.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel));

    switch (shop_manager::get().get_item_list()[item_index]->get_equip_pos()) {
    case EquipPos::RightHand:
    case EquipPos::TwoHand:
        draw_weapon_stats(sX, sY, item_index, flag_red_shown);
        break;

    case EquipPos::LeftHand:
        draw_shield_stats(sX, sY, item_index, flag_red_shown);
        break;

    case EquipPos::Head:
    case EquipPos::Body:
    case EquipPos::Boots:
    case EquipPos::Arms:
    case EquipPos::Leggings:
        draw_armor_stats(sX, sY, item_index, flag_stat_low, flag_red_shown);
        break;

    case EquipPos::None:
        break;
    }

    // Consumable restore display
    auto* shop_item = shop_manager::get().get_item_list()[item_index].get();
    auto shop_effect = shop_item->get_item_effect_type();
    if (is_consumable_effect_type(shop_effect))
    {
        auto range = parse_dice(shop_item->m_item_effect_value1, shop_item->m_item_effect_value2, shop_item->m_item_effect_value3);
        std::string restoreStr;
        if (shop_effect == ItemEffectType::HP)
            restoreStr = std::format(TOOLTIP_RESTORES_HP, range.min, range.max);
        else if (shop_effect == ItemEffectType::MP)
            restoreStr = std::format(TOOLTIP_RESTORES_MP, range.min, range.max);
        else if (shop_effect == ItemEffectType::SP)
            restoreStr = std::format(TOOLTIP_RESTORES_SP, range.min, range.max);
        else
            restoreStr = std::format(DRAW_DIALOGBOX_SHOP28, range.min, range.max);
        hb::shared::text::draw_text(GameFont::Default, sX + 40, sY + 145, restoreStr.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    }

    draw_level_requirement(sX, sY, item_index, flag_red_shown);
    draw_quantity_selector(sX, sY, mouse_x, mouse_y, z);

    // draw buttons
    if (mouse_in(btn_buy))
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 31);
    else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 30);

    if (mouse_in(btn_cancel))
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 17);
    else m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 16);
}

void DialogBox_Shop::draw_weapon_stats(short sX, short sY, int item_index, bool& flag_red_shown)
{
    char temp_buf[255];
    int temp;

    hb::shared::text::draw_text(GameFont::Default, sX + 90, sY + 145, DRAW_DIALOGBOX_SHOP9, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    hb::shared::text::draw_text(GameFont::Default, sX + 91, sY + 145, DRAW_DIALOGBOX_SHOP9, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    hb::shared::text::draw_text(GameFont::Default, sX + 40, sY + 160, DRAW_DIALOGBOX_SHOP10, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    hb::shared::text::draw_text(GameFont::Default, sX + 41, sY + 160, DRAW_DIALOGBOX_SHOP10, hb::shared::text::TextStyle::from_color(GameColors::UILabel));

    auto range = shop_manager::get().get_item_list()[item_index]->get_damage_range();
    auto damage_str = std::format(": {}-{}", range.min, range.max);
    hb::shared::text::draw_text(GameFont::Default, sX + 140, sY + 145, damage_str.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel));

    int str_divisor = hb::shared::balance::swing_str_divisor;
    int wups = hb::shared::balance::weight_units_per_stone;
    temp = shop_manager::get().get_item_list()[item_index]->m_weight / wups;
    std::snprintf(temp_buf, sizeof(temp_buf), ": %d(%d ~ %d)", shop_manager::get().get_item_list()[item_index]->m_swing_speed, temp, shop_manager::get().get_item_list()[item_index]->m_swing_speed * str_divisor);
    hb::shared::text::draw_text(GameFont::Default, sX + 140, sY + 160, temp_buf, hb::shared::text::TextStyle::from_color(GameColors::UILabel));

    if ((shop_manager::get().get_item_list()[item_index]->m_weight / wups) > player().m_str) {
        auto strWarn = std::format(DRAW_DIALOGBOX_SHOP11, (shop_manager::get().get_item_list()[item_index]->m_weight / wups));
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 258, (sX + 240) - (sX + 25), 15, strWarn.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 258, (sX + 241) - (sX + 26), 15, strWarn.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
        flag_red_shown = true;
    }
}

void DialogBox_Shop::draw_shield_stats(short sX, short sY, int item_index, bool& flag_red_shown)
{
    char temp[255];

    hb::shared::text::draw_text(GameFont::Default, sX + 90, sY + 145, DRAW_DIALOGBOX_SHOP12, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    hb::shared::text::draw_text(GameFont::Default, sX + 91, sY + 145, DRAW_DIALOGBOX_SHOP12, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    std::snprintf(temp, sizeof(temp), ": +%d%%", shop_manager::get().get_item_list()[item_index]->m_item_effect_value1);
    hb::shared::text::draw_text(GameFont::Default, sX + 140, sY + 145, temp, hb::shared::text::TextStyle::from_color(GameColors::UILabel));

    int wups = hb::shared::balance::weight_units_per_stone;
    if ((shop_manager::get().get_item_list()[item_index]->m_weight / wups) > player().m_str) {
        auto strWarn = std::format(DRAW_DIALOGBOX_SHOP11, (shop_manager::get().get_item_list()[item_index]->m_weight / wups));
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 258, (sX + 240) - (sX + 25), 15, strWarn.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 258, (sX + 241) - (sX + 26), 15, strWarn.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
        flag_red_shown = true;
    }
}

void DialogBox_Shop::draw_armor_stats(short sX, short sY, int item_index, bool& flag_stat_low, bool& flag_red_shown)
{
    char temp[255];

    hb::shared::text::draw_text(GameFont::Default, sX + 90, sY + 145, DRAW_DIALOGBOX_SHOP12, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    hb::shared::text::draw_text(GameFont::Default, sX + 91, sY + 145, DRAW_DIALOGBOX_SHOP12, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    std::snprintf(temp, sizeof(temp), ": +%d%%", shop_manager::get().get_item_list()[item_index]->m_item_effect_value1);
    hb::shared::text::draw_text(GameFont::Default, sX + 140, sY + 145, temp, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    flag_stat_low = false;

    std::string statReq;
    switch (shop_manager::get().get_item_list()[item_index]->m_item_effect_value4) {
    case 10://"Available for above Str %d"
        statReq = std::format(DRAW_DIALOGBOX_SHOP15, shop_manager::get().get_item_list()[item_index]->m_item_effect_value5);
        if (player().m_str >= shop_manager::get().get_item_list()[item_index]->m_item_effect_value5) {
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 160, (sX + 240) - (sX + 25), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 160, (sX + 241) - (sX + 26), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
        }
        else {
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 160, (sX + 240) - (sX + 25), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 160, (sX + 241) - (sX + 26), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
            flag_stat_low = true;
        }
        break;
    case 11: // "Available for above Dex %d"
        statReq = std::format(DRAW_DIALOGBOX_SHOP16, shop_manager::get().get_item_list()[item_index]->m_item_effect_value5);
        if (player().m_dex >= shop_manager::get().get_item_list()[item_index]->m_item_effect_value5) {
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 160, (sX + 240) - (sX + 25), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 160, (sX + 241) - (sX + 26), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
        }
        else {
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 160, (sX + 240) - (sX + 25), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 160, (sX + 241) - (sX + 26), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
            flag_stat_low = true;
        }
        break;
    case 12: // "Available for above Vit %d"
        statReq = std::format(DRAW_DIALOGBOX_SHOP17, shop_manager::get().get_item_list()[item_index]->m_item_effect_value5);
        if (player().m_vit >= shop_manager::get().get_item_list()[item_index]->m_item_effect_value5) {
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 160, (sX + 240) - (sX + 25), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 160, (sX + 241) - (sX + 26), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
        }
        else {
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 160, (sX + 240) - (sX + 25), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 160, (sX + 241) - (sX + 26), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
            flag_stat_low = true;
        }
        break;
    case 13: // "Available for above Int %d"
        statReq = std::format(DRAW_DIALOGBOX_SHOP18, shop_manager::get().get_item_list()[item_index]->m_item_effect_value5);
        if (player().m_int >= shop_manager::get().get_item_list()[item_index]->m_item_effect_value5) {
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 160, (sX + 240) - (sX + 25), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 160, (sX + 241) - (sX + 26), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
        }
        else {
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 160, (sX + 240) - (sX + 25), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 160, (sX + 241) - (sX + 26), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
            flag_stat_low = true;
        }
        break;
    case 14: // "Available for above Mag %d"
        statReq = std::format(DRAW_DIALOGBOX_SHOP19, shop_manager::get().get_item_list()[item_index]->m_item_effect_value5);
        if (player().m_mag >= shop_manager::get().get_item_list()[item_index]->m_item_effect_value5) {
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 160, (sX + 240) - (sX + 25), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 160, (sX + 241) - (sX + 26), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
        }
        else {
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 160, (sX + 240) - (sX + 25), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 160, (sX + 241) - (sX + 26), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
            flag_stat_low = true;
        }
        break;
    case 15: // "Available for above Chr %d"
        statReq = std::format(DRAW_DIALOGBOX_SHOP20, shop_manager::get().get_item_list()[item_index]->m_item_effect_value5);
        if (player().m_charisma >= shop_manager::get().get_item_list()[item_index]->m_item_effect_value5) {
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 160, (sX + 240) - (sX + 25), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 160, (sX + 241) - (sX + 26), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel), hb::shared::text::Align::TopCenter);
        }
        else {
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 160, (sX + 240) - (sX + 25), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 160, (sX + 241) - (sX + 26), 15, statReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
            flag_stat_low = true;
        }
        break;
    default:
        break;
    }

    int wups = hb::shared::balance::weight_units_per_stone;
    if ((shop_manager::get().get_item_list()[item_index]->m_weight / wups) > player().m_str) {
        auto strWarn2 = std::format(DRAW_DIALOGBOX_SHOP11, (shop_manager::get().get_item_list()[item_index]->m_weight / wups));
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 288, (sX + 240) - (sX + 25), 15, strWarn2.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 288, (sX + 241) - (sX + 26), 15, strWarn2.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
        flag_red_shown = true;
    }
    else if (flag_stat_low == true) {
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 258, (sX + 240) - (sX + 25), 15, DRAW_DIALOGBOX_SHOP21, hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 258, (sX + 241) - (sX + 26), 15, DRAW_DIALOGBOX_SHOP21, hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
        flag_red_shown = true;
    }
    else if ((strstr(shop_manager::get().get_item_list()[item_index]->m_name, "(M)") != 0)
        && (player().m_player_type > 3)) {
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 258, (sX + 240) - (sX + 25), 15, DRAW_DIALOGBOX_SHOP22, hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 258, (sX + 241) - (sX + 26), 15, DRAW_DIALOGBOX_SHOP22, hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
        flag_red_shown = true;
    }
    else if ((strstr(shop_manager::get().get_item_list()[item_index]->m_name, "(W)") != 0)
        && (player().m_player_type <= 3)) {
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 258, (sX + 240) - (sX + 25), 15, DRAW_DIALOGBOX_SHOP23, hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 26, sY + 258, (sX + 241) - (sX + 26), 15, DRAW_DIALOGBOX_SHOP23, hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
        flag_red_shown = true;
    }
}

void DialogBox_Shop::draw_level_requirement(short sX, short sY, int item_index, bool& flag_red_shown)
{
    if (shop_manager::get().get_item_list()[item_index]->m_level_requirement != 0) {
        if (player().m_level >= shop_manager::get().get_item_list()[item_index]->m_level_requirement) {
            hb::shared::text::draw_text(GameFont::Default, sX + 90, sY + 190, DRAW_DIALOGBOX_SHOP24, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
            hb::shared::text::draw_text(GameFont::Default, sX + 91, sY + 190, DRAW_DIALOGBOX_SHOP24, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
            auto lvlReq = std::format(DRAW_DIALOGBOX_SHOP25, shop_manager::get().get_item_list()[item_index]->m_level_requirement);
            hb::shared::text::draw_text(GameFont::Default, sX + 140, sY + 190, lvlReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UILabel));
        }
        else {
            hb::shared::text::draw_text(GameFont::Default, sX + 90, sY + 190, DRAW_DIALOGBOX_SHOP24, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
            hb::shared::text::draw_text(GameFont::Default, sX + 91, sY + 190, DRAW_DIALOGBOX_SHOP24, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
            auto lvlReq = std::format(DRAW_DIALOGBOX_SHOP25, shop_manager::get().get_item_list()[item_index]->m_level_requirement);
            hb::shared::text::draw_text(GameFont::Default, sX + 140, sY + 190, lvlReq.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed));
            if (flag_red_shown == false) {
                hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25, sY + 258, (sX + 240) - (sX + 25), 15, DRAW_DIALOGBOX_SHOP26, hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
                hb::shared::text::draw_text_aligned(GameFont::Default, sX + 25 + 1, sY + 258, (sX + 240 + 1) - (sX + 25 + 1), 15, DRAW_DIALOGBOX_SHOP26, hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
                flag_red_shown = true;
            }
        }
    }
}

int DialogBox_Shop::get_max_quantity() const
{
	if (m_mode <= 0) return 1;
	int item_index = m_mode - 1;
	CItem* shop_item = shop_manager::get().get_item_list()[item_index].get();
	if (shop_item != nullptr && shop_item->is_stackable())
		return 9999;
	return (50 - inventory_manager::get().get_total_item_count());
}

void DialogBox_Shop::draw_quantity_selector(short sX, short sY, short mouse_x, short mouse_y, short z)
{
    char temp[16];
    int max_qty = get_max_quantity();

    // 4 up buttons
    m_game->m_sprite[InterfaceNdGame2]->draw(sX + 128, sY + 219, 19);
    m_game->m_sprite[InterfaceNdGame2]->draw(sX + 142, sY + 219, 19);
    m_game->m_sprite[InterfaceNdGame2]->draw(sX + 156, sY + 219, 19);
    m_game->m_sprite[InterfaceNdGame2]->draw(sX + 170, sY + 219, 19);

    // "Quantity:" label
    hb::shared::text::draw_text(GameFont::Default, sX + 80, sY + 227, DRAW_DIALOGBOX_SHOP27, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    hb::shared::text::draw_text(GameFont::Default, sX + 81, sY + 227, DRAW_DIALOGBOX_SHOP27, hb::shared::text::TextStyle::from_color(GameColors::UILabel));

    // Mouse wheel quantity adjustment
    if (m_game->get_dialog_box_manager().get_top_id() == DialogBoxId::SaleMenu && z != 0) {
        m_quantity = m_quantity + z / 60;
    }

    if (m_quantity > max_qty)
        m_quantity = max_qty;
    if (m_quantity < 1)
        m_quantity = 1;

    // Draw 4 digits (zero-padded)
    std::snprintf(temp, sizeof(temp), "%04d", m_quantity);
    constexpr int digit_x[] = {137, 151, 165, 179};
    for (int i = 0; i < 4; i++)
    {
        char digit[2] = { temp[i], '\0' };
        hb::shared::text::draw_text(GameFont::Default, sX + digit_x[i], sY + 227, digit, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
        hb::shared::text::draw_text(GameFont::Default, sX + digit_x[i] + 1, sY + 227, digit, hb::shared::text::TextStyle::from_color(GameColors::UILabel));
    }

    // 4 down buttons
    m_game->m_sprite[InterfaceNdGame2]->draw(sX + 128, sY + 244, 20);
    m_game->m_sprite[InterfaceNdGame2]->draw(sX + 142, sY + 244, 20);
    m_game->m_sprite[InterfaceNdGame2]->draw(sX + 156, sY + 244, 20);
    m_game->m_sprite[InterfaceNdGame2]->draw(sX + 170, sY + 244, 20);
}

bool DialogBox_Shop::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
    short sX = m_x;
    short sY = m_y;

    switch (m_mode) {
    case 0:
        return on_click_item_list(sX, sY);
    default:
        return on_click_item_details(sX, sY);
    }
    return false;
}

bool DialogBox_Shop::on_click_item_list(short sX, short sY)
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
    for (int i = 0; i < 13; i++)
        if ((mouse_x >= sX + 20) && (mouse_x <= sX + 220) && (mouse_y >= sY + i * 18 + 65) && (mouse_y <= sY + i * 18 + 79)) {
            if (inventory_manager::get().get_total_item_count() >= 50) {
                m_game->add_event_list(DLGBOX_CLICK_SHOP1, 10);//"You cannot buy anything because your bag is full."
                return true;
            }

            audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
            if (shop_manager::get().get_item_list()[m_scroll_offset + i] != 0)
                m_mode = m_scroll_offset + i + 1;
            return true;
        }
    return false;
}

bool DialogBox_Shop::on_click_item_details(short sX, short sY)
{
    char temp[hb::shared::limits::ItemNameLen];

    int max_qty = get_max_quantity();

    // +1000 quantity button
    if (mouse_in(btn_qty_up_1000)) {
        m_quantity += 1000;
        if (m_quantity > max_qty) m_quantity = max_qty;
        return true;
    }

    // -1000 quantity button
    if (mouse_in(btn_qty_down_1000)) {
        m_quantity -= 1000;
        if (m_quantity < 1) m_quantity = 1;
        return true;
    }

    // +100 quantity button
    if (mouse_in(btn_qty_up_100)) {
        m_quantity += 100;
        if (m_quantity > max_qty) m_quantity = max_qty;
        return true;
    }

    // -100 quantity button
    if (mouse_in(btn_qty_down_100)) {
        m_quantity -= 100;
        if (m_quantity < 1) m_quantity = 1;
        return true;
    }

    // +10 quantity button
    if (mouse_in(btn_qty_up_10)) {
        m_quantity += 10;
        if (m_quantity > max_qty) m_quantity = max_qty;
        return true;
    }

    // -10 quantity button
    if (mouse_in(btn_qty_down_10)) {
        m_quantity -= 10;
        if (m_quantity < 1) m_quantity = 1;
        return true;
    }

    // +1 quantity button
    if (mouse_in(btn_qty_up_1)) {
        m_quantity++;
        if (m_quantity > max_qty) m_quantity = max_qty;
        return true;
    }

    // -1 quantity button
    if (mouse_in(btn_qty_down_1)) {
        m_quantity--;
        if (m_quantity < 1) m_quantity = 1;
        return true;
    }

    // Purchase button
    if (mouse_in(btn_buy)) {
        if ((50 - inventory_manager::get().get_total_item_count()) < m_quantity) {
            m_game->add_event_list(DLGBOX_CLICK_SHOP1, 10);//"ou cannot buy anything because your bag is full."
        }
        else {
            std::memset(temp, 0, sizeof(temp));
            CItem* shop_item = shop_manager::get().get_item_list()[m_mode - 1].get();
            std::snprintf(temp, sizeof(temp), "%s", shop_item->m_name);
            // Send item ID in v2 for reliable item lookup on server
            int item_id = shop_item->m_id_num;
            {
            	auto pkt = hb::net::make_common_command_str(CommonType::ReqPurchaseItem, m_game->m_player->m_player_x, m_game->m_player->m_player_y);
            	pkt.v1 = m_quantity;
            	pkt.v2 = item_id;
            	std::snprintf(pkt.text, sizeof(pkt.text), "%s", temp);
            	m_game->send_game_packet(pkt);
            }
        }
        m_mode = 0;
        m_quantity = 1;
        audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
        return true;
    }

    // Cancel button
    if (mouse_in(btn_cancel)) {
        m_mode = 0;
        m_quantity = 1;
        audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
        return true;
    }

    return false;
}

PressResult DialogBox_Shop::on_press()
{
    // Only claim scroll in item list mode (mode == 0)
    if (m_mode == 0)
    {
        if (mouse_in(area_scroll))
        {
            return PressResult::ScrollClaimed;
        }
    }

    return PressResult::Normal;
}

bool DialogBox_Shop::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	switch (type) {
	case 0:
		break;
	default:
		if (shop_manager::get().has_items()) {
			m_npc_id = type;
			m_mode = 0;
			m_scroll_offset = 0;
			m_items_loaded = true;
			m_quantity = 1;
		} else {
			shop_manager::get().set_pending_npc_config_id(type);
			shop_manager::get().request_shop_menu(type);
		}
		break;
	}
	return true;
}

bool DialogBox_Shop::on_disable()
{
	shop_manager::get().clear_items();
	auto& give = m_game->get_dialog_box_manager().m_give_item;
	give.action_type = 0;
	give.object_id = 0;
	give.target_x = 0;
	give.target_y = 0;
	return true;
}
