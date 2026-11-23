#include "EventListManager.h"
#include "Game.h"
#include "GameFonts.h"
#include "TextLibExt.h"
#include "GameTimer.h"
#include "lan_eng.h"
#include "Screen_OnGame.h"

event_list_manager& event_list_manager::get()
{
	static event_list_manager instance;
	return instance;
}

void event_list_manager::set_game(CGame* game)
{
	m_game = game;
}

void event_list_manager::add_event(const char* txt, char color, bool dup_allow)
{
	int i;
	if ((dup_allow == false) && (m_events[5].txt == txt)) return;
	if (color == 10)
	{
		for (i = 1; i < 6; i++)
		{
			m_events2[i - 1].txt = m_events2[i].txt;
			m_events2[i - 1].color = m_events2[i].color;
			m_events2[i - 1].time = m_events2[i].time;
		}
		m_events2[5].txt = txt;
		m_events2[5].color = color;
		m_events2[5].time = GameClock::get_time_ms();
	}
	else
	{
		for (i = 1; i < 6; i++)
		{
			m_events[i - 1].txt = m_events[i].txt;
			m_events[i - 1].color = m_events[i].color;
			m_events[i - 1].time = m_events[i].time;
		}
		m_events[5].txt = txt;
		m_events[5].color = color;
		m_events[5].time = GameClock::get_time_ms();
	}
}

void event_list_manager::add_event_top(const char* txt, char color)
{
	for (int i = 1; i < 6; i++)
	{
		m_events[i - 1].txt = m_events[i].txt;
		m_events[i - 1].color = m_events[i].color;
		m_events[i - 1].time = m_events[i].time;
	}
	m_events[5].txt = txt;
	m_events[5].color = color;
	m_events[5].time = GameClock::get_time_ms();
}

void event_list_manager::show_events(uint32_t time)
{
	int i;
	int baseY = EVENTLIST2_BASE_Y();
	m_game->m_Renderer->begin_text_batch();
	// uint32_t subtraction wraps safely — expired events (time far in the past) produce
	// large differences that exceed 5000, so the comparison remains correct across wrap
	for (i = 0; i < 6; i++)
		if ((time - m_events[i].time) < 5000)
		{
			switch (m_events[i].color) {
			case 0:
				hb::shared::text::draw_text(GameFont::Default, 10, 10 + i * 15, m_events[i].txt.c_str(), hb::shared::text::TextStyle::with_shadow(GameColors::UINearWhite));
				break;
			case 1:
				hb::shared::text::draw_text(GameFont::Default, 10, 10 + i * 15, m_events[i].txt.c_str(), hb::shared::text::TextStyle::with_shadow(GameColors::ChatEventGreen));
				break;
			case 2:
				hb::shared::text::draw_text(GameFont::Default, 10, 10 + i * 15, m_events[i].txt.c_str(), hb::shared::text::TextStyle::with_shadow(GameColors::UIWorldChat));
				break;
			case 3:
				hb::shared::text::draw_text(GameFont::Default, 10, 10 + i * 15, m_events[i].txt.c_str(), hb::shared::text::TextStyle::with_shadow(GameColors::UIFactionChat));
				break;
			case 4:
				hb::shared::text::draw_text(GameFont::Default, 10, 10 + i * 15, m_events[i].txt.c_str(), hb::shared::text::TextStyle::with_shadow(GameColors::UIPartyChat));
				break;
			case hb::shared::owner::Slime:
				hb::shared::text::draw_text(GameFont::Default, 10, 10 + i * 15, m_events[i].txt.c_str(), hb::shared::text::TextStyle::with_shadow(GameColors::UIGameMasterChat));
				break;
			case hb::shared::owner::Howard:
				hb::shared::text::draw_text(GameFont::Default, 10, 10 + i * 15, m_events[i].txt.c_str(), hb::shared::text::TextStyle::with_shadow(GameColors::UINormalChat));
				break;
			}
		}

	for (i = 0; i < 6; i++)
		if ((time - m_events2[i].time) < 5000)
		{
			switch (m_events2[i].color) {
			case 0:
				hb::shared::text::draw_text(GameFont::Default, 10, baseY + i * 15, m_events2[i].txt.c_str(), hb::shared::text::TextStyle::with_shadow(GameColors::UINearWhite));
				break;
			case 1:
				hb::shared::text::draw_text(GameFont::Default, 10, baseY + i * 15, m_events2[i].txt.c_str(), hb::shared::text::TextStyle::with_shadow(GameColors::ChatEventGreen));
				break;
			case 2:
				hb::shared::text::draw_text(GameFont::Default, 10, baseY + i * 15, m_events2[i].txt.c_str(), hb::shared::text::TextStyle::with_shadow(GameColors::UIWorldChat));
				break;
			case 3:
				hb::shared::text::draw_text(GameFont::Default, 10, baseY + i * 15, m_events2[i].txt.c_str(), hb::shared::text::TextStyle::with_shadow(GameColors::UIFactionChat));
				break;
			case 4:
				hb::shared::text::draw_text(GameFont::Default, 10, baseY + i * 15, m_events2[i].txt.c_str(), hb::shared::text::TextStyle::with_shadow(GameColors::UIPartyChat));
				break;
			case hb::shared::owner::Slime:
				hb::shared::text::draw_text(GameFont::Default, 10, baseY + i * 15, m_events2[i].txt.c_str(), hb::shared::text::TextStyle::with_shadow(GameColors::UIGameMasterChat));
				break;
			case hb::shared::owner::Howard:
				hb::shared::text::draw_text(GameFont::Default, 10, baseY + i * 15, m_events2[i].txt.c_str(), hb::shared::text::TextStyle::with_shadow(GameColors::UINormalChat));
				break;
			}
		}
	if (m_game->on_game()->m_skill_using_status == true)
	{
		int text_w = hb::shared::text::GetTextRenderer()->measure_text(SHOW_EVENT_LIST1).width;
		int text_x = (LOGICAL_WIDTH() - text_w) / 2;
		int text_y = LOGICAL_HEIGHT() / 2 + 45;
		hb::shared::text::draw_text(GameFont::Default, text_x, text_y, SHOW_EVENT_LIST1, hb::shared::text::TextStyle::with_shadow(GameColors::UINearWhite));
	}
	m_game->m_Renderer->end_text_batch();
}
