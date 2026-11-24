#include "agent_journal.h"

#ifdef DEF_CLAUDE_TESTING

#include "client_log_observer.h"
#include "log_level.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <utility>

namespace hb::client
{

	//-----------------------------------------------------------------------------
	// journal_fields
	//-----------------------------------------------------------------------------

	void journal_fields::append_json_string(std::string& out, std::string_view text)
	{
		out.push_back('"');
		for (const char raw : text)
		{
			const auto named = std::ranges::find(named_escapes, raw, &std::pair<char, std::string_view>::first);
			if (named != named_escapes.end())
			{
				out.append(named->second);
				continue;
			}
			const auto byte = static_cast<std::uint8_t>(raw);
			if (byte < first_printable_byte)
			{
				out.append(std::format("\\u{:04x}", static_cast<std::uint32_t>(byte)));
				continue;
			}
			if (byte >= first_non_ascii_byte)
			{
				out.push_back(non_ascii_substitute);
				continue;
			}
			out.push_back(raw);
		}
		out.push_back('"');
	}

	void journal_fields::begin_key(std::string_view key)
	{
		if (!m_text.empty())
		{
			m_text.push_back(',');
		}
		append_json_string(m_text, key);
		m_text.push_back(':');
	}

	journal_fields& journal_fields::add(std::string_view key, std::string_view value)
	{
		begin_key(key);
		append_json_string(m_text, value);
		return *this;
	}

	journal_fields& journal_fields::add(std::string_view key, const char* value)
	{
		return add(key, value != nullptr ? std::string_view{value} : std::string_view{});
	}

	journal_fields& journal_fields::add(std::string_view key, bool value)
	{
		begin_key(key);
		m_text.append(value ? "true" : "false");
		return *this;
	}

	journal_fields& journal_fields::add_integer(std::string_view key, std::int64_t value)
	{
		begin_key(key);
		m_text.append(std::format("{}", value));
		return *this;
	}

	journal_fields& journal_fields::add_real(std::string_view key, double value)
	{
		begin_key(key);
		// a non-finite double has no JSON spelling; recording 0 keeps the reply parseable
		m_text.append(std::isfinite(value) ? std::format("{}", value) : "0");
		return *this;
	}

	std::string journal_fields::take()
	{
		std::string finished;
		finished.reserve(m_text.size() + 2);
		finished.push_back('{');
		finished.append(m_text);
		finished.push_back('}');
		m_text.clear();
		return finished;
	}

	//-----------------------------------------------------------------------------
	// journal_data_filter
	//-----------------------------------------------------------------------------

	void journal_data_filter::set_object_id(std::int64_t object_id)
	{
		m_object_id_needles.clear();
		for (const std::string_view key : journal_object_id_keys)
		{
			m_object_id_needles.push_back(std::format("\"{}\":{}", key, object_id));
		}
	}

	void journal_data_filter::set_message_contains(std::string needle)
	{
		m_message_contains = std::move(needle);
	}

	bool journal_data_filter::admits(std::string_view data) const
	{
		if (!m_object_id_needles.empty())
		{
			// journal_fields writes `"key":value` with no spaces, so the needle is exact text
			const auto names_object = [data](const std::string& needle)
			{
				for (std::size_t at = data.find(needle); at != std::string_view::npos; at = data.find(needle, at + 1))
				{
					const std::size_t past = at + needle.size();
					if (past < data.size() && (data[past] == ',' || data[past] == '}'))
					{
						return true;
					}
				}
				return false;
			};
			if (std::ranges::none_of(m_object_id_needles, names_object))
			{
				return false;
			}
		}
		if (m_message_contains.empty())
		{
			return true;
		}
		return std::ranges::any_of(journal_message_openings, [this, data](std::string_view opening)
		                           { return string_field_contains(data, opening); });
	}

	bool journal_data_filter::string_field_contains(std::string_view data, std::string_view opening) const
	{
		const std::size_t start = data.find(opening);
		if (start == std::string_view::npos)
		{
			return false;
		}
		// the string value runs to the first UNESCAPED quote
		const std::size_t value_start = start + opening.size();
		std::size_t value_end = value_start;
		bool escaped = false;
		while (value_end < data.size() && (escaped || data[value_end] != '"'))
		{
			escaped = !escaped && data[value_end] == '\\';
			++value_end;
		}
		return data.substr(value_start, value_end - value_start).find(m_message_contains) != std::string_view::npos;
	}

