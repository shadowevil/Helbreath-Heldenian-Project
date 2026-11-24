// agent_bridge.cpp: the debug-only agent command channel (HH-0037, HH-0038). See agent_bridge.h for the wire format.
#include "agent_bridge.h"

#ifdef DEF_CLAUDE_TESTING

#include "game.h"
#include "game_mode_manager.h"
#include "screen_on_game.h"
#include "player.h"
#include "renderer.h"
#include "input.h"
#include "version.h"
#include "log.h"
#include "updater_file_ops.h" // hb::updater::get_exe_path, the client's existing cross-platform exe lookup
#include "window.h"
#include "ccontrols.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <exception>
#include <optional>
#include <format>
#include <fstream>

namespace hb::client
{
	namespace
	{
		constexpr std::size_t max_line_bytes = 64 * 1024;        // a request line longer than this drops the client
		constexpr std::size_t max_read_per_poll = 256 * 1024;    // bounds the work one update spends reading
		constexpr std::size_t max_outbound_bytes = 4 * 1024 * 1024; // a client that stops reading is dropped
		constexpr std::size_t max_deferred = 16;
		constexpr std::size_t max_screenshot_name = 96;
		constexpr std::uintmax_t screenshot_folder_cap = 20ull * 1024 * 1024; // d-028: SDL's 20 MB default
		constexpr auto screenshot_deadline = std::chrono::seconds(10); // no presented frame by then = an error
		constexpr char temp_prefix = '~'; // outside the name whitelist, so it never meets a real name
		constexpr int max_json_depth = 32;  // deeper values are refused: copy and dump of json recurse

		// UTF-8 for logs and replies; path::string() throws on Windows for names outside the code page
		std::string path_utf8(const std::filesystem::path& path)
		{
			const std::u8string text = path.u8string();
			return std::string(reinterpret_cast<const char*>(text.data()), text.size());
		}

		bool is_temp_name(const std::filesystem::path& path)
		{
			const auto& name = path.filename().native(); // wide on Windows, narrow elsewhere; no conversion
			return !name.empty() && name.front() == temp_prefix;
		}

		// The repo root is the first ancestor holding both sources/ and staged/, searched from the executable's
		// folder (game/client in a dev tree), then from the working directory; else <cwd>/build/screenshots
		std::filesystem::path resolve_screenshot_dir()
		{
			std::error_code ec;
			const std::filesystem::path cwd = std::filesystem::current_path(ec);
			const std::string exe = hb::updater::get_exe_path();
			const std::array<std::filesystem::path, 2> starts = {
				exe.empty() ? std::filesystem::path{} : std::filesystem::path(exe).parent_path(), cwd};
			for (const std::filesystem::path& start : starts)
			{
				for (std::filesystem::path dir = start; !dir.empty(); dir = dir.parent_path())
				{
					if (std::filesystem::is_directory(dir / "sources", ec) && std::filesystem::is_directory(dir / "staged", ec))
					{
						return dir / "build" / "screenshots";
					}
					if (dir == dir.parent_path())
					{
						break;
					}
				}
			}
			return cwd / "build" / "screenshots";
		}

		constexpr bool is_ascii_alnum(char c)
		{
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
		}

		bool is_reserved_device_name(std::string_view stem)
		{
			// Windows refuses these as file names whatever the extension
			constexpr std::array<std::string_view, 4> plain = {"con", "prn", "aux", "nul"};
			std::string lower(stem);
			std::transform(lower.begin(), lower.end(), lower.begin(),
				[](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; });
			if (std::find(plain.begin(), plain.end(), lower) != plain.end())
			{
				return true;
			}
			return lower.size() == 4 && (lower.starts_with("com") || lower.starts_with("lpt")) && lower[3] >= '1' && lower[3] <= '9';
		}

		// A bare file name: [A-Za-z0-9._-] only, ends in .png, no leading dot, no "..", no device name
		bool valid_screenshot_name(std::string_view name)
		{
			constexpr std::string_view extension = ".png";
			if (name.size() <= extension.size() || name.size() > max_screenshot_name || name.front() == '.'
				|| !name.ends_with(extension) || name.find("..") != std::string_view::npos)
			{
				return false;
			}
			const bool allowed = std::all_of(name.begin(), name.end(), [](char c)
			{
				return is_ascii_alnum(c) || c == '.' || c == '_' || c == '-';
			});
			return allowed && !is_reserved_device_name(name.substr(0, name.find('.')));
		}

		// Width and height from the PNG signature and IHDR chunk; false if the file is not a PNG
		bool read_png_size(const std::filesystem::path& path, std::uint32_t& width, std::uint32_t& height)
		{
			std::array<unsigned char, 24> header{};
			std::ifstream file(path, std::ios::binary);
			if (!file.read(reinterpret_cast<char*>(header.data()), header.size()))
			{
				return false;
			}
			constexpr std::array<unsigned char, 8> signature = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
			if (!std::equal(signature.begin(), signature.end(), header.begin())
				|| header[12] != 'I' || header[13] != 'H' || header[14] != 'D' || header[15] != 'R')
			{
				return false;
			}
			const auto be32 = [&header](std::size_t at)
			{
				return (std::uint32_t{header[at]} << 24) | (std::uint32_t{header[at + 1]} << 16)
					| (std::uint32_t{header[at + 2]} << 8) | std::uint32_t{header[at + 3]};
			};
			width = be32(16);
			height = be32(20);
			return true;
		}

		agent_bridge::json screen_id_or_null(const IGameScreen* screen)
		{
			if (screen == nullptr || screen->get_type_id() == nullptr)
			{
				return nullptr;
			}
			return screen->get_type_id();
		}
	}

	// The one dispatch table; ping's commands[] is built from it, so the list never drifts from the code
	const agent_bridge::command_entry agent_bridge::commands[] = {
		{"ping", &agent_bridge::run_ping},
		{"get_state", &agent_bridge::run_get_state},
		{"screenshot", &agent_bridge::run_screenshot},
		{"probe_point", &agent_bridge::run_probe_point},
		{"mouse_move", &agent_bridge::run_mouse_move},
		{"click", &agent_bridge::run_click},
		{"mouse_down", &agent_bridge::run_mouse_down},
		{"mouse_up", &agent_bridge::run_mouse_up},
		{"wheel", &agent_bridge::run_wheel},
		{"key", &agent_bridge::run_key},
		{"text", &agent_bridge::run_text},
		{"set_text", &agent_bridge::run_set_text},
		{"modifiers", &agent_bridge::run_modifiers},
		{"wait_for", &agent_bridge::run_wait_for},
	};

