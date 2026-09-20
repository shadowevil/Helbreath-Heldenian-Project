#include "DialogBox_ItemUpgrade.h"
#include "CursorTarget.h"
#include "Game.h"
#include "PacketSendHelpers.h"

#include "InventoryManager.h"
#include "ItemNameFormatter.h"
#include "ItemSpriteMetadata.h"
#include "lan_eng.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include <format>
#include <string>
#include "IInput.h"
#include "Screen_OnGame.h"
#include "AudioManager.h"

using namespace hb::shared::net;
using namespace hb::shared::item;
using namespace hb::client::sprite_id;

DialogBox_ItemUpgrade::DialogBox_ItemUpgrade(CGame* game)
	: IDialogBox(DialogBoxId::ItemUpgrade, game)
{
	set_default_rect(60 , 50 , 258, 339);
	m_can_close_on_right_click = false;
}

void DialogBox_ItemUpgrade::on_draw()
{
    if (!m_game->ensure_item_configs_loaded()) return;
    int sX = m_x;
    int sY = m_y;

    m_game->draw_new_dialog_box(InterfaceNdGame2, sX, sY, 0);
    m_game->draw_new_dialog_box(InterfaceNdText, sX, sY, 5); // Item Upgrade Text

    switch (m_mode) {
    case mode::gizon_upgrade:        DrawMode1_GizonUpgrade(sX, sY); break;
    case mode::in_progress:          DrawMode2_InProgress(sX, sY); break;
    case mode::success:              DrawMode3_Success(sX, sY); break;
    case mode::failed:               DrawMode4_Failed(sX, sY); break;
    case mode::select_upgrade_type:  DrawMode5_SelectUpgradeType(sX, sY); break;
    case mode::stone_upgrade:        DrawMode6_StoneUpgrade(sX, sY); break;
    case mode::item_lost:            DrawMode7_ItemLost(sX, sY); break;
    case mode::max_upgrade:          DrawMode8_MaxUpgrade(sX, sY); break;
    case mode::cannot_upgrade:       DrawMode9_CannotUpgrade(sX, sY); break;
    case mode::no_points:            DrawMode10_NoPoints(sX, sY); break;
    }
}

int DialogBox_ItemUpgrade::calculate_upgrade_cost(int item_index)
{
    int value = player().m_item_list[item_index]->m_instance.enchant_bonus;
    value = value * (value + 6) / 8 + 2;

    // Special handling for Angelic Pendants
    CItem* cfg = m_game->get_item_config(player().m_item_list[item_index]->m_id_num);
    if (cfg && (cfg->get_equip_pos() >= EquipPos::LeftFinger)
        && (cfg->get_item_type() == hb::shared::item::item_type::equipment))
    {
        short id = player().m_item_list[item_index]->m_id_num;
        if (id == hb::shared::item::ItemId::AngelicPendantSTR || id == hb::shared::item::ItemId::AngelicPendantDEX ||
            id == hb::shared::item::ItemId::AngelicPendantINT || id == hb::shared::item::ItemId::AngelicPendantMAG)
        {
            value = player().m_item_list[item_index]->m_instance.enchant_bonus;
            switch (value) {
            case 0: value = 10; break;
            case 1: value = 11; break;
            case 2: value = 13; break;
            case 3: value = 16; break;
            case 4: value = 20; break;
            case 5: value = 25; break;
            case 6: value = 31; break;
            case 7: value = 38; break;
            case 8: value = 46; break;
            case 9: value = 55; break;
            }
        }
    }
    return value;
}

void DialogBox_ItemUpgrade::draw_item_preview(int sX, int sY, int item_index)
{
    char item_color = player().m_item_list[item_index]->m_instance.item_color;
    CItem* cfg = m_game->get_item_config(player().m_item_list[item_index]->m_id_num);
    if (!cfg) return;

    auto upg_draw = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
    if (item_color == 0)
    {
        upg_draw.sprite->draw(sX + 134, sY + 182, upg_draw.frame);
    }
    else
    {
        const auto& upg_tint = m_game->m_color_palette[item_color];
        upg_draw.sprite->draw(sX + 134, sY + 182, upg_draw.frame, hb::shared::sprite::DrawParams::tint(upg_tint.r, upg_tint.g, upg_tint.b));
    }

    auto itemInfo = item_name_formatter::get().format(player().m_item_list[item_index].get());
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 230 + 20, (sX + 248) - (sX + 24), 15, itemInfo.name.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    auto effect = itemInfo.effect_text();
    auto extra = itemInfo.extra_text();
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 245 + 20, (sX + 248) - (sX + 24), 15, effect.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 260 + 20, (sX + 248) - (sX + 24), 15, extra.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
}

