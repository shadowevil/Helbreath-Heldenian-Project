#pragma once

#include <string>
#include <vector>
#include <memory>

// Forward declaration
class CGame;

// Base class for all chat commands
class ChatCommand
{
public:
	virtual ~ChatCommand() = default;

	// get the command name (without leading slash)
	virtual const char* get_name() const = 0;

	// execute the command
	// game: pointer to game instance
	// args: arguments after the command (may be nullptr or empty)
	// Returns: true if command was handled
	virtual bool execute(CGame* game, const char* args) = 0;
};

class ChatCommandManager
{
public:
	static ChatCommandManager& get();

	// initialize with game pointer and register built-in commands
	void initialize(CGame* game);

	// register_hotkey a command (takes ownership)
	void register_command(std::unique_ptr<ChatCommand> command);

	// Process a chat message, returns true if it was a command
	bool process_command(const char* message);

private:
	ChatCommandManager() = default;
	~ChatCommandManager() = default;
	ChatCommandManager(const ChatCommandManager&) = delete;
	ChatCommandManager& operator=(const ChatCommandManager&) = delete;

	// register_hotkey all built-in commands
	void register_built_in_commands();

	// Pointer to game instance
	CGame* m_game = nullptr;

	// Registered commands
	std::vector<std::unique_ptr<ChatCommand>> m_commands;

	bool m_initialized = false;
};