	//-----------------------------------------------------------------------------
	// agent_journal
	//-----------------------------------------------------------------------------

	agent_journal& agent_journal::get()
	{
		static agent_journal instance;
		return instance;
	}

	agent_journal::agent_journal()
		: m_ring(journal_default_capacity), m_started_at(std::chrono::steady_clock::now())
	{
		// the logger's single observer slot; a process-lifetime singleton never deregisters, as set_observer asks
		hb::logger::set_observer(&agent_journal::on_log_line, this);
	}

	std::string_view agent_journal::level_name(int entry_level)
	{
		switch (entry_level)
		{
		case hb::logger::level::debug:
			return "debug";
		case hb::logger::level::log:
			return "info";
		case hb::logger::level::warn:
			return "warning";
		case hb::logger::level::error:
			return "error";
		default:
			return "unknown";
		}
	}

	void agent_journal::on_log_line(void* context, int entry_level, std::string_view message)
	{
		// warning and error only (debug stays in client.log), split by severity across the diagnostic channels
		journal_channel channel = journal_channel::client_warnings;
		if (entry_level == hb::logger::level::error)
		{
			channel = journal_channel::client_errors;
		}
		else if (entry_level != hb::logger::level::warn)
		{
			return;
		}
		auto* journal = static_cast<agent_journal*>(context);
		if (journal == nullptr || !journal->enabled(channel))
		{
			return;
		}
		// on the logging thread with the logger's lock released: logging here recurses, throwing unwinds it
		try
		{
			journal_fields fields;
			fields.add("level", level_name(entry_level))
				.add("message", message.substr(0, journal_max_log_message))
				.add("component", log_component_name);
			journal->append(channel, "log_line", fields.take());
		}
		catch (...)
		{
			// nowhere to say it: dropping one journal line beats unwinding out of a log call
		}
	}

	void agent_journal::begin_frame()
	{
		m_frame.fetch_add(1, std::memory_order_relaxed);
	}