void DialogBox_ItemUpgrade::DrawMode1_GizonUpgrade(int sX, int sY)
{
    short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
    short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
    uint32_t time = m_game->m_cur_time;
    int item_index = m_selected_item_index;
    std::string txt;

    m_game->draw_new_dialog_box(InterfaceNdGame3, sX, sY, 3);
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 20 + 30, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE1, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 20 + 45, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE2, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 20 + 60, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE3, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 46);

    txt = std::format(DRAW_DIALOGBOX_ITEMUPGRADE11, m_game->on_game()->m_gizon_item_upgrade_left);
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 100, (sX + 248) - (sX + 24), 15, txt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);

    if (item_index != -1)
    {
        m_game->draw_new_dialog_box(InterfaceNdGame3, sX, sY, 3);
        int value = calculate_upgrade_cost(item_index);

        txt = std::format(DRAW_DIALOGBOX_ITEMUPGRADE12, value);
        if (m_game->on_game()->m_gizon_item_upgrade_left < value)
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 115, (sX + 248) - (sX + 24), 15, txt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
        else
            hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 115, (sX + 248) - (sX + 24), 15, txt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);

        draw_item_preview(sX, sY, item_index);

        if (m_game->on_game()->m_gizon_item_upgrade_left < value)
            m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 46);
        else
        {
            if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
                m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 47);
            else
                m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 46);
        }
    }
    else
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 46);

    // Cancel button
    if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 17);
    else
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 16);
}

void DialogBox_ItemUpgrade::DrawMode2_InProgress(int sX, int sY)
{
    short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
    short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
    uint32_t time = m_game->m_cur_time;
    int item_index = m_selected_item_index;

    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 55 + 30 + 282 - 117 - 170, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE5, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 55 + 45 + 282 - 117 - 170, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE6, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);

    if (item_index != -1)
    {
        m_game->draw_new_dialog_box(InterfaceNdGame3, sX, sY, 3);
        char item_color = player().m_item_list[item_index]->m_instance.item_color;
        CItem* cfg = m_game->get_item_config(player().m_item_list[item_index]->m_id_num);

        if (cfg)
        {
            auto upg2_draw = m_game->get_item_draw(cfg->m_display_id, item_atlas::pack, cfg->sprite_is_female());
            if (item_color == 0)
            {
                upg2_draw.sprite->draw(sX + 134, sY + 182, upg2_draw.frame);
            }
            else
            {
                const auto& upg2_tint = m_game->m_color_palette[item_color];
                upg2_draw.sprite->draw(sX + 134, sY + 182, upg2_draw.frame, hb::shared::sprite::DrawParams::tint(upg2_tint.r, upg2_tint.g, upg2_tint.b));
            }

            // Flickering effect
            if ((rand() % 5) == 0)
                upg2_draw.sprite->draw(sX + 134, sY + 182, upg2_draw.frame, hb::shared::sprite::DrawParams::alpha_blend(0.25f));
        }

        auto itemInfo2 = item_name_formatter::get().format(player().m_item_list[item_index].get());
        auto effect2 = itemInfo2.effect_text();
        auto extra2 = itemInfo2.extra_text();
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 230 + 20, (sX + 248) - (sX + 24), 15, itemInfo2.name.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 245 + 20, (sX + 248) - (sX + 24), 15, effect2.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 260 + 20, (sX + 248) - (sX + 24), 15, extra2.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }

    // Send upgrade command after 4 seconds
    if (((time - m_upgrade_start_time) / 1000 > 4)
        && (m_upgrade_start_time != 0))
    {
        m_upgrade_start_time = 0;
        {
        	auto pkt = hb::net::make_common_command(CommonType::UpgradeItem, m_game->m_player->m_player_x, m_game->m_player->m_player_y);
        	pkt.v1 = item_index;
        	m_game->send_game_packet(pkt);
        }
    }
}

void DialogBox_ItemUpgrade::DrawMode3_Success(int sX, int sY)
{
    short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
    short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
    int item_index = m_selected_item_index;

    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 55 + 30 + 282 - 117 - 170, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE7, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 55 + 45 + 282 - 117 - 170, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE8, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);

    if (item_index != -1)
    {
        m_game->draw_new_dialog_box(InterfaceNdGame3, sX, sY, 3);
        draw_item_preview(sX, sY, item_index);
    }

    // OK button
    if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
    else
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
}

void DialogBox_ItemUpgrade::DrawMode4_Failed(int sX, int sY)
{
    short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
    short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
    int item_index = m_selected_item_index;

    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 55 + 30 + 282 - 117 - 170, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE9, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);

    // Check if item was destroyed
    if ((item_index != -1) && (player().m_item_list[item_index] == 0))
    {
        audio_manager::get().play_game_sound(sound_type::effect, 24, 0, 0);
        m_mode = mode::item_lost;
        return;
    }

    if (item_index != -1)
    {
        m_game->draw_new_dialog_box(InterfaceNdGame3, sX, sY, 3);
        draw_item_preview(sX, sY, item_index);
    }

    // OK button
    if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
    else
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
}

