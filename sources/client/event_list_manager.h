#pragma once

#include <cstdint>
#include <string>
#include <cstring>
#include <cstdio>

class CGame;

namespace hb::shared::render { class IRenderer; }

struct EventEntry
{
	uint32_t time = 0;
	char color = 0;
	std::string txt;
};

class event_list_manager
{
public:
	static event_list_manager& get();
	void set_game(CGame* game);

	void add_event(const char* txt, char color = 0, bool dup_allow = true);
	void add_event_top(const char* txt, char color);
	void show_events(uint32_t time);

private:
	event_list_manager() = default;
	~event_list_manager() = default;

	CGame* m_game = nullptr;
	EventEntry m_events[6]{};
	EventEntry m_events2[6]{};
};
