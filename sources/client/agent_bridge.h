#pragma once

// Agent bridge (HH-0037, phase 1a-pre): a loopback-only, newline-delimited JSON command channel that lets an
// external agent (the MCP server) inspect the client. Debug builds only: the whole file is inside the guard,
// so a Release build has no listener, no command table and none of these strings (claude_testing.h, d-027).
//
// Wire: one request per line, {"id": any, "cmd": "name", "args": {...}}; one reply per line,
// {"id": same, "ok": true, "result": {...}} or {"id": same, "ok": false, "error": "text"}.
// Polled on the main thread once per update (CGame::on_update), so nothing here is threaded.
#include "claude_testing.h"

#ifdef DEF_CLAUDE_TESTING

#include "asio_socket.h" // the project's ASIO configuration, then asio.hpp
#include "json.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

class CGame;
namespace hb::shared::render { class IRenderer; }

namespace hb::client
{
	class agent_bridge
	{
	public:
		using json = nlohmann::json;

		// Bumped when a command's arguments or result keys change incompatibly
		static constexpr int bridge_protocol = 1;

		agent_bridge();
		~agent_bridge();
		agent_bridge(const agent_bridge&) = delete;
		agent_bridge& operator=(const agent_bridge&) = delete;

		// Binds 127.0.0.1:port (never another address, never SO_REUSEADDR). False, logged once, if the bind fails.
		// start, poll and on_pre_present never throw: a failure inside them logs once and disables the bridge.
		bool start(std::uint16_t port);

		// Accepts, reads, dispatches and flushes; call once per update, before the game's own update
		void poll(CGame& active_game);

		// Called after the last draw and before the present: captures the screenshots requested since the last
		// presented frame and answers their deferred replies
		void on_pre_present(hb::shared::render::IRenderer& renderer);

	private:
		using clock = std::chrono::steady_clock;

		// false = the reply is deferred (registered in m_deferred), nothing is owed yet
		using handler = bool (agent_bridge::*)(const json& id, const json& args, CGame& active_game, json& result,
			std::string& error);

		struct command_entry
		{
			std::string_view name;
			handler run;
		};

		enum class deferred_kind { screenshot };

		// A request answered on a later frame (screenshot now; wait_for in phase 1a)
		struct deferred_reply
		{
			json id;
			deferred_kind kind;
			std::string file_name;
			clock::time_point deadline;
		};

		static const command_entry commands[];

		bool run_ping(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_get_state(const json& id, const json& args, CGame& active_game, json& result, std::string& error);
		bool run_screenshot(const json& id, const json& args, CGame& active_game, json& result, std::string& error);

		// the bodies of start / poll / on_pre_present, which catch whatever escapes them and call disable
		bool open_listener(std::uint16_t port);
		void poll_client(CGame& active_game);
		void capture_due(hb::shared::render::IRenderer& renderer);
		void disable(std::string_view where, std::string_view what);

		void accept_client();
		bool read_client();
		void dispatch_line(std::string_view line, CGame& active_game);
		void queue_reply(const json& id, const json& result, const std::string& error);
		bool flush_outbound();
		void drop_client(std::string_view reason);
		void expire_deferred();

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
	};
}

#endif
