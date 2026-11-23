#pragma once

#include <string>
#include <vector>
#include <memory>

class CGame;

class ServerCommand
{
public:
	virtual ~ServerCommand() = default;
	virtual const char* get_name() const = 0;
	virtual const char* GetDescription() const = 0;
	virtual const char* GetHelp() const { return GetDescription(); }
	virtual void execute(CGame* game, const char* args) = 0;
};

class ServerCommandManager
{
public:
	static ServerCommandManager& get();

	void initialize(CGame* game);
	void register_command(std::unique_ptr<ServerCommand> command);
	bool process_command(const char* input);

private:
	ServerCommandManager() = default;
	~ServerCommandManager() = default;
	ServerCommandManager(const ServerCommandManager&) = delete;
	ServerCommandManager& operator=(const ServerCommandManager&) = delete;

	void register_built_in_commands();

	CGame* m_game = nullptr;
	std::vector<std::unique_ptr<ServerCommand>> m_commands;
	bool m_initialized = false;
};