	agent_bridge::agent_bridge()
		: m_acceptor(m_io), m_client(m_io)
	{
	}

	agent_bridge::~agent_bridge()
	{
		std::error_code ec;
		m_client.close(ec);
		m_acceptor.close(ec);
	}

	// The entry points from the frame loop: anything that still throws disables the bridge, never the game
	bool agent_bridge::start(std::uint16_t port)
	{
		try
		{
			return open_listener(port);
		}
		catch (const std::exception& failure)
		{
			disable("start", failure.what());
		}
		catch (...)
		{
			disable("start", "unknown");
		}
		return false;
	}

	void agent_bridge::poll(CGame& active_game)
	{
		try
		{
			poll_client(active_game);
		}
		catch (const std::exception& failure)
		{
			disable("poll", failure.what());
		}
		catch (...)
		{
			disable("poll", "unknown");
		}
	}

	void agent_bridge::on_pre_present(hb::shared::render::IRenderer& renderer)
	{
		try
		{
			capture_due(renderer);
		}
		catch (const std::exception& failure)
		{
			disable("capture", failure.what());
		}
		catch (...)
		{
			disable("capture", "unknown");
		}
	}

	void agent_bridge::frame_presented()
	{
		try
		{
			advance_on_frame();
		}
		catch (const std::exception& failure)
		{
			disable("frame", failure.what());
		}
		catch (...)
		{
			disable("frame", "unknown");
		}
	}

	void agent_bridge::disable(std::string_view where, std::string_view what)
	{
		hb::logger::warn("agent bridge: {} failed ({}) - bridge disabled", where, what);
		try
		{
			release_held(); // nothing the bridge pressed stays down
		}
		catch (...)
		{
		}
		m_jobs.clear();
		std::error_code ignored;
		m_client.close(ignored);
		m_acceptor.close(ignored);
		m_listening = false;
		m_connected = false;
		m_inbound.clear();
		m_outbound.clear();
		m_deferred.clear();
	}

	bool agent_bridge::open_listener(std::uint16_t port)
	{
		// Opened and bound by hand: the endpoint constructor would set reuse_address, which on Windows lets a
		// second process bind the same port
		const asio::ip::tcp::endpoint endpoint(asio::ip::address_v4::loopback(), port);
		std::error_code ec;
		m_acceptor.open(endpoint.protocol(), ec);
		if (!ec)
		{
			m_acceptor.bind(endpoint, ec);
		}
		if (!ec)
		{
			m_acceptor.listen(asio::socket_base::max_listen_connections, ec);
		}
		if (!ec)
		{
			m_acceptor.non_blocking(true, ec);
		}
		if (ec)
		{
			hb::logger::warn("agent bridge: could not listen on 127.0.0.1:{} ({}) - bridge disabled", port, ec.message());
			std::error_code ignored;
			m_acceptor.close(ignored);
			return false;
		}
		m_listening = true;

		m_screenshot_dir = resolve_screenshot_dir();
		std::filesystem::create_directories(m_screenshot_dir, ec);
		// a temporary left by a crash mid-save would otherwise sit there forever
		for (std::filesystem::directory_iterator it(m_screenshot_dir, ec), end; !ec && it != end; it.increment(ec))
		{
			std::error_code entry_ec;
			if (it->is_regular_file(entry_ec) && it->path().extension() == ".png" && is_temp_name(it->path()))
			{
				std::filesystem::remove(it->path(), entry_ec);
			}
		}
		hb::logger::log("agent bridge: listening on 127.0.0.1:{} (debug testing hook), screenshots in {}", port,
			path_utf8(m_screenshot_dir));
		return true;
	}

	void agent_bridge::poll_client(CGame& active_game)
	{
		if (!m_listening)
		{
			return;
		}
		if (m_handler == nullptr)
		{
			// the exact handler and window SFMLWindow delivers real events to
			m_window = active_game.get_window();
			m_handler = m_window != nullptr ? m_window->get_event_handler() : nullptr;
		}
		if (!m_connected)
		{
			accept_client();
			if (!m_connected)
			{
				return;
			}
		}
		if (!read_client())
		{
			return;
		}

		// walk the buffer by offset and erase the consumed part once: per-line erases are quadratic
		const std::string inbound = std::move(m_inbound);
		m_inbound.clear();
		std::size_t start = 0;
		for (std::size_t newline = inbound.find('\n'); m_connected && newline != std::string::npos;
			newline = inbound.find('\n', start))
		{
			std::string_view line(inbound.data() + start, newline - start);
			start = newline + 1;
			if (!line.empty() && line.back() == '\r')
			{
				line.remove_suffix(1); // tolerate CRLF from a line-oriented client
			}
			if (line.size() > max_line_bytes)
			{
				queue_reply(nullptr, {}, std::format("request line exceeds {} bytes", max_line_bytes));
			}
			else if (!line.empty())
			{
				dispatch_line(line, active_game);
			}
		}
		if (!m_connected)
		{
			return;
		}
		m_inbound.assign(inbound, std::min(start, inbound.size()));
		if (m_inbound.size() > max_line_bytes)
		{
			queue_reply(nullptr, {}, std::format("request line exceeds {} bytes", max_line_bytes));
			flush_outbound();
			drop_client("request line over the cap - dropping client");
			return;
		}

		advance_input();
		settle_waits(active_game);
		expire_deferred();
		flush_outbound();
	}

	void agent_bridge::capture_due(hb::shared::render::IRenderer& renderer)
	{
		if (!m_connected || m_deferred.empty())
		{
			return;
		}
		// take the list first: a capture failure can queue a reply that drops the client and clears it
		std::vector<deferred_reply> due;
		std::erase_if(m_deferred, [&due](deferred_reply& pending)
		{
			if (pending.kind != deferred_kind::screenshot)
			{
				return false;
			}
			due.push_back(std::move(pending));
			return true;
		});
		for (const deferred_reply& pending : due)
		{
			if (!m_connected)
			{
				break; // a reply backlog dropped the client: nobody is left to answer
			}
			json result = json::object();
			std::string error;
			capture_screenshot(renderer, pending.file_name, result, error);
			queue_reply(pending.id, result, error);
		}
		flush_outbound();
	}

	void agent_bridge::accept_client()
	{
		std::error_code ec;
		m_acceptor.accept(m_client, ec);
		if (ec)
		{
			return; // would_block: nobody is waiting
		}
		// set on the accepted socket itself; it is not inherited from the acceptor everywhere
		m_client.non_blocking(true, ec);
		if (!ec)
		{
			m_client.set_option(asio::ip::tcp::no_delay(true), ec);
		}
		if (ec)
		{
			std::error_code ignored;
			m_client.close(ignored);
			return;
		}
		m_connected = true;
		m_inbound.clear();
		m_outbound.clear();
		m_deferred.clear(); // a fresh connection inherits nothing from the last one
		hb::logger::log("agent bridge: client connected");
	}

