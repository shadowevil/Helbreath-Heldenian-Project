#include "updater_http.h"
#include "updater_constants.h"
#include "updater_platform.h"

#include "vendor/httplib.h"
#include <filesystem>
#include <fstream>

namespace hb::updater
{
	bool http_get_text(const char* host, int port, const char* path, std::string& response_body)
	{
		httplib::Client client(host, port);
		client.set_connection_timeout(connect_timeout);
		client.set_read_timeout(read_timeout);

		auto result = client.Get(path);
		if (!result || result->status != 200)
			return false;

		response_body = std::move(result->body);
		return true;
	}

	bool http_download_file(const char* host, int port, const char* url_path,
		const std::string& local_path)
	{
		// Ensure parent directory exists
		auto parent = std::filesystem::path(local_path).parent_path();
		if (!parent.empty())
			std::filesystem::create_directories(parent);

		httplib::Client client(host, port);
		client.set_connection_timeout(connect_timeout);
		client.set_read_timeout(read_timeout);

		auto result = client.Get(url_path);
		if (!result || result->status != 200)
			return false;

		std::ofstream ofs(local_path, std::ios::binary);
		if (!ofs.is_open())
			return false;

		ofs.write(result->body.data(), static_cast<std::streamsize>(result->body.size()));
		return ofs.good();
	}
}
