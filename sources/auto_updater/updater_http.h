#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace hb::updater
{
	// HTTP GET a text resource. Returns true on success (HTTP 200).
	// response_body receives the full body on success.
	bool http_get_text(const char* host, int port, const char* path, std::string& response_body);

	// HTTP GET a binary file and write it to disk. Returns true on success.
	// Creates parent directories as needed.
	bool http_download_file(const char* host, int port, const char* url_path,
		const std::string& local_path);
}
