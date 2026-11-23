#include "chat_manager.h"
#include <cstring>

ChatManager& ChatManager::get()
{
	static ChatManager instance;
	return instance;
}

void ChatManager::initialize()
{
	m_whisper_index = game_limits::max_whisper_msgs;
	m_whisper_enabled = true;
	m_shout_enabled = true;
	clear_messages();
	clear_whispers();
}

void ChatManager::shutdown()
{
	clear_messages();
	clear_whispers();
}

void ChatManager::add_message(char* msg, char type)
{
	if (m_messages[game_limits::max_chat_scroll_msgs - 1] != nullptr)
	{
		m_messages[game_limits::max_chat_scroll_msgs - 1].reset();
	}
	for (int i = game_limits::max_chat_scroll_msgs - 2; i >= 0; i--)
	{
		m_messages[i + 1] = std::move(m_messages[i]);
	}
	m_messages[0] = std::make_unique<CMsg>(1, msg, type);
}

void ChatManager::clear_messages()
{
	for (auto& msg : m_messages) msg.reset();
}

CMsg* ChatManager::get_message(int index) const
{
	if (index < 0 || index >= game_limits::max_chat_scroll_msgs) return nullptr;
	return m_messages[index].get();
}

void ChatManager::add_whisper_target(const char* name)
{
	if (m_whisper_targets[game_limits::max_whisper_msgs - 1] != nullptr)
	{
		m_whisper_targets[game_limits::max_whisper_msgs - 1].reset();
	}
	for (int i = game_limits::max_whisper_msgs - 2; i >= 0; i--)
	{
		m_whisper_targets[i + 1] = std::move(m_whisper_targets[i]);
	}
	m_whisper_targets[0] = std::make_unique<CMsg>(0, name, 0);
	m_whisper_index = 0;
}

void ChatManager::clear_whispers()
{
	for (auto& msg : m_whisper_targets) msg.reset();
}

const char* ChatManager::get_whisper_target_name(int index) const
{
	if (index < 0 || index >= game_limits::max_whisper_msgs) return nullptr;
	if (!m_whisper_targets[index]) return nullptr;
	return m_whisper_targets[index]->m_pMsg;
}

bool ChatManager::has_whisper_target(int index) const
{
	if (index < 0 || index >= game_limits::max_whisper_msgs) return false;
	return m_whisper_targets[index] != nullptr;
}

void ChatManager::cycle_whisper_up()
{
	int max_index = 0;
	for (int i = game_limits::max_whisper_msgs - 1; i >= 0; i--)
	{
		if (m_whisper_targets[i] != nullptr)
		{
			max_index = i;
			break;
		}
	}
	m_whisper_index++;
	if (m_whisper_index > max_index) m_whisper_index = 0;
}

void ChatManager::cycle_whisper_down()
{
	int max_index = 0;
	for (int i = game_limits::max_whisper_msgs - 1; i >= 0; i--)
	{
		if (m_whisper_targets[i] != nullptr)
		{
			max_index = i;
			break;
		}
	}
	m_whisper_index--;
	if (m_whisper_index < 0) m_whisper_index = max_index;
}
