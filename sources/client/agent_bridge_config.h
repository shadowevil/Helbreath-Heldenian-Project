#pragma once

// Agent bridge launch flags (HH-0035). Everything below compiles in debug builds only, so a Release
// build parses no flag and carries none of these strings (claude_testing.h, d-027).
#include "claude_testing.h"

#ifdef DEF_CLAUDE_TESTING

#include "log.h"
#include <charconv>
#include <cstdint>
#include <string_view>

namespace hb::client
{
	// d-028: the bridge range is 8891-8898
	constexpr std::uint16_t agent_bridge_min_port = 8891;
	constexpr std::uint16_t agent_bridge_max_port = 8898;
	constexpr std::uint16_t agent_bridge_default_port = agent_bridge_min_port;

	struct agent_bridge_config
	{
		bool enabled = true;
		std::uint16_t port = agent_bridge_default_port;
	};

	// Reads --agent-bridge-port=N and --no-agent-bridge from a whitespace-separated command line and
	// ignores every other token; one pair of surrounding quotes is stripped first (lpCmdLine keeps them).
	// A malformed or out-of-range port warns and keeps the default; call it after hb::logger::initialize
	// so the warning is written.
	inline agent_bridge_config parse_agent_bridge_flags(std::string_view cmd_line)
	{
		constexpr std::string_view whitespace = " \t\r\n";
		constexpr std::string_view port_flag = "--agent-bridge-port";
		constexpr std::string_view port_prefix = "--agent-bridge-port=";

		agent_bridge_config config;
		while (true)
		{
			const auto start = cmd_line.find_first_not_of(whitespace);
			if (start == std::string_view::npos)
			{
				break;
			}
			cmd_line.remove_prefix(start);
			std::string_view token = cmd_line.substr(0, cmd_line.find_first_of(whitespace));
			cmd_line.remove_prefix(token.size());
			if (token.size() >= 2 && token.front() == '"' && token.back() == '"')
			{
				token = token.substr(1, token.size() - 2);
			}

			if (token == "--no-agent-bridge")
			{
				config.enabled = false;
			}
			else if (token.starts_with(port_flag))
			{
				// the space form, a missing value and a bad number all land in the warning
				const std::string_view value = token.starts_with(port_prefix) ? token.substr(port_prefix.size()) : std::string_view{};
				std::uint16_t parsed = 0;
				const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
				if (error == std::errc{} && end == value.data() + value.size()
					&& parsed >= agent_bridge_min_port && parsed <= agent_bridge_max_port)
				{
					config.port = parsed;
				}
				else
				{
					hb::logger::warn("agent bridge: invalid '{}', expected --agent-bridge-port=N with N in {}-{}; using {}",
						token, agent_bridge_min_port, agent_bridge_max_port, config.port);
				}
			}
		}
		return config;
	}
}

#endif
