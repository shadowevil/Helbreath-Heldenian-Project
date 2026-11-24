#pragma once

// Agent bridge (HH-0037 core, HH-0038 input): a loopback-only, newline-delimited JSON command channel that lets
// an external agent (the MCP server) inspect and drive the client. Debug builds only: the whole file is inside
// the guard, so a Release build has no listener, no command table and none of these strings (claude_testing.h,
// d-027).
//
// Wire: one request per line, {"id": any, "cmd": "name", "args": {...}}; one reply per line,
// {"id": same, "ok": true, "result": {...}} or {"id": same, "ok": false, "error": "text"}.
// Polled on the main thread once per update (CGame::on_update), so nothing here is threaded.
//
// Widgets (HH-0040): `widgets` lists the named controls of what takes input (the overlay if one is up, else the
// screen; in the world, the legacy dialogs as coarse windows). Names are "<screen>.<control>" from each screen's
// agent_control_names(). `widget_click {name}` clicks a widget's centre through the input queue; click, mouse_down,
// mouse_up and probe_point report what the point reaches ("consumed_by" / "front"; with count 2 or 3 it reflects the
// last press). widget_click fails with "widget_stale" if the widget is gone when the press runs. A masked field
// reports neither text nor length.
//
// Input is injected through the window's IWindowEventHandler, the calls SFMLWindow makes for real events, one
// queued step per presented frame (legacy dialogs read the mouse in on_draw, which only presented frames run).
// A real mouse moving over the window overwrites the injected position; while a pointer job runs its position
// is re-sent at the top of every update, so the real mouse only wins between jobs.
// The modifiers latch is a held left shift/ctrl/alt key, the same key state a real keyboard writes: a player
// pressing and releasing that real key while it is latched releases it for the game too, and the latch then
// reports a key that is no longer down (it is only what the bridge last asked for).
#include "claude_testing.h"

#ifdef DEF_CLAUDE_TESTING

#include "agent_journal.h"
#include "asio_socket.h" // the project's ASIO configuration, then asio.hpp
#include "input.h"       // KeyCode
#include "json.hpp"
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class CGame;
class IGameScreen;
namespace hb::shared::render { class IRenderer; class IWindow; class IWindowEventHandler; }

namespace hb::client
{
	class agent_bridge
	{
	public:
		using json = nlohmann::json;

		// Bumped when a command's arguments or result keys change incompatibly
		static constexpr int bridge_protocol = 4;

		agent_bridge();
		~agent_bridge();
		agent_bridge(const agent_bridge&) = delete;
		agent_bridge& operator=(const agent_bridge&) = delete;

		// Binds 127.0.0.1:port (never another address, never SO_REUSEADDR). False, logged once, if the bind fails.
		// None of the four entry points throws: a failure inside one logs once and disables the bridge.
		bool start(std::uint16_t port);

		// Accepts, reads, dispatches, advances the input queue, settles waits and flushes; call once per update,
		// before the game's own update
		void poll(CGame& active_game);

		// Called after the last draw and before the present: captures the screenshots requested since the last
		// presented frame and answers their deferred replies
		void on_pre_present(hb::shared::render::IRenderer& renderer);

		// Called after the present: lets the input queue take its next step, and answers a finished job
		void frame_presented();

	private:
		using clock = std::chrono::steady_clock;

		// false = the reply is deferred (a later frame answers it), nothing is owed yet
		using handler = bool (agent_bridge::*)(const json& id, const json& args, CGame& active_game, json& result,
			std::string& error);

		struct command_entry
		{
			std::string_view name;
			handler run;
		};

		enum class deferred_kind { screenshot, wait };

		// One wait_for condition: a state key equal to a value, or a journal event matching channel/type/fields
		struct wait_condition
		{
			json source;            // echoed back in the reply's `matched`
			bool is_state = false;
			std::string state_key;  // state form
			json equals;
			std::string channel;    // event form: a dotted prefix, matched by whole segment
			std::string type;       // empty = any type
			json match;             // top-level data keys that must equal these values
			journal_data_filter data;
		};

		// A request answered on a later frame: a screenshot, or a wait_for
		struct deferred_reply
		{
			json id;
			deferred_kind kind;
			std::string file_name; // screenshot
			json conditions;       // wait: the raw conditions array
			clock::time_point deadline;
			std::vector<wait_condition> parsed; // wait: the same, read once at registration
			std::uint64_t scan_seq = 0;    // wait: events from here on are candidates ("did it happen next")
			std::uint64_t match_from = 0;  // wait: echoed as scan_from
			std::uint64_t trail_since = 0; // wait: where the reply's event trail starts
		};

		enum class step_kind { move, button_down, button_up, wheel, key_down, key_up, chars };

		struct input_step
		{
			step_kind kind;
			int x = 0;
			int y = 0;
			int button = 0;
			int delta = 0;
			KeyCode key = KeyCode::Unknown;
			std::vector<std::uint32_t> chars;
		};