void DialogBox_ItemUpgrade::DrawMode5_SelectUpgradeType(int sX, int sY)
{
    short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
    short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 20 + 45, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE13, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);

    // Normal item upgrade option
    if (mouse_in(link_normal_upgrade))
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 100, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE14, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 150, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE16, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 165, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE17, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 180, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE18, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 195, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE19, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 210, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE20, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 225, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE21, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 255, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE26, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 270, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE27, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    else
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 100, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE14, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);

    // Majestic item upgrade option
    if (mouse_in(link_majestic_upgrade))
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 120, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE15, hb::shared::text::TextStyle::from_color(GameColors::UIWhite), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 150, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE22, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 165, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE23, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 180, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE24, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 195, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE25, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 225, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE28, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 240, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE29, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }
    else
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 120, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE15, hb::shared::text::TextStyle::from_color(GameColors::UIMagicBlue), hb::shared::text::Align::TopCenter);

    // Cancel button
    if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 17);
    else
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 16);
}

void DialogBox_ItemUpgrade::DrawMode6_StoneUpgrade(int sX, int sY)
{
    short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
    short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
    int item_index = m_selected_item_index;
    int so_x = m_stone_xelima_count;
    int so_m = m_stone_merien_count;
    std::string txt;

    m_game->draw_new_dialog_box(InterfaceNdGame3, sX, sY, 3);
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 20 + 30, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE31, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 20 + 45, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE32, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 20 + 60, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE33, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);

    if (so_x == 0)
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 20 + 80, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE41, hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
    }
    else
    {
        txt = std::format(DRAW_DIALOGBOX_ITEMUPGRADE34, so_x);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 20 + 80, (sX + 248) - (sX + 24), 15, txt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }

    if (so_m == 0)
    {
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 20 + 95, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE42, hb::shared::text::TextStyle::from_color(GameColors::UIWarningRed), hb::shared::text::Align::TopCenter);
    }
    else
    {
        txt = std::format(DRAW_DIALOGBOX_ITEMUPGRADE35, so_m);
        hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 20 + 95, (sX + 248) - (sX + 24), 15, txt.c_str(), hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    }

    m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 46);

    if (item_index != -1)
    {
        m_game->draw_new_dialog_box(InterfaceNdGame3, sX, sY, 3);
        draw_item_preview(sX, sY, item_index);

        if ((mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
            m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 47);
        else
            m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::left_btn_x, sY + ui_layout::btn_y, 46);
    }

    // Cancel button
    if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 17);
    else
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 16);
}

void DialogBox_ItemUpgrade::DrawMode7_ItemLost(int sX, int sY)
{
    short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
    short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 20 + 130, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE36, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 20 + 145, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE37, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);

    // OK button
    if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
    else
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
}

void DialogBox_ItemUpgrade::DrawMode8_MaxUpgrade(int sX, int sY)
{
    short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
    short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 20 + 130, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE38, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);

    // OK button
    if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
    else
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
}

void DialogBox_ItemUpgrade::DrawMode9_CannotUpgrade(int sX, int sY)
{
    short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
    short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 20 + 130, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE39, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);

    // OK button
    if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
    else
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
}

void DialogBox_ItemUpgrade::DrawMode10_NoPoints(int sX, int sY)
{
    short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
    short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
    hb::shared::text::draw_text_aligned(GameFont::Default, sX + 24, sY + 20 + 130, (sX + 248) - (sX + 24), 15, DRAW_DIALOGBOX_ITEMUPGRADE40, hb::shared::text::TextStyle::from_color(GameColors::UIBlack), hb::shared::text::Align::TopCenter);

    // OK button
    if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x) && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 1);
    else
        m_game->draw_new_dialog_box(InterfaceNdButton, sX + ui_layout::right_btn_x, sY + ui_layout::btn_y, 0);
}

