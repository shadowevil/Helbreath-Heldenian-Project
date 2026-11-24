// agent_bridge.cpp: the debug-only agent command channel (HH-0037). See agent_bridge.h for the wire format.
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
#include <algorithm>
#include <array>
#include <exception>
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

	// The three entry points from the frame loop: anything that still throws disables the bridge, never the game
	bool agent_bridge::start(std::uint16_t port)
	{
		try
		{
			return open_listener(port);
		}
		catch (const std::exception& failure)
		{
			disable("start", failure.what());
			return false;
		}
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
	}

	void agent_bridge::disable(std::string_view where, std::string_view what)
	{
		hb::logger::warn("agent bridge: {} failed ({}) - bridge disabled", where, what);
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
	}

	void agent_bridge::expire_deferred()
	{
		const clock::time_point now = clock::now();
		std::vector<json> expired;
		std::erase_if(m_deferred, [&](const deferred_reply& pending)
		{
			if (pending.deadline > now)
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
		// read-only; flat keys, because the later wait_for reads top-level keys only
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
		return true;
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
		m_deferred.push_back(deferred_reply{id, deferred_kind::screenshot, file_name, clock::now() + screenshot_deadline});
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
}

#endif