	bool agent_bridge::read_client()
	{
		std::array<char, 16 * 1024> buffer{};
		std::size_t total = 0;
		while (total < max_read_per_poll)
		{
			std::error_code ec;
			const std::size_t bytes = m_client.read_some(asio::buffer(buffer), ec);
			if (ec == asio::error::would_block || ec == asio::error::try_again)
			{
				break;
			}
			if (ec || bytes == 0)
			{
				drop_client("client disconnected");
				return false;
			}
			m_inbound.append(buffer.data(), bytes);
			total += bytes;
		}
		return true;
	}

	void agent_bridge::dispatch_line(std::string_view line, CGame& active_game)
	{
		// The parser itself is iterative; a container deeper than the limit is skipped, never stored, so the
		// request copied and dumped below stays shallow
		bool too_deep = false;
		const json::parser_callback_t depth_limit = [&too_deep](int depth, json::parse_event_t event, json&)
		{
			if ((event == json::parse_event_t::object_start || event == json::parse_event_t::array_start)
				&& depth >= max_json_depth)
			{
				too_deep = true;
				return false;
			}
			return true;
		};
		const json request = json::parse(line, depth_limit, false);
		if (request.is_discarded())
		{
			queue_reply(nullptr, {}, "invalid JSON");
			return;
		}
		if (too_deep)
		{
			queue_reply(nullptr, {}, std::format("request nests deeper than {} levels", max_json_depth));
			return;
		}
		if (!request.is_object())
		{
			queue_reply(nullptr, {}, "request must be a JSON object");
			return;
		}
		const json id = request.value("id", json(nullptr));
		if (!id.is_null() && !id.is_string() && !id.is_number())
		{
			queue_reply(nullptr, {}, "id must be a string, a number or null");
			return;
		}

		const auto cmd = request.find("cmd");
		if (cmd == request.end() || !cmd->is_string())
		{
			queue_reply(id, {}, "request needs a string 'cmd'");
			return;
		}
		json args = json::object();
		if (const auto found = request.find("args"); found != request.end() && !found->is_null())
		{
			if (!found->is_object())
			{
				queue_reply(id, {}, "args must be an object");
				return;
			}
			args = *found;
		}

		const std::string& name = cmd->get_ref<const std::string&>();
		const auto entry = std::find_if(std::begin(commands), std::end(commands),
			[&name](const command_entry& candidate) { return candidate.name == name; });
		if (entry == std::end(commands))
		{
			queue_reply(id, {}, "unknown cmd: " + name.substr(0, 64));
			return;
		}

		json result = json::object();
		std::string error;
		try
		{
			if (!(this->*(entry->run))(id, args, active_game, result, error))
			{
				return; // deferred: answered on a later frame
			}
		}
		catch (const std::exception& failure)
		{
			// the backstop: a command must never unwind into the frame loop
			result = json::object();
			error = std::format("command '{}' failed: {}", entry->name, failure.what());
		}
		queue_reply(id, result, error);
	}

	void agent_bridge::queue_reply(const json& id, const json& result, const std::string& error)
	{
		if (!m_connected)
		{
			return;
		}
		json reply;
		reply["id"] = id;
		reply["ok"] = error.empty();
		if (error.empty())
		{
			reply["result"] = result.is_null() ? json::object() : result;
		}
		else
		{
			reply["error"] = error;
		}
		// replace: a legacy name in a non-UTF-8 code page must not throw out of the frame loop
		m_outbound += reply.dump(-1, ' ', false, json::error_handler_t::replace);
		m_outbound += '\n';
		if (m_outbound.size() > max_outbound_bytes)
		{
			drop_client("reply backlog over the cap - dropping client");
		}
	}

	bool agent_bridge::flush_outbound()
	{
		while (m_connected && !m_outbound.empty())
		{
			std::error_code ec;
			const std::size_t bytes = m_client.write_some(asio::buffer(m_outbound), ec);
			if (ec == asio::error::would_block || ec == asio::error::try_again)
			{
				return true; // the rest goes on the next poll
			}
			if (ec)
			{
				drop_client("reply send failed - dropping client");
				return false;
			}
			m_outbound.erase(0, bytes);
		}
		return m_connected;
	}

	void agent_bridge::drop_client(std::string_view reason)
	{
		hb::logger::log("agent bridge: {}", reason);
		std::error_code ignored;
		m_client.close(ignored);
		m_connected = false;
		m_inbound.clear();
		m_outbound.clear();
		// a deferred reply sent into a new connection would answer a request that client never made
		m_deferred.clear();
		// and nothing the bridge pressed stays down for the next one
		m_jobs.clear();
		release_held();
	}

	void agent_bridge::expire_deferred()
	{
		// screenshots only: a wait that times out is an ordinary outcome, answered by settle_waits
		const clock::time_point now = clock::now();
		std::vector<json> expired;
		std::erase_if(m_deferred, [&](const deferred_reply& pending)
		{
			if (pending.kind != deferred_kind::screenshot || pending.deadline > now)
			{
				return false;
			}
			expired.push_back(pending.id);
			return true;
		});
		for (const json& id : expired)
		{
			queue_reply(id, {}, std::format("no frame was presented within {} s (minimized?)", screenshot_deadline.count()));
		}
	}

	//-------------------------------------------------------------------------
	// commands
	//-------------------------------------------------------------------------

	bool agent_bridge::run_ping(const json&, const json&, CGame&, json& result, std::string&)
	{
		namespace version = hb::shared::version;
		result["pong"] = true;
		result["build"] = version::version_string(version::component::client);
		result["release"] = version::release_version(version::component::client);
		result["channel"] = version::channel_name();
		result["build_id"] = version::build_id();
		result["source_clean"] = version::source_clean();
		result["commit"] = version::commit_sha();
		result["protocol_version"] = version::protocol_version;
		result["bridge_protocol"] = bridge_protocol;
		json names = json::array();
		for (const command_entry& entry : commands)
		{
			names.push_back(entry.name);
		}
		result["commands"] = std::move(names);
		return true;
	}

	bool agent_bridge::run_get_state(const json&, const json&, CGame& active_game, json& result, std::string&)
	{
		result = state_snapshot(active_game);
		return true;
	}