bool DialogBox_ItemUpgrade::on_click()
{
	short mouse_x = static_cast<short>(hb::shared::input::get_mouse_x());
	short mouse_y = static_cast<short>(hb::shared::input::get_mouse_y());
    short sX = m_x;
    short sY = m_y;
    int item_index = m_selected_item_index;

    switch (m_mode) {
    case mode::gizon_upgrade:
        if ((item_index != -1) && (mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x)
            && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
        {
            int value = calculate_upgrade_cost(item_index);
            if (m_game->on_game()->m_gizon_item_upgrade_left < value) break;

            audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
            audio_manager::get().play_game_sound(sound_type::effect, 44, 0);
            m_mode = mode::in_progress;
            m_upgrade_start_time = m_game->m_cur_time;
            return true;
        }
        if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x)
            && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
        {
            audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
            m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::ItemUpgrade);
            return true;
        }
        break;

    case mode::success:
    case mode::failed:
    case mode::item_lost:
    case mode::max_upgrade:
    case mode::cannot_upgrade:
    case mode::no_points:
    case mode::need_stone:
        if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x)
            && (mouse_y > sY + ui_layout::btn_y) && (mouse_y < sY + ui_layout::btn_y + ui_layout::btn_size_y))
        {
            audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
            m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::ItemUpgrade);
            return true;
        }
        break;

    case mode::select_upgrade_type:
        // Normal item upgrade (Stone)
        if (mouse_in(link_normal_upgrade))
        {
            audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
            int so_x = 0, so_m = 0;
            for (int i = 0; i < hb::shared::limits::MaxItems; i++)
                if (player().m_item_list[i] != 0)
                {
                    CItem* cfg = m_game->get_item_config(player().m_item_list[i]->m_id_num);
                    if (!cfg) continue;
                    if (player().m_item_list[i]->m_id_num == 656) so_x++; // Stone of Xelima
                    if (player().m_item_list[i]->m_id_num == 657) so_m++; // Stone of Merien
                }

            if ((so_x > 0) || (so_m > 0))
            {
                m_mode = mode::stone_upgrade;
                m_stone_xelima_count = so_x;
                m_stone_merien_count = so_m;
            }
            else
            {
                m_game->add_event_list(DRAW_DIALOGBOX_ITEMUPGRADE30, 10);
                m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::ItemUpgrade);
            }
            return true;
        }
        // Majestic item upgrade (Gizon)
        if (mouse_in(link_majestic_upgrade))
        {
            audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
            if (m_game->on_game()->m_gizon_item_upgrade_left > 0)
            {
                m_mode = mode::gizon_upgrade;
            }
            else
            {
                m_game->add_event_list(DRAW_DIALOGBOX_ITEMUPGRADE40, 10);
                m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::ItemUpgrade);
            }
            return true;
        }
        // Cancel
        if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x)
            && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
        {
            audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
            m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::ItemUpgrade);
            return true;
        }
        break;

    case mode::stone_upgrade:
        if ((item_index != -1) && (mouse_x >= sX + ui_layout::left_btn_x) && (mouse_x <= sX + ui_layout::left_btn_x + ui_layout::btn_size_x)
            && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
        {
            audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
            audio_manager::get().play_game_sound(sound_type::effect, 44, 0);
            m_mode = mode::in_progress;
            m_upgrade_start_time = m_game->m_cur_time;
            return true;
        }
        if ((mouse_x >= sX + ui_layout::right_btn_x) && (mouse_x <= sX + ui_layout::right_btn_x + ui_layout::btn_size_x)
            && (mouse_y >= sY + ui_layout::btn_y) && (mouse_y <= sY + ui_layout::btn_y + ui_layout::btn_size_y))
        {
            audio_manager::get().play_game_sound(sound_type::effect, 14, 5);
            m_game->get_dialog_box_manager().disable_dialog_box(DialogBoxId::ItemUpgrade);
            return true;
        }
        break;
    }

    return false;
}

bool DialogBox_ItemUpgrade::on_item_drop()
{
	int item_id = CursorTarget::get_selected_id();
	if (item_id < 0 || item_id >= hb::shared::limits::MaxItems) return false;
	if (inventory_manager::get().warn_if_locked(item_id)) return false;
	if (player().m_Controller.get_command() < 0) return false;
	CItem* cfg = m_game->get_item_config(player().m_item_list[item_id]->m_id_num);
	if (!cfg || cfg->get_equip_pos() == EquipPos::None) return false;

	switch (m_mode) {
	case mode::gizon_upgrade:
		inventory_manager::get().unlock_item(m_selected_item_index);
		m_selected_item_index = item_id;
		inventory_manager::get().lock_item(item_id);
		audio_manager::get().play_game_sound(sound_type::effect, 29, 0);
		break;

	case mode::stone_upgrade:
		inventory_manager::get().unlock_item(m_selected_item_index);
		m_selected_item_index = item_id;
		inventory_manager::get().lock_item(item_id);
		audio_manager::get().play_game_sound(sound_type::effect, 29, 0);
		break;
	}

	return true;
}

bool DialogBox_ItemUpgrade::on_enable(int type, int64_t v1, int v2, const char* string)
{
	if (is_enabled()) return true;
	m_mode = static_cast<mode>(type);
	m_selected_item_index = -1;
	m_upgrade_start_time = 0;
	return true;
}

bool DialogBox_ItemUpgrade::on_disable()
{
	{ int idx = m_selected_item_index;
	inventory_manager::get().unlock_item(idx); }
	return true;
}
