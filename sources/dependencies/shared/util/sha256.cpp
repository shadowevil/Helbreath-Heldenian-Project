#include "sha256.h"
#include "vendor/picosha2.h"
#include <fstream>

namespace hb::shared::util {

std::string sha256(const void* data, std::size_t length)
{
	const auto* bytes = static_cast<const unsigned char*>(data);
	return picosha2::hash256_hex_string(bytes, bytes + length);
}

std::string sha256_file(const char* file_path)
{
	std::ifstream ifs(file_path, std::ios::binary);
	if (!ifs.is_open())
		return {};

	return picosha2::hash256_hex_string(
		std::istreambuf_iterator<char>(ifs),
		std::istreambuf_iterator<char>());
}

} // namespace hb::shared::util