	agent_bridge::json agent_bridge::state_snapshot(CGame& active_game) const
	{
		// read-only; flat keys, because wait_for reads top-level keys only
		json result;
		result["screen"] = screen_id_or_null(GameModeManager::get_active_screen());
		result["overlay"] = screen_id_or_null(GameModeManager::get_active_overlay());
		result["transitioning"] = GameModeManager::is_transitioning();
		result["mouse_x"] = hb::shared::input::get_mouse_x(); // UI units (800x600 design space)
		result["mouse_y"] = hb::shared::input::get_mouse_y();
		if (const auto* window = active_game.get_window())
		{
			result["window_w"] = window->get_width();
			result["window_h"] = window->get_height();
		}

		Screen_OnGame* on_game = active_game.on_game();
		result["in_world"] = on_game != nullptr && active_game.m_player != nullptr;
		if (on_game != nullptr && active_game.m_player != nullptr)
		{
			result["character"] = active_game.m_player->m_player_name;
			result["player_x"] = active_game.m_player->m_player_x;
			result["player_y"] = active_game.m_player->m_player_y;
			result["map"] = active_game.m_map_name;
			const int top = on_game->get_dialog_box_manager().get_top_id();
			result["top_dialog"] = top != 0 ? json(top) : json(nullptr);
		}

		// focus on a screen that exposes its CControls (the overlay first: it takes the input). Field text never
		// leaves the client; a masked field reports neither its text nor its length.
		IGameScreen* front = GameModeManager::get_active_overlay();
		if (front == nullptr || front->agent_controls() == nullptr)
		{
			front = GameModeManager::get_active_screen();
		}
		cc::control_collection* controls = front != nullptr ? front->agent_controls() : nullptr;
		result["focus_id"] = nullptr;
		if (controls != nullptr)
		{
			const int focused = controls->focused_id();
			result["focus_id"] = focused >= 0 ? json(focused) : json(nullptr); // -1 = nothing focused
			if (const auto* field = dynamic_cast<const cc::text_input*>(controls->find(focused)))
			{
				result["focus_masked"] = field->is_hidden();
				if (!field->is_hidden())
				{
					// characters, not bytes: count the UTF-8 lead bytes
					const std::string& text = field->text();
					result["focus_text_length"] = std::count_if(text.begin(), text.end(),
						[](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; });
				}
			}
		}

		result["latch_shift"] = m_latch_shift;
		result["latch_ctrl"] = m_latch_ctrl;
		result["latch_alt"] = m_latch_alt;
		result["input_pending"] = m_jobs.size();
		return result;
	}

	bool agent_bridge::run_screenshot(const json& id, const json& args, CGame&, json&, std::string& error)
	{
		if (args.contains("path"))
		{
			error = "screenshot takes 'name', a bare file name saved under build/screenshots/; 'path' is refused";
			return true;
		}
		const auto name = args.find("name");
		if (name == args.end() || !name->is_string())
		{
			error = "screenshot requires a string 'name'";
			return true;
		}
		const std::string& file_name = name->get_ref<const std::string&>();
		if (!valid_screenshot_name(file_name))
		{
			error = std::format("invalid name: a bare file name of [A-Za-z0-9._-], at most {} characters, ending in .png",
				max_screenshot_name);
			return true;
		}
		if (m_deferred.size() >= max_deferred)
		{
			error = "too many requests pending";
			return true;
		}
		// captured just before the next present, then answered
		m_deferred.push_back(deferred_reply{.id = id, .kind = deferred_kind::screenshot, .file_name = file_name,
			.deadline = clock::now() + screenshot_deadline});
		return false;
	}

	void agent_bridge::capture_screenshot(hb::shared::render::IRenderer& renderer, const std::string& file_name,
		json& result, std::string& error)
	{
		const std::filesystem::path final_path = m_screenshot_dir / file_name;
		// the temporary keeps the .png suffix: the image writer picks its format from the extension
		const std::filesystem::path temp_path = m_screenshot_dir / (temp_prefix + file_name);
		std::error_code ec;
		std::filesystem::create_directories(m_screenshot_dir, ec);
		const bool overwritten = std::filesystem::exists(final_path, ec);
		// the renderer takes a narrow path, which cannot name a folder outside the code page on Windows
		std::string narrow_temp;
		try
		{
			narrow_temp = temp_path.string();
		}
		catch (const std::exception&)
		{
			error = "the screenshot folder cannot be named in the system code page: " + path_utf8(m_screenshot_dir);
			return;
		}
		if (!renderer.screenshot(narrow_temp.c_str()))
		{
			std::filesystem::remove(temp_path, ec);
			error = "the renderer could not capture the frame";
			return;
		}
		// rename into place so a reader polling for the name never sees half a file
		std::filesystem::rename(temp_path, final_path, ec);
		if (ec)
		{
			std::error_code ignored;
			std::filesystem::remove(temp_path, ignored);
			error = "could not move the screenshot into place: " + ec.message();
			return;
		}
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		if (!read_png_size(final_path, width, height))
		{
			error = "the saved file is not a readable PNG";
			return;
		}
		result["name"] = file_name;
		result["bytes"] = std::filesystem::file_size(final_path, ec);
		result["width"] = width;
		result["height"] = height;
		result["overwritten"] = overwritten;
		enforce_folder_cap(final_path);
	}

	void agent_bridge::enforce_folder_cap(const std::filesystem::path& keep)
	{
		struct shot
		{
			std::filesystem::path path;
			std::filesystem::file_time_type written;
			std::uintmax_t bytes;
		};
		std::vector<shot> shots;
		std::uintmax_t total = 0;
		std::error_code ec;
		for (std::filesystem::directory_iterator it(m_screenshot_dir, ec), end; !ec && it != end; it.increment(ec))
		{
			const std::filesystem::path& path = it->path();
			std::error_code entry_ec;
			if (!it->is_regular_file(entry_ec) || path.extension() != ".png" || is_temp_name(path))
			{
				continue;
			}
			const std::uintmax_t bytes = it->file_size(entry_ec);
			if (entry_ec)
			{
				continue; // unreadable size: neither counted nor deleted
			}
			const auto written = it->last_write_time(entry_ec);
			total += bytes;
			if (path != keep && !entry_ec)
			{
				shots.push_back(shot{path, written, bytes});
			}
		}
		std::sort(shots.begin(), shots.end(), [](const shot& a, const shot& b) { return a.written < b.written; });
		for (const shot& oldest : shots)
		{
			if (total <= screenshot_folder_cap)
			{
				break;
			}
			if (std::filesystem::remove(oldest.path, ec))
			{
				total -= oldest.bytes;
			}
		}
	}

