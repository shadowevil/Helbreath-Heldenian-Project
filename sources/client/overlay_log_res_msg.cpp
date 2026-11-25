// overlay_log_res_msg.cpp: Login/Account result message overlay
//
//////////////////////////////////////////////////////////////////////

#include "overlay_log_res_msg.h"
#include "game.h"
#include "game_mode_manager.h"
#include "common_types.h"
#include "lan_eng.h"
#include "input.h"
#include "audio_manager.h"
#include "text_lib_ext.h"
#include "game_fonts.h"
#include <format>
#include <string>
#include <string_view>
using namespace hb::client::sprite_id;

Overlay_LogResMsg::Overlay_LogResMsg(CGame* game)
    : IGameScreen(game)
    , m_cReturnDest('1')
    , m_cMsgCode('1')
{
}

void Overlay_LogResMsg::on_initialize()
{
    // Read parameters from CGame (set before change_game_mode call)
    m_cReturnDest = m_game->m_msg[0];
    m_cMsgCode = m_game->m_msg[1];

    // stop any playing sound
    audio_manager::get().stop_sound(sound_type::effect, 38);

    int dlgX, dlgY;
    get_centered_dialog_pos(InterfaceNdGame4, 2, dlgX, dlgY);

    m_controls.set_screen_size(LOGICAL_WIDTH(), LOGICAL_HEIGHT());
    m_controls.set_hover_focus(false);
    m_controls.set_default_button(BTN_OK);

    auto* btn_ok = m_controls.add<cc::button>(BTN_OK, cc::rect{dlgX + 208, dlgY + 119, ui_layout::btn_size_x, ui_layout::btn_size_y});
    btn_ok->set_click_sound([this] { audio_manager::get().play_game_sound(sound_type::effect, 14, 5); });
    btn_ok->set_on_click([this](int) {
        handle_dismiss();
    });
    btn_ok->set_render_handler([this](const cc::control& c) {
        auto sb = c.screen_bounds();
        int frame = c.is_highlighted() ? 1 : 0;
        m_game->draw_new_dialog_box(InterfaceNdButton, sb.x, sb.y, frame);
    });

    m_controls.set_focus_order({BTN_OK});
    m_controls.set_focus(BTN_OK);

    // Discard stale key state so Enter held from the previous screen
    // doesn't immediately fire the OK button on the first frame.
    discard_pending_controls_input(m_controls);
}
void Overlay_LogResMsg::handle_dismiss()
{
    // Note: clear_overlay() is not needed here as change_game_mode will either:
    // - Call set_screen<T>() which clears overlays automatically
    // - Call set_overlay<T>() which clears existing overlays automatically

    // Transition based on return destination
    switch (m_cReturnDest)
    {
    case '0':
        m_game->change_game_mode(GameMode::CreateNewAccount);
        break;
    case '1':
        m_game->change_game_mode(GameMode::MainMenu);
        break;
    case '2':
        m_game->change_game_mode(GameMode::CreateNewCharacter);
        break;
    case '3':
    case '4':
        m_game->change_game_mode(GameMode::SelectCharacter);
        break;
    case '5':
        m_game->change_game_mode(GameMode::MainMenu);
        break;
    case '6':
        // Context-dependent based on message code
        switch (m_cMsgCode)
        {
        case 'B':
            m_game->change_game_mode(GameMode::MainMenu);
            break;
        case 'C':
        case 'M':
            m_game->change_game_mode(GameMode::ChangePassword);
            break;
        default:
            m_game->change_game_mode(GameMode::MainMenu);
            break;
        }
        break;
    case '7':
    case '8':
    default:
        m_game->change_game_mode(GameMode::MainMenu);
        break;
    }
}

void Overlay_LogResMsg::on_update()
{
    update_controls(m_controls);

    // ESC also dismisses
    if (m_controls.escape_pressed())
    {
        handle_dismiss();
        return;
    }
}

