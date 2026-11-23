#pragma once

#include <chrono>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

// Helpers for refusal log lines, the log lines an unauthenticated peer can force.
namespace hb::server
{
	// Caps one kind of refusal warning (as in Helbreath-SDL): this many per interval across all peers, the
	// rest counted and reported once by the first refusal of the next interval. Give each kind of line its own
	// instance. It caps only the log line; the caller still answers every refusal.
	class refusal_log_limiter
	{
	public:
		static constexpr std::uint32_t max_per_interval = 5;
		static constexpr std::chrono::seconds interval{60};

		struct verdict
		{
			bool log = false;             // write this refusal's own line
			std::uint32_t suppressed = 0; // refusals dropped since the last report
		};

		[[nodiscard]] verdict note(std::chrono::steady_clock::time_point now)
		{
			verdict result;
			if (!m_started || now - m_interval_start >= interval)
			{
				m_started = true;
				m_interval_start = now;
				m_logged = 0;
				result.suppressed = m_suppressed;
				m_suppressed = 0;
			}
			if (m_logged < max_per_interval)
			{
				++m_logged;
				result.log = true;
			}
			else
			{
				++m_suppressed;
			}
			return result;
		}

	private:
		bool m_started = false;
		std::chrono::steady_clock::time_point m_interval_start{};
		std::uint32_t m_logged = 0;
		std::uint32_t m_suppressed = 0;
	};

	// `text` for a log line, with every byte outside printable ASCII written as \xNN (and a backslash as \\),
	// so bytes a peer sent cannot forge a log line or a terminal escape.
	[[nodiscard]] inline std::string escape_log_text(std::string_view text)
	{
		constexpr unsigned char first_printable = 0x20;
		constexpr unsigned char last_printable = 0x7e;

		std::string escaped;
		escaped.reserve(text.size());
		for (const char letter : text)
		{
			const auto byte = static_cast<unsigned char>(letter);
			if (letter == '\\')
			{
				escaped += "\\\\";
			}
			else if (byte < first_printable || byte > last_printable)
			{
				escaped += std::format("\\x{:02X}", static_cast<unsigned>(byte));
			}
			else
			{
				escaped += letter;
			}
		}
		return escaped;
	}

} // namespace hb::server
