// network_messages_fish.cpp: Fish network message handlers.
// Handlers now live in fishing_manager; this file provides the
// NetworkMessageHandlers namespace wrappers for backward compatibility.

#include "game.h"
#include "screen_on_game.h"

namespace NetworkMessageHandlers {

void handle_fish_chance(CGame* game, char* data)
{
	auto* screen = game->get_active_screen_as<Screen_OnGame>();
	if (screen) screen->get_fishing_manager().handle_fish_chance(data);
}

void handle_event_fish_mode(CGame* game, char* data)
{
	auto* screen = game->get_active_screen_as<Screen_OnGame>();
	if (screen) screen->get_fishing_manager().handle_event_fish_mode(data);
}

void handle_fish_canceled(CGame* game, char* data)
{
	auto* screen = game->get_active_screen_as<Screen_OnGame>();
	if (screen) screen->get_fishing_manager().handle_fish_canceled(data);
}

void handle_fish_success(CGame* game, char* data)
{
	auto* screen = game->get_active_screen_as<Screen_OnGame>();
	if (screen) screen->get_fishing_manager().handle_fish_success(data);
}

void handle_fish_fail(CGame* game, char* data)
{
	auto* screen = game->get_active_screen_as<Screen_OnGame>();
	if (screen) screen->get_fishing_manager().handle_fish_fail(data);
}

} // namespace NetworkMessageHandlers
