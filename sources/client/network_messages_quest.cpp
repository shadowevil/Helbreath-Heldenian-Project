#include "game.h"
#include "screen_on_game.h"

namespace NetworkMessageHandlers {

void handle_quest_counter(CGame* game, char* data)
{
	auto* screen = game->get_active_screen_as<Screen_OnGame>();
	if (screen) screen->get_quest_manager().handle_quest_counter(data);
}

void handle_quest_contents(CGame* game, char* data)
{
	auto* screen = game->get_active_screen_as<Screen_OnGame>();
	if (screen) screen->get_quest_manager().handle_quest_contents(data);
}

void handle_quest_reward(CGame* game, char* data)
{
	auto* screen = game->get_active_screen_as<Screen_OnGame>();
	if (screen) screen->get_quest_manager().handle_quest_reward(data);
}

void handle_quest_completed(CGame* game, char* data)
{
	auto* screen = game->get_active_screen_as<Screen_OnGame>();
	if (screen) screen->get_quest_manager().handle_quest_completed(data);
}

void handle_quest_aborted(CGame* game, char* data)
{
	auto* screen = game->get_active_screen_as<Screen_OnGame>();
	if (screen) screen->get_quest_manager().handle_quest_aborted(data);
}

} // namespace NetworkMessageHandlers