	//-------------------------------------------------------------------------
	// input (HH-0038)
	//-------------------------------------------------------------------------

	namespace
	{
		constexpr std::size_t max_jobs = 16;
		constexpr std::size_t max_text_bytes = 256;
		// CControls reads at most this many typed characters per update, so a text step carries no more
		constexpr std::size_t chars_per_step = static_cast<std::size_t>(cc::input_state::max_typed_chars);
		constexpr int max_coordinate = 16384;
		constexpr int default_job_timeout_ms = 10000;
		constexpr int default_wait_timeout_ms = 5000;
		constexpr int max_timeout_ms = 60000;
		constexpr std::size_t max_waits = 8;
		constexpr std::size_t max_wait_conditions = 16;
		constexpr int max_wheel_notches = 20;
		constexpr int wheel_delta_per_notch = 120; // as SFMLWindow converts a wheel step
		constexpr std::uint32_t text_char_message = 0x0102; // the WM_CHAR-style id SFMLWindow passes to on_text_input

		struct key_name
		{
			std::string_view name;
			KeyCode code;
		};

		// SDL's names (lowercase); single letters, digits and f1-f12 are derived in key_from_name
		constexpr std::array named_keys = {
			key_name{"up", KeyCode::Up}, key_name{"down", KeyCode::Down}, key_name{"left", KeyCode::Left},
			key_name{"right", KeyCode::Right}, key_name{"enter", KeyCode::Enter}, key_name{"escape", KeyCode::Escape},
			key_name{"backspace", KeyCode::Backspace}, key_name{"delete", KeyCode::Delete},
			key_name{"home", KeyCode::Home}, key_name{"end", KeyCode::End}, key_name{"tab", KeyCode::Tab},
			key_name{"space", KeyCode::Space}, key_name{"insert", KeyCode::Insert},
			key_name{"page_up", KeyCode::PageUp}, key_name{"page_down", KeyCode::PageDown},
			key_name{"plus", KeyCode::Equal}, key_name{"minus", KeyCode::Hyphen}, key_name{"grave", KeyCode::Grave},
			key_name{"shift", KeyCode::LShift}, key_name{"ctrl", KeyCode::LControl}, key_name{"alt", KeyCode::LAlt},
		};

		std::optional<KeyCode> key_from_name(std::string_view name)
		{
			if (name.size() == 1)
			{
				const char c = name.front();
				if (c >= 'a' && c <= 'z')
				{
					return static_cast<KeyCode>(c - 'a' + 'A');
				}
				if (c >= '0' && c <= '9')
				{
					return static_cast<KeyCode>(c);
				}
			}
			if (name.size() >= 2 && name.size() <= 3 && name.front() == 'f')
			{
				int number = 0;
				const auto [end, ec] = std::from_chars(name.data() + 1, name.data() + name.size(), number);
				if (ec == std::errc{} && end == name.data() + name.size() && number >= 1 && number <= 12)
				{
					return static_cast<KeyCode>(static_cast<int>(KeyCode::F1) + number - 1);
				}
			}
			for (const key_name& known : named_keys)
			{
				if (known.name == name)
				{
					return known.code;
				}
			}
			return std::nullopt;
		}

		// UTF-8 to code points; false on malformed or overlong input, a surrogate, or a control character
		bool decode_text(std::string_view text, std::vector<std::uint32_t>& out)
		{
			for (std::size_t i = 0; i < text.size();)
			{
				const auto lead = static_cast<unsigned char>(text[i]);
				std::uint32_t cp = 0;
				std::size_t length = 0;
				std::uint32_t minimum = 0;
				if (lead < 0x80)
				{
					cp = lead;
					length = 1;
				}
				else if ((lead & 0xE0) == 0xC0)
				{
					cp = lead & 0x1Fu;
					length = 2;
					minimum = 0x80;
				}
				else if ((lead & 0xF0) == 0xE0)
				{
					cp = lead & 0x0Fu;
					length = 3;
					minimum = 0x800;
				}
				else if ((lead & 0xF8) == 0xF0)
				{
					cp = lead & 0x07u;
					length = 4;
					minimum = 0x10000;
				}
				else
				{
					return false;
				}
				if (i + length > text.size())
				{
					return false;
				}
				for (std::size_t j = 1; j < length; ++j)
				{
					const auto next = static_cast<unsigned char>(text[i + j]);
					if ((next & 0xC0) != 0x80)
					{
						return false;
					}
					cp = (cp << 6) | (next & 0x3Fu);
				}
				if (cp < minimum || cp < 32 || cp == 127 || (cp >= 0x80 && cp < 0xA0) || (cp >= 0xD800 && cp <= 0xDFFF)
					|| cp > 0x10FFFF)
				{
					return false;
				}
				out.push_back(cp);
				i += length;
			}
			return true;
		}

		bool read_bool(const agent_bridge::json& args, const char* key, bool& out, std::string& error)
		{
			const auto found = args.find(key);
			if (found == args.end() || found->is_null())
			{
				return true;
			}
			if (!found->is_boolean())
			{
				error = std::format("'{}' must be a boolean", key);
				return false;
			}
			out = found->get<bool>();
			return true;
		}

		bool read_int(const agent_bridge::json& args, const char* key, bool required, int low, int high, int& out,
			std::string& error)
		{
			const auto found = args.find(key);
			if (found == args.end() || found->is_null())
			{
				if (required)
				{
					error = std::format("'{}' is required", key);
				}
				return !required;
			}
			// an unsigned value above INT64_MAX would wrap negative through get<int64_t>, so it is checked as unsigned
			const bool in_range = found->is_number_unsigned()
				? high >= 0 && found->get<std::uint64_t>() <= static_cast<std::uint64_t>(high)
					&& (low <= 0 || found->get<std::uint64_t>() >= static_cast<std::uint64_t>(low))
				: found->is_number_integer() && found->get<std::int64_t>() >= low && found->get<std::int64_t>() <= high;
			if (!in_range)
			{
				error = std::format("'{}' must be an integer in {}..{}", key, low, high);
				return false;
			}
			out = found->get<int>();
			return true;
		}

		bool read_button(const agent_bridge::json& args, int& button, std::string& error)
		{
			button = hb::shared::input::MouseButton::Left;
			const auto found = args.find("button");
			if (found == args.end() || found->is_null())
			{
				return true;
			}
			const std::string name = found->is_string() ? found->get<std::string>() : std::string{};
			if (name == "left")
			{
				return true;
			}
			if (name == "right")
			{
				button = hb::shared::input::MouseButton::Right;
				return true;
			}
			if (name == "middle")
			{
				button = hb::shared::input::MouseButton::Middle;
				return true;
			}
			error = "'button' must be left, right or middle";
			return false;
		}
	}

