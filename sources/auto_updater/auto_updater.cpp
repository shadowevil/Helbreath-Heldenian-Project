#include "auto_updater.h"
#include "updater_constants.h"
#include "updater_manifest.h"
#include "updater_http.h"
#include "updater_sha256.h"
#include "updater_file_ops.h"
#include "updater_gui.h"
#include <filesystem>
#include <string>
#include <vector>
#include <cstdio>

namespace fs = std::filesystem;

namespace hb::updater
{
	update_result check_for_updates()
	{
		std::string exe_path = get_exe_path();
		std::string base_dir = get_exe_directory();

		// Phase 0 — pending exe swap from last run
		if (apply_pending_exe_swap(exe_path))
		{
			cleanup_old_exe(exe_path);
			relaunch(); // does not return
		}

		// Clean up .old if left over
		cleanup_old_exe(exe_path);

		// Phase 1 — fetch manifest
		std::string manifest_json;
		while (!http_get_text(update_server_host, update_server_port, manifest_path, manifest_json))
		{
			if (!show_server_unreachable_dialog())
				return update_result::server_unreachable;
		}

		update_manifest manifest;
		if (!parse_manifest(manifest_json, manifest))
			return update_result::error;

		// Phase 2 — compare local files vs manifest
		struct changed_file
		{
			const manifest_entry* entry;
		};
		std::vector<changed_file> changed_files;

		for (const auto& entry : manifest.files)
		{
#ifdef _WIN32
			if (entry.platform == "linux") continue;
#else
			if (entry.platform == "windows") continue;
#endif

			std::string local_path = (fs::path(base_dir) / entry.path).string();
			std::string local_hash = sha256_file(local_path.c_str());

			if (local_hash != entry.sha256)
				changed_files.push_back({&entry});
		}

		if (changed_files.empty())
			return update_result::no_update;

		// Phase 3 — download changed files to staging
		updater_gui gui;
		gui.create();

		if (!ensure_staging_dir(base_dir))
		{
			gui.destroy();
			return update_result::error;
		}

		std::string staging_base = (fs::path(base_dir) / staging_dir).string();
		int total = static_cast<int>(changed_files.size());

		for (int i = 0; i < total; ++i)
		{
			const auto& entry = *changed_files[i].entry;
			std::string staged_path = (fs::path(staging_base) / entry.path).string();

			// Resume: skip files already staged with correct hash
			std::string staged_hash = sha256_file(staged_path.c_str());
			if (staged_hash == entry.sha256)
			{
				gui.set_progress(static_cast<float>(i + 1) / static_cast<float>(total));
				gui.pump_messages();
				continue;
			}

			char status_buf[256];
			std::snprintf(status_buf, sizeof(status_buf),
				"Downloading %s (%d of %d)...", entry.path.c_str(), i + 1, total);
			gui.set_status(status_buf);
			gui.set_progress(static_cast<float>(i) / static_cast<float>(total));
			gui.pump_messages();

			if (gui.is_cancelled())
			{
				gui.destroy();
				return update_result::error;
			}

			std::string url = "/" + entry.path;

			if (!http_download_file(update_server_host, update_server_port,
				url.c_str(), staged_path))
			{
				gui.destroy();
				return update_result::error;
			}
		}

		// Phase 4 — verify staged files
		gui.set_status("Verifying files...");
		gui.set_progress(1.0f);
		gui.pump_messages();

		for (const auto& cf : changed_files)
		{
			const auto& entry = *cf.entry;
			std::string staged_path = (fs::path(staging_base) / entry.path).string();
			std::string hash = sha256_file(staged_path.c_str());

			if (hash != entry.sha256)
			{
				// Remove corrupt file so next run re-downloads it
				std::error_code ec;
				fs::remove(staged_path, ec);
				gui.destroy();
				return update_result::error;
			}
		}

		// Phase 5 — disperse files
		bool has_exe_update = false;
		gui.set_status("Applying update...");
		gui.pump_messages();

		for (const auto& cf : changed_files)
		{
			const auto& entry = *cf.entry;
			std::string staged_path = (fs::path(staging_base) / entry.path).string();
			std::string final_path = (fs::path(base_dir) / entry.path).string();

			if (!disperse_file(staged_path, final_path, entry.is_executable))
			{
				cleanup_staging(base_dir);
				gui.destroy();
				return update_result::error;
			}

			if (entry.is_executable)
				has_exe_update = true;
		}

		cleanup_staging(base_dir);
		gui.destroy();

		// Phase 6
		if (has_exe_update)
			return update_result::restart_required;

		return update_result::updated;
	}
}
