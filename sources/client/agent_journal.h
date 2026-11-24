#pragma once

#include "claude_testing.h"

#ifdef DEF_CLAUDE_TESTING

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hb::client
{

	// The client's EVENT JOURNAL (HH-0041, ported from SDL): a ring the bridge only READS. Appends come from any
	// thread (the log observer), so it is mutex-guarded and never calls hb::logger. Emit sites observe only.
	// Never record a masked field's text, a login packet's body or any packet payload (ids and sizes only).

	constexpr char journal_channel_separator = '.';   // the dotted-segment separator
	constexpr std::string_view journal_channel_all = "*";  // the selector that matches every channel

	// Ring capacity: 4096 is the contract's starting figure; the bounds keep journal_config sane.
	constexpr std::size_t journal_default_capacity = 4096;
	constexpr std::size_t journal_min_capacity = 16;
	constexpr std::size_t journal_max_capacity = 65536;

	// RAW events one drain returns by default (a drain is one reply LINE; callers page with the cursor).
	// 1000, not 512, since replies coalesce runs of identical events; cursor arithmetic is unchanged.
	constexpr std::size_t journal_default_drain_max = 1000;

	// How many RAW events one QUERY returns when the caller names no limit. The live view is a screenful
	// of the newest events, not a transcript; older history is paged with more_older / older_before_seq.
	constexpr std::size_t journal_default_query_max = 200;

	// The closed channel vocabulary, matched by whole dotted segment ("state" selects all three). The last
	// four are DIAGNOSTIC, by origin and severity, so "everything that went wrong" is one subscription.
	enum class journal_channel : std::uint8_t
	{
		state_player,     // player_moved, player_stat_changed, stance_changed
		state_map,        // map_changed
		state_entity,     // entity_appeared, entity_left
		state_entity_motion, // entity_moved, entity_stopped: a monster field floods them, so they switch off alone
		                  // (state.entity.motion; "state.entity" still selects both)
		audio_sound,      // sound_play (successful triggers only; a refusal is a client warning), sound_stop, field_loop,
		                  // impact_folded (a cast's impact sound landing with an identical one, sounded once),
		                  // music_start / music_stop (the track id and a reason)
		visual_damage,    // damage_number, evade_word
		visual_effect,    // effect_start, spell_start, spell_shot, cast_beat, tap_ring (a walk click's ground ring)
		render_bubble,    // bubble_changed: the see-through bubble around the local player (HB-0342)
		perf_frame,       // frame_spike: a frame past the 60 Hz budget with its section breakdown (HB-0349); turning it
		                  // off and on again opens a new frame-time measurement window
		net_packet,       // packet_in, packet_out - ordinary traffic is observational, not diagnostic
		chat_message,     // chat_line (a broadcast arrived, own lines included), chat_send (the input line sent)
		ui_screen,        // screen_changed, and the one-off readiness markers: screen_active (the fade-in ended),
		                  // first_frame_presented (first present after a change), world_ready (the spawn frame ended)
		ui_window,        // window_opened, window_closed (via close / dismissed / esc), click_consumed
		ui_button,        // button_activated
		client_warnings,  // sound_refused, log_line (warning), and future recoverable client faults
		client_errors,    // log_line (error), and future hard client faults
		net_warnings,     // DECLARED but silent: future warnings (a retry, an out-of-order packet)
		net_errors,       // connect_failed, connection_lost, and future connection-level failures
	};

	// Every channel in enum order - the iteration order of journal_config's reply.
	inline constexpr std::array journal_channels{
		journal_channel::state_player,    journal_channel::state_map,     journal_channel::state_entity,
		journal_channel::state_entity_motion,
		journal_channel::audio_sound,     journal_channel::visual_damage,  journal_channel::visual_effect,
		journal_channel::render_bubble,   journal_channel::perf_frame,
		journal_channel::net_packet,      journal_channel::chat_message,   journal_channel::ui_screen,
		journal_channel::ui_window,       journal_channel::ui_button,      journal_channel::client_warnings,
		journal_channel::client_errors,   journal_channel::net_warnings,   journal_channel::net_errors,
	};

	// The wire name of a channel. Not user-facing text (nothing here is): it is a protocol token.
	[[nodiscard]] constexpr std::string_view journal_channel_name(journal_channel channel)
	{
		switch (channel)
		{
		case journal_channel::state_player:
			return "state.player";
		case journal_channel::state_map:
			return "state.map";
		case journal_channel::state_entity:
			return "state.entity";
		case journal_channel::state_entity_motion:
			return "state.entity.motion";
		case journal_channel::audio_sound:
			return "audio.sound";
		case journal_channel::visual_damage:
			return "visual.damage";
		case journal_channel::visual_effect:
			return "visual.effect";
		case journal_channel::render_bubble:
			return "render.bubble";
		case journal_channel::perf_frame:
			return "perf.frame";
		case journal_channel::net_packet:
			return "net.packet";
		case journal_channel::chat_message:
			return "chat.message"; // lang-ok: a journal event name, not a lang id
		case journal_channel::ui_screen:
			return "ui.screen";
		case journal_channel::ui_window:
			return "ui.window";
		case journal_channel::ui_button:
			return "ui.button";
		case journal_channel::client_warnings:
			return "client.warnings";
		case journal_channel::client_errors:
			return "client.errors";
		case journal_channel::net_warnings:
			return "net.warnings";
		case journal_channel::net_errors:
			return "net.errors";
		default:
			return "unknown";
		}
	}

	// Does `selector` (a dotted PREFIX, "" or "*" for everything) select this channel? By whole segment,
	// never raw prefix: "state" selects state.player, "stat" selects nothing.
	[[nodiscard]] constexpr bool journal_channel_matches(journal_channel channel, std::string_view selector)
	{
		if (selector.empty() || selector == journal_channel_all)
		{
			return true;
		}
		const std::string_view name = journal_channel_name(channel);
		if (name.size() < selector.size() || name.substr(0, selector.size()) != selector)
		{
			return false;
		}
		return name.size() == selector.size() || name[selector.size()] == journal_channel_separator;
	}

	// Does a drain OWN the one-shot dropped counter and read mark, or only look? A side read (a wait_for
	// trail) that cleared them would hide real loss from the caller whose accounting it was.
	enum class journal_drain_accounting : std::uint8_t
	{
		consume,  // the caller pages on this cursor: take the dropped counter and move the read mark
		observe,  // a side read: report what is outstanding, clear nothing, move nothing
	};

	// One recorded event. `data` is a pre-serialized JSON object ("{...}") from journal_fields, so an emission
	// site pays no JSON-library cost; the bridge parses it back only when it must.
	struct journal_event
	{
		std::uint64_t seq = 0;    // monotonic from client start, never reused: the cursor basis
		std::uint64_t t_ms = 0;   // milliseconds since the journal came up (the real interval between events)
		std::uint64_t frame = 0;  // the frame counter, so an event correlates with a screenshot of that frame
		journal_channel channel = journal_channel::state_player;
		std::string type;
		std::string data;
	};

	// The data keys that name an entity: an `object_id` filter admits an event carrying the id under any of them.
	inline constexpr std::array<std::string_view, 5> journal_object_id_keys{"object_id", "attacker_id", "target_id",
	                                                                        "caster", "anchor"};
	// Where a `message_contains` filter reads: the `message` string (a packet's message name, a log line's text) or
	// the `text` string (a chat line's words); either containing the needle admits the event.
	inline constexpr std::array<std::string_view, 2> journal_message_openings{"\"message\":\"", "\"text\":\""};

	// Filters on an event's DATA text, read without parsing it: `object_id` as any journal_object_id_keys
	// value, `message_contains` as a substring of the `message` or `text` string. Empty = no restriction.
	class journal_data_filter
	{
	public:
		// Admits only events naming this object id.
		void set_object_id(std::int64_t object_id);
		void set_message_contains(std::string needle);

		[[nodiscard]] bool admits(std::string_view data) const;

	private:
		// `"key":<id>` per id key, built once; a hit must end at ',' or '}' so 5 never matches 57.
		std::vector<std::string> m_object_id_needles;
		// true when the first string value under `opening` contains the needle
		[[nodiscard]] bool string_field_contains(std::string_view data, std::string_view opening) const;
		std::string m_message_contains;
	};

	// What one drain selects: `channels` by dotted-segment prefix, `types` then `exclude_types` by EXACT type,
	// then the data filter. An empty list is no restriction. Filtered inside the drain, so `max` counts only
	// events returned.
	struct journal_selector
	{
		std::vector<std::string> channels;
		std::vector<std::string> types;
		std::vector<std::string> exclude_types;
		journal_data_filter data;

		[[nodiscard]] bool matches(const journal_event& candidate) const
		{
			if (!channels.empty())
			{
				bool wanted = false;
				for (const std::string& selector : channels)
				{
					if (journal_channel_matches(candidate.channel, selector))
					{
						wanted = true;
						break;
					}
				}
				if (!wanted)
				{
					return false;
				}
			}
			if (!types.empty() && std::ranges::find(types, candidate.type) == types.end())
			{
				return false;
			}
			if (std::ranges::find(exclude_types, candidate.type) != exclude_types.end())
			{
				return false;
			}
			return data.admits(candidate.data);
		}
	};

	// The slice ONE query reads; with no bound it is the LIVE view. Time bounds (t_ms) are inclusive, seq
	// bounds exclusive, so a noted newest_seq is the after_seq that returns exactly what followed it.
	struct journal_window
	{
		std::optional<std::uint64_t> from_ms;
		std::optional<std::uint64_t> to_ms;
		std::optional<std::uint64_t> after_seq;
		std::optional<std::uint64_t> before_seq;

		[[nodiscard]] bool is_live() const
		{
			return !from_ms && !to_ms && !after_seq && !before_seq;
		}

		[[nodiscard]] bool admits(const journal_event& candidate) const
		{
			if (after_seq && candidate.seq <= *after_seq)
			{
				return false;
			}
			if (before_seq && candidate.seq >= *before_seq)
			{
				return false;
			}
			if (from_ms && candidate.t_ms < *from_ms)
			{
				return false;
			}
			return !to_ms || candidate.t_ms <= *to_ms;
		}
	};

	// A minimal JSON-object builder for an event's `data`. Escapes what JSON requires, so a log line
	// carrying quotes or backslashes cannot corrupt a drain reply.
	class journal_fields
	{
	public:
		journal_fields& add(std::string_view key, std::string_view value);
		// explicit, because without it a string literal would bind to the bool overload (pointer-to-bool
		// is a standard conversion and beats the user-defined conversion to string_view)
		journal_fields& add(std::string_view key, const char* value);
		journal_fields& add(std::string_view key, bool value);

		// Integers and reals go through templates so an int argument cannot be ambiguous between the
		// int64 and bool overloads (both would be plain conversions).
		template <std::integral value_type>
			requires(!std::same_as<value_type, bool>)
		journal_fields& add(std::string_view key, value_type value)
		{
			return add_integer(key, static_cast<std::int64_t>(value));
		}

		template <std::floating_point value_type>
		journal_fields& add(std::string_view key, value_type value)
		{
			return add_real(key, static_cast<double>(value));
		}

		// The finished object text, moved out. Valid exactly once.
		[[nodiscard]] std::string take();

	private:
		// Bytes below this are control characters JSON must escape.
		static constexpr std::uint8_t first_printable_byte = 0x20;
		// Non-ASCII is substituted: one invalid UTF-8 byte (a path, an OS message) would break a whole reply.
		static constexpr std::uint8_t first_non_ascii_byte = 0x80;
		static constexpr char non_ascii_substitute = '?';
		// The characters JSON escapes by name, and their escape sequences.
		static constexpr auto named_escapes = std::to_array<std::pair<char, std::string_view>>({
			{'"', "\\\""},
			{'\\', "\\\\"},
			{'\n', "\\n"},
			{'\r', "\\r"},
			{'\t', "\\t"},
			{'\b', "\\b"},
			{'\f', "\\f"},
		});

		// Appends `text` as a JSON string literal (quotes included) with the required escapes.
		static void append_json_string(std::string& out, std::string_view text);

		journal_fields& add_integer(std::string_view key, std::int64_t value);
		journal_fields& add_real(std::string_view key, double value);
		void begin_key(std::string_view key);

		std::string m_text;  // accumulates `"key":value` pairs; take() wraps it in braces
	};

	class agent_journal
	{
	public:
		// What a drain reports alongside the events themselves.
		struct drain_result
		{
			std::uint64_t cursor = 0;   // pass as the next `since`
			std::uint64_t dropped = 0;  // events evicted UNREAD since the previous drain (never silent)
			bool truncated = false;     // the max was hit: more events are already waiting
		};

		// What a query reports alongside the events themselves. Nothing here is accounting: a query
		// owns no cursor, so it has nothing to move and nothing to clear.
		struct query_result
		{
			std::uint64_t now_ms = 0;                // the journal clock when the query ran
			std::uint64_t newest_seq = 0;            // the newest seq ever recorded (0 before the first event)
			std::uint64_t oldest_seq_retained = 0;   // the oldest seq still in the ring (next_seq when empty)
			std::uint64_t oldest_t_ms_retained = 0;  // its t_ms, so a window reaching past the ring is visible
			bool more_older = false;                 // matching events older than the last one returned exist
			std::uint64_t older_before_seq = 0;      // pass as before_seq to page older (set when more_older)
		};

		static agent_journal& get();

		agent_journal(const agent_journal&) = delete;
		agent_journal& operator=(const agent_journal&) = delete;

		// The whole cost an emission site pays when its channel is off: one relaxed atomic load and a
		// mask test. Call it before building any fields.
		[[nodiscard]] bool enabled(journal_channel channel) const
		{
			return (m_enabled_mask.load(std::memory_order_relaxed) & channel_bit(channel)) != 0;
		}

		// Records one event. Safe from ANY thread. Never logs, never throws.
		void append(journal_channel channel, std::string_view type, std::string data);

		// Advances the frame counter. Called once per frame from the application loop, before the frame's
		// work, so every event appended during the frame carries that frame's number.
		void begin_frame();

		[[nodiscard]] std::uint64_t frame() const
		{
			return m_frame.load(std::memory_order_relaxed);
		}

		// Copies retained events with seq >= `since` that `selector` admits into `out`, up to max_events. The
		// ring is NOT emptied, so a window re-drains identically until eviction.
		drain_result drain(std::uint64_t since, std::size_t max_events, const journal_selector& selector,
		                   std::vector<journal_event>& out, journal_drain_accounting accounting);

		// A drain that touches no accounting, for pending wait_fors. Returns the seq to pass next, so a peek
		// cut short by max_events resumes where it stopped (bounded work per frame).
		std::uint64_t peek(std::uint64_t since, std::size_t max_events, std::vector<journal_event>& out) const;

		// The event-log read: the NEWEST max_events events `window` and `selector` admit, newest first. Const:
		// a query never touches the drain's accounting.
		query_result query(const journal_window& window, std::size_t max_events, const journal_selector& selector,
		                   std::vector<journal_event>& out) const;

		// The seq the NEXT appended event will carry - a wait registers this as its scan start, so it
		// matches only events that arrive after it was registered.
		[[nodiscard]] std::uint64_t next_seq() const;

		// Switches a channel on or off. Used by journal_config so a noisy channel cannot flood the ring
		// during an unrelated test.
		void set_enabled(journal_channel channel, bool on);

		// Resizes the ring (clamped to the bounds), keeping the newest events; discarded unread ones count as
		// dropped. Returns the effective capacity.
		std::size_t set_capacity(std::size_t capacity);

		[[nodiscard]] std::size_t capacity() const;

		// Milliseconds since the journal came up: the clock every event's t_ms is read from.
		[[nodiscard]] std::uint64_t elapsed_ms() const;
		// The same clock in microseconds, for an instant taken elsewhere (a network-thread read stamp); 0 before it.
		[[nodiscard]] std::uint64_t micros_at(std::chrono::steady_clock::time_point instant) const;

	private:
		agent_journal();

		// The component a captured log line came from: this journal only sees the client's own logger.
		static constexpr std::string_view log_component_name = "client";

		// The log observer: folds warnings and errors into client.warnings / client.errors. Runs on whichever
		// thread logged and must neither log nor throw.
		static void on_log_line(void* context, int entry_level, std::string_view message);
		// The wire name of a log level (log_line's `level` field).
		[[nodiscard]] static std::string_view level_name(int entry_level);

		[[nodiscard]] static constexpr std::uint32_t channel_bit(journal_channel channel)
		{
			return 1u << static_cast<std::uint32_t>(channel);
		}

		// Records the eviction of one event, counting it as dropped when nobody had read it yet.
		// Caller holds m_mutex.
		void note_evicted(const journal_event& evicted);

		// Every channel on: recording is on by default in a testing build (the contract), and a caller
		// switches off what it does not want.
		std::atomic<std::uint32_t> m_enabled_mask{~0u};
		std::atomic<std::uint64_t> m_frame{0};

		mutable std::mutex m_mutex;
		std::vector<journal_event> m_ring;  // m_ring.size() IS the capacity; m_head/m_size ride inside it
		std::size_t m_head = 0;             // index of the oldest retained event
		std::size_t m_size = 0;
		std::uint64_t m_next_seq = 1;       // seq 0 is never used, so `since: 0` means "everything retained"
		std::uint64_t m_read_cursor = 0;    // the cursor the last drain reported
		std::uint64_t m_dropped_unread = 0; // evicted with seq >= m_read_cursor since that drain
		std::chrono::steady_clock::time_point m_started_at;
	};

	// Records one event when its channel is on. `build` (a lambda filling journal_fields) runs ONLY then, so
	// a disabled channel costs the enabled() test and nothing else.
	template <typename build_fields>
	void journal_record(journal_channel channel, std::string_view type, build_fields&& build)
	{
		agent_journal& journal = agent_journal::get();
		if (!journal.enabled(channel))
		{
			return;
		}
		journal_fields fields;
		build(fields);
		journal.append(channel, type, fields.take());
	}

	// A log_line's message is cut to this many bytes, so one long line cannot bloat the ring or a reply.
	constexpr std::size_t journal_max_log_message = 1024;

	// net.packet event for a send or receive: the message id, type and size only, never a byte of the payload (a
	// login packet carries credentials). Defined in game.cpp; observes only.
	void journal_note_packet(const char* type, const char* link, const void* data, std::size_t size);

	// A span in whole microseconds: the unit of every `*_us` journal field.
	[[nodiscard]] inline std::int64_t journal_micros(std::chrono::steady_clock::duration span)
	{
		return std::chrono::duration_cast<std::chrono::microseconds>(span).count();
	}

} // namespace hb::client

#endif // DEF_CLAUDE_TESTING
