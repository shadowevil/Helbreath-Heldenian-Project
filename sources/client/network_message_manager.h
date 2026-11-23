#pragma once
#include <cstdint>

class CGame;

// NetworkMessageManager: Lightweight orchestrator for network message handling
class NetworkMessageManager
{
public:
	explicit NetworkMessageManager(CGame* game);
	~NetworkMessageManager() = default;

	// Main entry point: processes incoming network messages
	bool process_message(uint32_t msg_id, char* data, uint32_t msg_size);

private:
	CGame* m_game;
};
