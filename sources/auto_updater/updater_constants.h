#pragma once

namespace hb::updater
{
	// Server configuration
	constexpr const char* update_server_host = "199.187.160.239";
	constexpr int update_server_port = 8080;
	constexpr const char* manifest_path = "/update.manifest.json";

	// Timeouts (seconds)
	constexpr int connect_timeout = 10;
	constexpr int read_timeout = 30;

	// Paths (relative to exe directory)
	constexpr const char* staging_dir = "updates";
	constexpr const char* exe_update_suffix = ".update";
	constexpr const char* exe_old_suffix = ".old";

	// GUI
	constexpr int window_width = 420;
	constexpr int window_height = 140;
	constexpr const char* window_title = "Helbreath Updater";
}
