#include "updater_manifest.h"
#include "json.hpp"

namespace hb::updater
{
	bool parse_manifest(const std::string& json_text, update_manifest& out)
	{
		try
		{
			auto doc = nlohmann::json::parse(json_text);

			if (doc.contains("version"))
			{
				auto& ver = doc["version"];
				out.version_major = ver.value("major", 0);
				out.version_minor = ver.value("minor", 0);
				out.version_patch = ver.value("patch", 0);
			}

			out.files.clear();
			if (doc.contains("files") && doc["files"].is_array())
			{
				for (auto& entry : doc["files"])
				{
					manifest_entry e;
					e.path = entry.value("path", "");
					e.sha256 = entry.value("sha256", "");
					e.platform = entry.value("platform", "any");
					e.size = entry.value("size", uint64_t{0});
					e.is_executable = entry.value("executable", false);

					if (!e.path.empty() && !e.sha256.empty())
						out.files.push_back(std::move(e));
				}
			}

			return true;
		}
		catch (const nlohmann::json::exception&)
		{
			return false;
		}
	}
}