	namespace
	{
		// x and y, and whether they are window pixels ("space": "window") rather than UI units (the default)
		bool read_point(const agent_bridge::json& args, int& x, int& y, bool& window_space, std::string& error)
		{
			if (!read_int(args, "x", true, -max_coordinate, max_coordinate, x, error)
				|| !read_int(args, "y", true, -max_coordinate, max_coordinate, y, error))
			{
				return false;
			}
			const auto space = args.find("space");
			window_space = space != args.end() && *space == "window";
			if (window_space || space == args.end() || space->is_null() || *space == "ui" || *space == "logical")
			{
				return true;
			}
			error = "'space' must be ui or window";
			return false;
		}
	}

	bool agent_bridge::resolve_xy(const json& args, CGame&, int& x, int& y, std::string& error) const
	{
		bool window_space = false;
		if (!read_point(args, x, y, window_space, error))
		{
			return false;
		}
		if (window_space)
		{
			if (m_window == nullptr)
			{
				error = "no window to map window pixels through";
				return false;
			}
			m_window->window_to_ui(x, y, x, y); // the mapping real mouse input uses, clamp included
		}
		else if (m_window != nullptr)
		{
			m_window->clamp_ui(x, y); // a UI point reaches only as far as a real mouse can
		}
		return true;
	}

	bool agent_bridge::read_job_timeout(const json& args, std::chrono::milliseconds& timeout, std::string& error) const
	{
		// counted from when the job reaches the front of the queue, not from when it was queued
		int timeout_ms = default_job_timeout_ms;
		if (!read_int(args, "timeout_ms", false, 1, max_timeout_ms, timeout_ms, error))
		{
			return false;
		}
		timeout = std::chrono::milliseconds(timeout_ms);
		return true;
	}

	bool agent_bridge::enqueue(const json& id, input_job job, std::string& error)
	{
		if (m_handler == nullptr)
		{
			error = "the window has no event handler to inject into";
			return false;
		}
		if (m_jobs.size() >= max_jobs)
		{
			error = "too many input commands pending";
			return false;
		}
		job.id = id;
		m_jobs.push_back(std::move(job));
		return true;
	}

	void agent_bridge::modifier_steps(bool shift, bool ctrl, bool alt, bool press, std::vector<input_step>& steps) const
	{
		// a modifier already latched stays as the latch has it
		const step_kind kind = press ? step_kind::key_down : step_kind::key_up;
		if (shift && !m_latch_shift)
		{
			steps.push_back(input_step{.kind = kind, .key = KeyCode::LShift});
		}
		if (ctrl && !m_latch_ctrl)
		{
			steps.push_back(input_step{.kind = kind, .key = KeyCode::LControl});
		}
		if (alt && !m_latch_alt)
		{
			steps.push_back(input_step{.kind = kind, .key = KeyCode::LAlt});
		}
	}

	void agent_bridge::advance_input()
	{
		if (m_jobs.empty() || m_handler == nullptr)
		{
			return;
		}
		input_job& job = m_jobs.front();
		if (!job.started)
		{
			job.started = true;
			job.deadline = clock::now() + job.timeout;
		}
		if (clock::now() >= job.deadline)
		{
			const bool transitioning = GameModeManager::is_transitioning();
			const json id = job.id;
			m_jobs.pop_front();
			release_held();
			queue_reply(id, {}, transitioning ? "input timed out waiting for a screen transition to end"
				: "input timed out waiting for presented frames");
			return;
		}
		// a real mouse over the window moves the cursor too; re-assert the job's position every update
		if (job.pointer && job.next > 0)
		{
			m_handler->on_mouse_move(job.pointer_x, job.pointer_y);
		}
		if (job.next >= job.steps.size() || !m_frame_since_step)
		{
			return;
		}
		const input_step& step = job.steps[job.next];
		// input is suppressed while a screen fades: hold presses until it ends, but never hold a release
		const bool release = step.kind == step_kind::button_up || step.kind == step_kind::key_up;
		if (GameModeManager::is_transitioning() && !release)
		{
			return;
		}
		execute(step);
		if (step.kind == step_kind::move)
		{
			job.pointer_x = step.x;
			job.pointer_y = step.y;
		}
		++job.next;
		m_frame_since_step = false;
	}

	void agent_bridge::advance_on_frame()
	{
		m_frame_since_step = true;
		if (m_jobs.empty() || m_jobs.front().next < m_jobs.front().steps.size())
		{
			return;
		}
		// the frame after the last step is on screen, so a follow-up get_state or screenshot sees the effect
		const input_job done = std::move(m_jobs.front());
		m_jobs.pop_front();
		queue_reply(done.id, done.result, {});
		flush_outbound();
	}

	void agent_bridge::execute(const input_step& step)
	{
		switch (step.kind)
		{
		case step_kind::move:
			m_handler->on_mouse_move(step.x, step.y);
			m_last_x = step.x;
			m_last_y = step.y;
			break;
		case step_kind::button_down:
			m_handler->on_mouse_button_down(step.button, step.x, step.y);
			m_held_buttons.insert(step.button);
			m_last_x = step.x;
			m_last_y = step.y;
			break;
		case step_kind::button_up:
			m_handler->on_mouse_button_up(step.button, step.x, step.y);
			m_held_buttons.erase(step.button);
			break;
		case step_kind::wheel:
			m_handler->on_mouse_wheel(step.delta, step.x, step.y);
			break;
		case step_kind::key_down:
			m_handler->on_key_down(step.key);
			m_held_keys.insert(step.key);
			break;
		case step_kind::key_up:
			m_handler->on_key_up(step.key);
			m_held_keys.erase(step.key);
			break;
		case step_kind::chars:
			for (const std::uint32_t cp : step.chars)
			{
				m_handler->on_text_input(m_window->get_handle(), text_char_message, cp, 0);
			}
			break;
		}
	}

	void agent_bridge::release_held()
	{
		if (m_handler != nullptr)
		{
			for (const int button : m_held_buttons)
			{
				m_handler->on_mouse_button_up(button, m_last_x, m_last_y);
			}
			for (const KeyCode key : m_held_keys)
			{
				m_handler->on_key_up(key);
			}
		}
		m_held_buttons.clear();
		m_held_keys.clear();
		m_latch_shift = false;
		m_latch_ctrl = false;
		m_latch_alt = false;
	}