// The kit title of a message code; empty for the codes that show only body lines
static std::string_view message_title(char code)
{
	switch (code)
	{
	case '1': return "Password is not correct!";
	case '2': return "Not existing account!";
	case '3': return "Can not connect to game server!";
	case '4': return "New account created.";
	case '5': return "Can not create new account!";
	case '6': return "Can not create new account!";
	case '7': return "New character created.";
	case '8': return "Can not create new character!";
	case '9': return "Can not create new character!";
	case 'A': return "Character deleted.";
	case 'B': return "Password changed.";
	case 'C': return "Can not change password!";
	case 'D':
	case 'E':
	case 'F':
	case 'G': return "Can not connect to game server!";
	case 'H': return "Connection Rejected!";
	case 'I': return "Not Enough Point!";
	case 'J': return "World Server Full";
	case 'M': return "Your password expired";
	case 'U': return "Keycode input Success!";
	default: return {};
	}
}

void Overlay_LogResMsg::render_message(int dlgX, int dlgY)
{
    std::string txt;

    switch (m_cMsgCode)
    {
    case '1':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG5);
        break;

    case '2':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG6);
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 90, UPDATE_SCREEN_ON_LOG_MSG7);
        break;

    case '3':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG8);
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 85, UPDATE_SCREEN_ON_LOG_MSG9);
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 100, UPDATE_SCREEN_ON_LOG_MSG10);
        break;

    case '4':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG11);
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 85, UPDATE_SCREEN_ON_LOG_MSG12);
        break;

    case '5':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG13);
        break;

    case '6':
        put_aligned_string(dlgX + 16, dlgX + 291, dlgY + 60, "Already existing account name.", GameColors::UIDarkRed);
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 80, UPDATE_SCREEN_ON_LOG_MSG14);
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 95, UPDATE_SCREEN_ON_LOG_MSG15);
        break;

    case '7':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG16);
        break;

    case '8':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG17);
        break;

    case '9':
        put_aligned_string(dlgX + 16, dlgX + 291, dlgY + 60, "Already existing character name.", GameColors::UIDarkRed);
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 80, UPDATE_SCREEN_ON_LOG_MSG18);
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 95, UPDATE_SCREEN_ON_LOG_MSG19);
        break;

    case 'A':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG20);
        break;

    case 'B':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG21);
        break;

    case 'C':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG22);
        break;

    case 'D':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG23);
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 85, UPDATE_SCREEN_ON_LOG_MSG24);
        break;

    case 'E':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG25);
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 85, UPDATE_SCREEN_ON_LOG_MSG26);
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 100, UPDATE_SCREEN_ON_LOG_MSG27);
        break;

    case 'F':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG28);
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 85, UPDATE_SCREEN_ON_LOG_MSG29);
        break;

    case 'G':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG30);
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 85, UPDATE_SCREEN_ON_LOG_MSG31);
        break;

    case 'H':
        if (m_game->m_block_year == 0)
        {
            put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG32);
            put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 85, UPDATE_SCREEN_ON_LOG_MSG33);
        }
        else
        {
            put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG34);
            txt = std::format(UPDATE_SCREEN_ON_LOG_MSG35, m_game->m_block_year, m_game->m_block_month, m_game->m_block_day);
            put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 85, txt.c_str());
        }
        break;

    case 'I':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 85, "Not enough points to play.");
        break;

    case 'J':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 85, "Please ! Try Other World Server");
        break;

    case 'M':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 85, "Please! Change password");
        break;

    case 'U':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 85, "Keycode Registration successed.");
        break;

    case 'X':
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG38);
        put_aligned_string(dlgX + 36, dlgX + 291, dlgY + 85, UPDATE_SCREEN_ON_LOG_MSG39);
        break;

    case 'Y':
        put_aligned_string(dlgX + 16, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG40);
        put_aligned_string(dlgX + 16, dlgX + 291, dlgY + 85, UPDATE_SCREEN_ON_LOG_MSG41);
        break;

    case 'Z':
        put_aligned_string(dlgX + 16, dlgX + 291, dlgY + 70, UPDATE_SCREEN_ON_LOG_MSG42);
        put_aligned_string(dlgX + 16, dlgX + 291, dlgY + 85, UPDATE_SCREEN_ON_LOG_MSG41);
        break;
    }
}

void Overlay_LogResMsg::on_render()
{
    int dlgX, dlgY;
    draw_centered_notice_frame(dlgX, dlgY, message_title(m_cMsgCode), 46);

    // render the appropriate message
    render_message(dlgX, dlgY);

    // OK button
    m_controls.render();
}
