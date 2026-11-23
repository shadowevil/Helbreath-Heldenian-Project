#pragma once

#include <cstdint>
#include <array>
#include <memory>
#include "game_constants.h"
#include "chat_msg.h"

class ChatManager
{
public:
	static ChatManager& get();

	// Lifecycle
	void initialize();
	void shutdown();

	// Message buffer management
	void add_message(char* msg, char type);
	void clear_messages();
	CMsg* get_message(int index) const;

	// Whisper target management
	void add_whisper_target(const char* name);
	void clear_whispers();
	const char* get_whisper_target_name(int index) const;
	bool has_whisper_target(int index) const;
	int get_whisper_index() const { return m_whisper_index; }
	void set_whisper_index(int index) { m_whisper_index = static_cast<char>(index); }
	void cycle_whisper_up();
	void cycle_whisper_down();

	// toggle accessors
	bool is_whisper_enabled() const { return m_whisper_enabled; }
	void set_whisper_enabled(bool enabled) { m_whisper_enabled = enabled; }
	bool is_shout_enabled() const { return m_shout_enabled; }
	void set_shout_enabled(bool enabled) { m_shout_enabled = enabled; }

private:
	ChatManager() = default;
	~ChatManager() = default;
	ChatManager(const ChatManager&) = delete;
	ChatManager& operator=(const ChatManager&) = delete;

	// Message scroll list (index 0 = most recent)
	std::array<std::unique_ptr<CMsg>, game_limits::max_chat_scroll_msgs> m_messages;

	// Whisper target names (index 0 = most recent)
	std::array<std::unique_ptr<CMsg>, game_limits::max_whisper_msgs> m_whisper_targets;
	char m_whisper_index = 0;

	// toggle state
	bool m_whisper_enabled = true;
	bool m_shout_enabled = true;
};