	bool agent_bridge::run_probe_point(const json&, const json& args, CGame& active_game, json& result,
		std::string& error)
	{
		int x = 0;
		int y = 0;
		bool from_window = false;
		if (!read_point(args, x, y, from_window, error))
		{
			return true;
		}
		if (m_window == nullptr)
		{
			error = "no window";
			return true;
		}
		int ui_x = x;
		int ui_y = y;
		int window_x = x;
		int window_y = y;
		if (from_window)
		{
			m_window->window_to_ui(x, y, ui_x, ui_y);
		}
		else
		{
			m_window->ui_to_window(x, y, window_x, window_y);
		}
		result["logical"] = {ui_x, ui_y};
		result["window"] = {window_x, window_y};
		result["window_size"] = {m_window->get_width(), m_window->get_height()};
		if (active_game.m_Renderer != nullptr)
		{
			const hb::shared::geometry::GamePoint world = active_game.m_Renderer->ui_to_world({ui_x, ui_y});
			result["world"] = {world.x, world.y};
		}
		return true;
	}

	bool agent_bridge::run_mouse_move(const json& id, const json& args, CGame& active_game, json&, std::string& error)
	{
		input_job job;
		int x = 0;
		int y = 0;
		if (!resolve_xy(args, active_game, x, y, error) || !read_job_timeout(args, job.timeout, error))
		{
			return true;
		}
		job.steps.push_back(input_step{.kind = step_kind::move, .x = x, .y = y});
		job.pointer = true;
		job.pointer_x = x;
		job.pointer_y = y;
		job.result = {{"x", x}, {"y", y}};
		return !enqueue(id, std::move(job), error);
	}

	bool agent_bridge::run_click(const json& id, const json& args, CGame& active_game, json&, std::string& error)
	{
		input_job job;
		int x = 0;
		int y = 0;
		int button = 0;
		int count = 1;
		bool shift = false;
		bool ctrl = false;
		if (!resolve_xy(args, active_game, x, y, error) || !read_button(args, button, error)
			|| !read_int(args, "count", false, 1, 3, count, error) || !read_bool(args, "shift", shift, error)
			|| !read_bool(args, "ctrl", ctrl, error) || !read_job_timeout(args, job.timeout, error))
		{
			return true;
		}
		modifier_steps(shift, ctrl, false, true, job.steps);
		job.steps.push_back(input_step{.kind = step_kind::move, .x = x, .y = y});
		for (int i = 0; i < count; ++i)
		{
			job.steps.push_back(input_step{.kind = step_kind::button_down, .x = x, .y = y, .button = button});
			job.steps.push_back(input_step{.kind = step_kind::button_up, .x = x, .y = y, .button = button});
		}
		modifier_steps(shift, ctrl, false, false, job.steps);
		job.pointer = true;
		job.pointer_x = x;
		job.pointer_y = y;
		job.result = {{"x", x}, {"y", y}, {"button", button}, {"count", count}};
		return !enqueue(id, std::move(job), error);
	}

	bool agent_bridge::run_mouse_down(const json& id, const json& args, CGame& active_game, json&, std::string& error)
	{
		input_job job;
		int x = 0;
		int y = 0;
		int button = 0;
		if (!resolve_xy(args, active_game, x, y, error) || !read_button(args, button, error)
			|| !read_job_timeout(args, job.timeout, error))
		{
			return true;
		}
		job.steps.push_back(input_step{.kind = step_kind::move, .x = x, .y = y});
		job.steps.push_back(input_step{.kind = step_kind::button_down, .x = x, .y = y, .button = button});
		job.pointer = true;
		job.pointer_x = x;
		job.pointer_y = y;
		job.result = {{"x", x}, {"y", y}, {"button", button}};
		return !enqueue(id, std::move(job), error);
	}

	bool agent_bridge::run_mouse_up(const json& id, const json& args, CGame& active_game, json&, std::string& error)
	{
		input_job job;
		int x = 0;
		int y = 0;
		int button = 0;
		if (!resolve_xy(args, active_game, x, y, error) || !read_button(args, button, error)
			|| !read_job_timeout(args, job.timeout, error))
		{
			return true;
		}
		// a release at a new point ends a drag there
		job.steps.push_back(input_step{.kind = step_kind::move, .x = x, .y = y});
		job.steps.push_back(input_step{.kind = step_kind::button_up, .x = x, .y = y, .button = button});
		job.pointer = true;
		job.pointer_x = x;
		job.pointer_y = y;
		job.result = {{"x", x}, {"y", y}, {"button", button}};
		return !enqueue(id, std::move(job), error);
	}

	bool agent_bridge::run_wheel(const json& id, const json& args, CGame& active_game, json&, std::string& error)
	{
		input_job job;
		int x = 0;
		int y = 0;
		int notches = 0;
		if (!resolve_xy(args, active_game, x, y, error)
			|| !read_int(args, "notches", true, -max_wheel_notches, max_wheel_notches, notches, error)
			|| !read_job_timeout(args, job.timeout, error))
		{
			return true;
		}
		job.steps.push_back(input_step{.kind = step_kind::move, .x = x, .y = y});
		job.steps.push_back(input_step{.kind = step_kind::wheel, .x = x, .y = y, .delta = notches * wheel_delta_per_notch});
		job.pointer = true;
		job.pointer_x = x;
		job.pointer_y = y;
		job.result = {{"x", x}, {"y", y}, {"notches", notches}};
		return !enqueue(id, std::move(job), error);
	}

	bool agent_bridge::run_key(const json& id, const json& args, CGame&, json&, std::string& error)
	{
		input_job job;
		bool shift = false;
		bool ctrl = false;
		bool alt = false;
		bool repeat = false;
		const auto name = args.find("name");
		if (name == args.end() || !name->is_string())
		{
			error = "key requires a string 'name'";
			return true;
		}
		const std::optional<KeyCode> code = key_from_name(name->get_ref<const std::string&>());
		if (!code)
		{
			error = "unknown key name";
			return true;
		}
		if (!read_bool(args, "shift", shift, error) || !read_bool(args, "ctrl", ctrl, error)
			|| !read_bool(args, "alt", alt, error) || !read_bool(args, "repeat", repeat, error)
			|| !read_job_timeout(args, job.timeout, error))
		{
			return true;
		}
		if (repeat)
		{
			error = "'repeat' is not supported; hold a key with the modifiers latch or send presses";
			return true;
		}
		modifier_steps(shift, ctrl, alt, true, job.steps);
		job.steps.push_back(input_step{.kind = step_kind::key_down, .key = *code});
		job.steps.push_back(input_step{.kind = step_kind::key_up, .key = *code});
		modifier_steps(shift, ctrl, alt, false, job.steps);
		job.result = {{"name", *name}};
		return !enqueue(id, std::move(job), error);
	}