	std::uint64_t agent_journal::elapsed_ms() const
	{
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - m_started_at);
		return static_cast<std::uint64_t>(elapsed.count());
	}

	std::uint64_t agent_journal::micros_at(std::chrono::steady_clock::time_point instant) const
	{
		if (instant <= m_started_at)
		{
			return 0;
		}
		return static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::microseconds>(instant - m_started_at).count());
	}

	void agent_journal::note_evicted(const journal_event& evicted)
	{
		if (evicted.seq >= m_read_cursor)
		{
			++m_dropped_unread;  // nobody had drained this one: it is a real loss and must be reported
		}
	}

	void agent_journal::append(journal_channel channel, std::string_view type, std::string data)
	{
		if (!enabled(channel))
		{
			return;  // re-checked under no lock: journal_record already tested it, direct callers may not have
		}
		const std::uint64_t elapsed = elapsed_ms();
		const auto frame_now = m_frame.load(std::memory_order_relaxed);
		try
		{
			const std::lock_guard<std::mutex> guard(m_mutex);
			const std::size_t capacity = m_ring.size();
			if (capacity == 0)
			{
				return;
			}
			if (m_size == capacity)
			{
				// drop-oldest on overflow
				note_evicted(m_ring[m_head]);
				m_head = (m_head + 1) % capacity;
				--m_size;
			}
			const std::size_t slot = (m_head + m_size) % capacity;
			journal_event& target = m_ring[slot];
			target.seq = m_next_seq++;
			target.t_ms = elapsed;
			target.frame = frame_now;
			target.channel = channel;
			target.type.assign(type);
			target.data = std::move(data);
			++m_size;
		}
		catch (...)
		{
			// an append is never worth taking the client down for, and the observer path must not throw
		}
	}

	agent_journal::drain_result agent_journal::drain(std::uint64_t since, std::size_t max_events,
	                                                 const journal_selector& selector,
	                                                 std::vector<journal_event>& out,
	                                                 journal_drain_accounting accounting)
	{
		drain_result result;
		const std::lock_guard<std::mutex> guard(m_mutex);
		const std::size_t capacity = m_ring.size();
		std::uint64_t last_returned = 0;
		for (std::size_t i = 0; i < m_size; ++i)
		{
			const journal_event& candidate = m_ring[(m_head + i) % capacity];
			if (candidate.seq < since)
			{
				continue;
			}
			if (!selector.matches(candidate))
			{
				continue;
			}
			if (out.size() >= max_events)
			{
				result.truncated = true;
				break;
			}
			out.push_back(candidate);
			last_returned = candidate.seq;
		}
		// a truncated drain claims only what it handed over; an untruncated one consumes past filtered events
		result.cursor = result.truncated ? last_returned + 1 : m_next_seq;
		// dropped is REPORTED either way, but only a consuming drain clears it and moves the read mark
		result.dropped = m_dropped_unread;
		if (accounting == journal_drain_accounting::consume)
		{
			m_dropped_unread = 0;
			m_read_cursor = result.cursor;
		}
		return result;
	}

	std::uint64_t agent_journal::peek(std::uint64_t since, std::size_t max_events,
	                                  std::vector<journal_event>& out) const
	{
		const std::lock_guard<std::mutex> guard(m_mutex);
		const std::size_t capacity = m_ring.size();
		const std::size_t room = out.size() + max_events;
		std::uint64_t last_copied = 0;
		for (std::size_t i = 0; i < m_size; ++i)
		{
			const journal_event& candidate = m_ring[(m_head + i) % capacity];
			if (candidate.seq < since)
			{
				continue;
			}
			if (out.size() >= room)
			{
				// cut short: resume from the event after the last one copied, next frame
				return last_copied != 0 ? last_copied + 1 : since;
			}
			out.push_back(candidate);
			last_copied = candidate.seq;
		}
		return m_next_seq;
	}

	agent_journal::query_result agent_journal::query(const journal_window& window, std::size_t max_events,
	                                                 const journal_selector& selector,
	                                                 std::vector<journal_event>& out) const
	{
		query_result result;
		result.now_ms = elapsed_ms();
		const std::lock_guard<std::mutex> guard(m_mutex);
		const std::size_t capacity = m_ring.size();
		result.newest_seq = m_next_seq - 1;
		result.oldest_seq_retained = m_size > 0 ? m_ring[m_head].seq : m_next_seq;
		result.oldest_t_ms_retained = m_size > 0 ? m_ring[m_head].t_ms : result.now_ms;
		// newest -> oldest, so the first max_events admitted ARE the newest ones
		for (std::size_t back = 0; back < m_size; ++back)
		{
			const journal_event& candidate = m_ring[(m_head + m_size - 1 - back) % capacity];
			if (window.after_seq && candidate.seq <= *window.after_seq)
			{
				break;  // seq only falls from here, so nothing further can be admitted
			}
			// t_ms is only NEARLY monotonic (read before the lock), so time bounds skip rather than stop
			if (!window.admits(candidate) || !selector.matches(candidate))
			{
				continue;
			}
			if (out.size() >= max_events)
			{
				result.more_older = true;
				result.older_before_seq = out.back().seq;
				break;
			}
			out.push_back(candidate);
		}
		return result;
	}

	std::uint64_t agent_journal::next_seq() const
	{
		const std::lock_guard<std::mutex> guard(m_mutex);
		return m_next_seq;
	}

	void agent_journal::set_enabled(journal_channel channel, bool on)
	{
		const std::uint32_t bit = channel_bit(channel);
		if (on)
		{
			m_enabled_mask.fetch_or(bit, std::memory_order_relaxed);
		}
		else
		{
			m_enabled_mask.fetch_and(~bit, std::memory_order_relaxed);
		}
	}

	std::size_t agent_journal::set_capacity(std::size_t capacity)
	{
		const std::size_t wanted = std::clamp(capacity, journal_min_capacity, journal_max_capacity);
		const std::lock_guard<std::mutex> guard(m_mutex);
		if (wanted == m_ring.size())
		{
			return wanted;
		}
		// keep the NEWEST events that still fit; anything a shrink discards is an eviction like any other
		const std::size_t keep = std::min(m_size, wanted);
		const std::size_t discard = m_size - keep;
		for (std::size_t i = 0; i < discard; ++i)
		{
			note_evicted(m_ring[(m_head + i) % m_ring.size()]);
		}
		std::vector<journal_event> resized(wanted);
		for (std::size_t i = 0; i < keep; ++i)
		{
			resized[i] = std::move(m_ring[(m_head + discard + i) % m_ring.size()]);
		}
		m_ring = std::move(resized);
		m_head = 0;
		m_size = keep;
		return wanted;
	}

	std::size_t agent_journal::capacity() const
	{
		const std::lock_guard<std::mutex> guard(m_mutex);
		return m_ring.size();
	}

} // namespace hb::client

#endif // DEF_CLAUDE_TESTING