		// One command's steps; each waits for a presented frame, and the reply waits for one more
		struct input_job
		{
			json id;
			std::vector<input_step> steps;
			std::size_t next = 0;
			json result;
			std::chrono::milliseconds timeout{};
			clock::time_point deadline{}; // set when the job reaches the front of the queue
			bool started = false;
			bool pointer = false; // re-send the pointer position each update while it runs
			int pointer_x = 0;
			int pointer_y = 0;
			bool report_front = false; // add "consumed_by" to the reply, taken just before front_step runs
			step_kind front_step = step_kind::button_down;
			std::string widget; // widget_click: re-aimed at the widget's centre as the job starts and before the press
		};

		// One listed widget: a named CControls control (or one half of a toggle), or a legacy dialog. Rect in UI units.
		struct widget_row
		{
			std::string name;
			std::string kind;  // button | text_field | window
			std::string owner; // "screen:<type>" or "dialog:<name>"
			int x = 0;
			int y = 0;
			int w = 0;
			int h = 0;
			bool enabled = true;
			bool visible = true;
			bool focused = false;
			bool is_field = false;
			bool masked = false;
			std::size_t length = 0; // characters; only reported for an unmasked field
			bool is_dialog = false;
			int dialog_id = 0;

			// CControls: left/top edge in, right/bottom out; legacy dialogs hit-test strictly inside
			bool contains(int px, int py) const
			{
				return is_dialog ? (px > x && px < x + w && py > y && py < y + h)
					: (px >= x && px < x + w && py >= y && py < y + h);
			}
		};

		static const command_entry commands[];

		bool run_ping(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_get_state(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_screenshot(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_probe_point(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_mouse_move(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_click(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_mouse_down(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_mouse_up(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_wheel(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_key(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_text(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_set_text(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_modifiers(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_wait_for(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_widgets(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_widget_click(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_journal_query(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_journal_drain(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_journal_config(const json& id, const json& args, CGame& active_game, json& result, std::string& error);

		// journal commands: argument readers (false sets `error`) and reply builders
		static bool read_selector(const json& args, journal_selector& out, std::string& error);
		static bool read_data_filter(const json& args, journal_data_filter& out, std::string& error);
		static bool read_window(const json& args, journal_window& out, std::string& error);
		static json journal_event_json(const journal_event& recorded);
		static json journal_events_json(const std::vector<journal_event>& events);
		static bool event_satisfies(const wait_condition& condition, const journal_event& recorded, const json& data);
		void append_wait_trail(const deferred_reply& wait, json& result) const;

		// the bodies of the entry points, which catch whatever escapes them and call disable
		bool open_listener(std::uint16_t port);
		void poll_client(CGame& active_game);
		void capture_due(hb::shared::render::IRenderer& renderer);
		void advance_on_frame();
		void disable(std::string_view where, std::string_view what);

		void accept_client();
		bool read_client();
		void dispatch_line(std::string_view line, CGame& active_game);
		void queue_reply(const json& id, const json& result, const std::string& error);
		bool flush_outbound();
		void drop_client(std::string_view reason);
		void expire_deferred();

		json state_snapshot(CGame& active_game) const;
		void settle_waits(CGame& active_game);

		// input
		bool resolve_xy(const json& args, CGame& active_game, int& x, int& y, std::string& error) const;
		bool read_job_timeout(const json& args, std::chrono::milliseconds& timeout, std::string& error) const;
		bool enqueue(const json& id, input_job job, std::string& error);
		bool queue_click(const json& id, const json& args, int x, int y, const json& extra, std::string& error,
			const std::string& widget = {});
		std::vector<widget_row> input_rows(CGame& active_game) const;
		bool widget_centre(CGame& active_game, const std::string& name, int& x, int& y, bool& enabled,
			std::string& error) const;
		void advance_input(CGame& active_game);
		void execute(const input_step& step);

		// widgets: what takes input now is the overlay if one is up, else the screen; in the world, the dialogs
		std::vector<widget_row> control_rows(IGameScreen* surface) const;
		std::vector<widget_row> dialog_rows(CGame& active_game) const;
		json widget_json(const widget_row& row) const;
		// what a point reaches first: {kind, owner, name?}, mirroring the routing (overlay, transition, dialogs)
		json front_at(CGame& active_game, int x, int y) const;
		void release_held();
		void modifier_steps(bool shift, bool ctrl, bool alt, bool press, std::vector<input_step>& steps) const;

		void capture_screenshot(hb::shared::render::IRenderer& renderer, const std::string& file_name, json& result,
			std::string& error);
		void enforce_folder_cap(const std::filesystem::path& keep);

		asio::io_context m_io; // private: never the game's m_io_pool
		asio::ip::tcp::acceptor m_acceptor;
		asio::ip::tcp::socket m_client;
		bool m_listening = false;
		bool m_connected = false;
		std::string m_inbound;
		std::string m_outbound;
		std::vector<deferred_reply> m_deferred;
		std::filesystem::path m_screenshot_dir;

		// the window's own handler and handle, captured on the first poll; the game outlives the bridge
		hb::shared::render::IWindow* m_window = nullptr;
		hb::shared::render::IWindowEventHandler* m_handler = nullptr;
		std::deque<input_job> m_jobs;
		bool m_frame_since_step = true; // a frame was presented since the last step ran
		std::set<int> m_held_buttons;
		std::set<KeyCode> m_held_keys;
		int m_last_x = 0;
		int m_last_y = 0;
		bool m_latch_shift = false;
		bool m_latch_ctrl = false;
		bool m_latch_alt = false;
	};
}

#endif