	bool agent_bridge::run_text(const json& id, const json& args, CGame&, json&, std::string& error)
	{
		// the value is never echoed or logged: it may be going into a password field
		input_job job;
		const auto value = args.find("value");
		std::vector<std::uint32_t> chars;
		if (value == args.end() || !value->is_string())
		{
			error = "text requires a string 'value'";
			return true;
		}
		const std::string& text = value->get_ref<const std::string&>();
		if (text.size() > max_text_bytes || !decode_text(text, chars))
		{
			error = std::format("'value' must be valid UTF-8 of at most {} bytes with no control characters", max_text_bytes);
			return true;
		}
		if (!read_job_timeout(args, job.timeout, error))
		{
			return true;
		}
		for (std::size_t at = 0; at < chars.size(); at += chars_per_step)
		{
			const std::size_t end = std::min(chars.size(), at + chars_per_step);
			job.steps.push_back(input_step{.kind = step_kind::chars, .chars = {chars.begin() + at, chars.begin() + end}});
		}
		job.result = {{"chars", chars.size()}};
		return !enqueue(id, std::move(job), error);
	}

	bool agent_bridge::run_set_text(const json& id, const json& args, CGame& active_game, json& result,
		std::string& error)
	{
		// select all (ctrl+a) and delete it, then type: the same keys a player would use on a CControls field
		bool clear = true;
		if (!read_bool(args, "clear", clear, error))
		{
			return true;
		}
		std::vector<input_step> clearing;
		if (clear)
		{
			modifier_steps(false, true, false, true, clearing);
			clearing.push_back(input_step{.kind = step_kind::key_down, .key = KeyCode::A});
			clearing.push_back(input_step{.kind = step_kind::key_up, .key = KeyCode::A});
			modifier_steps(false, true, false, false, clearing);
			clearing.push_back(input_step{.kind = step_kind::key_down, .key = KeyCode::Backspace});
			clearing.push_back(input_step{.kind = step_kind::key_up, .key = KeyCode::Backspace});
		}
		if (!run_text(id, args, active_game, result, error))
		{
			// queued by run_text: put the clearing steps in front of its typing
			input_job& job = m_jobs.back();
			job.steps.insert(job.steps.begin(), clearing.begin(), clearing.end());
			job.result["cleared"] = clear;
			return false;
		}
		return true;
	}

	bool agent_bridge::run_modifiers(const json& id, const json& args, CGame&, json& result, std::string& error)
	{
		bool shift = m_latch_shift;
		bool ctrl = m_latch_ctrl;
		bool alt = m_latch_alt;
		input_job job;
		if (!read_bool(args, "shift", shift, error) || !read_bool(args, "ctrl", ctrl, error)
			|| !read_bool(args, "alt", alt, error) || !read_job_timeout(args, job.timeout, error))
		{
			return true;
		}
		// a latched modifier is a held key: press or release only what changes
		const auto change = [&job](bool from, bool to, KeyCode key)
		{
			if (from != to)
			{
				job.steps.push_back(input_step{.kind = to ? step_kind::key_down : step_kind::key_up, .key = key});
			}
		};
		change(m_latch_shift, shift, KeyCode::LShift);
		change(m_latch_ctrl, ctrl, KeyCode::LControl);
		change(m_latch_alt, alt, KeyCode::LAlt);
		job.result = {{"shift", shift}, {"ctrl", ctrl}, {"alt", alt}};
		if (job.steps.empty())
		{
			result = job.result;
			return true;
		}
		if (!enqueue(id, std::move(job), error))
		{
			return true;
		}
		m_latch_shift = shift;
		m_latch_ctrl = ctrl;
		m_latch_alt = alt;
		return false;
	}

	bool agent_bridge::run_wait_for(const json& id, const json& args, CGame&, json&, std::string& error)
	{
		const auto conditions = args.find("conditions");
		if (conditions == args.end() || !conditions->is_array() || conditions->empty()
			|| conditions->size() > max_wait_conditions)
		{
			error = std::format("wait_for requires 'conditions', an array of 1..{} state conditions", max_wait_conditions);
			return true;
		}
		for (const json& condition : *conditions)
		{
			if (!condition.is_object() || !condition.contains("state") || !condition["state"].is_string()
				|| !condition.contains("equals"))
			{
				error = "each condition must be {\"state\": key, \"equals\": value} (event conditions come in phase 3)";
				return true;
			}
		}
		int timeout_ms = default_wait_timeout_ms;
		if (!read_int(args, "timeout_ms", false, 1, max_timeout_ms, timeout_ms, error))
		{
			return true;
		}
		const auto waits = std::count_if(m_deferred.begin(), m_deferred.end(),
			[](const deferred_reply& pending) { return pending.kind == deferred_kind::wait; });
		if (static_cast<std::size_t>(waits) >= max_waits)
		{
			error = "too many waits pending";
			return true;
		}
		m_deferred.push_back(deferred_reply{.id = id, .kind = deferred_kind::wait, .conditions = *conditions,
			.deadline = clock::now() + std::chrono::milliseconds(timeout_ms)});
		return false; // settle_waits answers it, in this poll if it already holds
	}

	void agent_bridge::settle_waits(CGame& active_game)
	{
		const bool any = std::any_of(m_deferred.begin(), m_deferred.end(),
			[](const deferred_reply& pending) { return pending.kind == deferred_kind::wait; });
		if (!any)
		{
			return;
		}
		const json state = state_snapshot(active_game);
		const clock::time_point now = clock::now();
		std::vector<std::pair<json, json>> settled;
		std::erase_if(m_deferred, [&](const deferred_reply& pending)
		{
			if (pending.kind != deferred_kind::wait)
			{
				return false;
			}
			json result = json::object();
			for (std::size_t i = 0; i < pending.conditions.size(); ++i)
			{
				const json& condition = pending.conditions[i];
				const auto found = state.find(condition["state"].get_ref<const std::string&>());
				if (found != state.end() && *found == condition["equals"])
				{
					result["matched"] = {{"index", i}, {"condition", condition}, {"observed", *found}};
					break;
				}
			}
			const bool satisfied = result.contains("matched");
			if (!satisfied && now < pending.deadline)
			{
				return false;
			}
			// a timeout is an ordinary outcome: ok, satisfied false, with the state as it was
			result["satisfied"] = satisfied;
			result["state"] = state;
			settled.emplace_back(pending.id, std::move(result));
			return true;
		});
		for (const auto& [id, result] : settled)
		{
			queue_reply(id, result, {});
		}
	}
}

#endif
