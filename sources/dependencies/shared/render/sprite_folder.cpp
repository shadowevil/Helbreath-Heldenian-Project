// sprite_folder.cpp: Reads extracted sprite folders (see sprite_folder.h)
//
//////////////////////////////////////////////////////////////////////

#include "sprite_folder.h"
#include "json.hpp"
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace hb::shared::sprite {

namespace {

using json = nlohmann::json;

// Largest accepted sprite art_scale (image pixels per logical pixel)
constexpr int64_t max_art_scale = 16;

struct index_entry
{
	std::string dir;
	std::string manifest;
	int sprite_count = 0;
};

struct sprite_index
{
	std::unordered_map<std::string, index_entry> entries;
	bool loaded = false;
};

// A relative path that stays below the folder it is joined to: no "..", no leading separator, no
// backslash and no drive or stream colon. Data failing this is treated as missing.
bool is_safe_relative(std::string_view path)
{
	return !path.empty() && path.front() != '/' && path.find("..") == std::string_view::npos
		&& path.find('\\') == std::string_view::npos && path.find(':') == std::string_view::npos;
}

// <root>/index.json, read once per root. A missing or malformed index stays empty (loaded false), so
// every lookup falls back to <root>/<name>/manifest.json; entries with unsafe paths are dropped.
const sprite_index& index_for(const std::string& sprite_root)
{
	static std::mutex mutex;
	static std::unordered_map<std::string, sprite_index> cache;

	std::lock_guard<std::mutex> lock(mutex);
	auto [it, inserted] = cache.try_emplace(sprite_root);
	if (!inserted)
		return it->second;

	sprite_index& index = it->second;
	try
	{
		const json root = json::parse(read_file_bytes(sprite_root + "/index.json"), nullptr, false);
		const auto assets = root.is_object() ? root.find("assets") : root.end();
		if (assets != root.end() && assets->is_object())
		{
			for (const auto& [name, entry] : assets->items())
			{
				index_entry value{
					entry.value("dir", name),
					entry.value("manifest", name + "/manifest.json"),
					entry.value("sprite_count", 0)};
				if (is_safe_relative(name) && is_safe_relative(value.dir) && is_safe_relative(value.manifest))
					index.entries.emplace(name, std::move(value));
			}
			index.loaded = true;
		}
	}
	catch (const json::exception&)
	{
		index.entries.clear();
		index.loaded = false;
	}
	return index;
}

const index_entry* find_entry(const std::string& sprite_root, const std::string& name)
{
	const sprite_index& index = index_for(sprite_root);
	const auto it = index.entries.find(name);
	return it != index.entries.end() ? &it->second : nullptr;
}

// Streams a manifest straight into folder_sprite records, building no JSON tree: only sprites[].index,
// sprites[].art_scale, sprites[].frames and sprites[].image.file are read, every other value is skipped.
// Over all manifests (20.9 MB) in a Debug build a filtered DOM parse took 7.3 s, this reader 3.7 s
// (measured, HH-0056).
class manifest_reader : public nlohmann::json_sax<json>
{
public:
	manifest_reader(std::vector<folder_sprite>& sprites, const std::string& dir)
		: m_sprites(sprites)
		, m_dir(dir)
	{
	}

	const std::string& error() const { return m_error; }
	bool has_sprite_list() const { return m_have_sprites; }

	bool start_object(std::size_t) override { return push(true); }
	bool start_array(std::size_t) override { return push(false); }
	bool end_object() override { return pop(); }
	bool end_array() override { return pop(); }

	bool key(string_t& key) override
	{
		m_key = key;
		return true;
	}

	bool number_integer(number_integer_t value) override { return scalar() && number(value); }
	bool number_unsigned(number_unsigned_t value) override { return scalar() && number(static_cast<int64_t>(value)); }
	bool number_float(number_float_t, const string_t&) override { return scalar() && (in_record() ? fail("non-integer number") : true); }

	bool string(string_t& value) override
	{
		if (!scalar())
			return false;
		// An unsafe file name leaves the sprite without an image (drawn as empty)
		if (current() == scope::image && m_key == "file" && is_safe_relative(value))
			m_sprites.back().image_path = m_dir + "/" + value;
		return true;
	}

	bool null() override { return scalar(); }
	bool boolean(bool) override { return scalar(); }
	bool binary(binary_t&) override { return scalar(); }

	bool parse_error(std::size_t position, const std::string&, const nlohmann::detail::exception& ex) override
	{
		return fail("parse error at byte " + std::to_string(position) + ": " + ex.what());
	}

private:
	enum class scope { root, sprites, sprite, frames, frame, image, skip };

	scope current() const { return m_stack.empty() ? scope::skip : m_stack.back(); }

	// Inside a sprite record, where an unexpected value is a malformed manifest rather than skipped data
	bool in_record() const
	{
		return current() == scope::frame || (current() == scope::sprite && (m_key == "index" || m_key == "art_scale"));
	}

	// A scalar is never valid as a sprites or frames element, nor as the frames or image value itself
	bool scalar()
	{
		if (current() == scope::sprites)
			return fail("sprite is not an object");
		if (current() == scope::frames)
			return fail("frame is not an array");
		if (current() == scope::sprite && (m_key == "frames" || m_key == "image"))
			return fail("\"" + m_key + "\" has the wrong type");
		return true;
	}

	bool push(bool is_object)
	{
		scope next = scope::skip;
		if (m_stack.empty())
			next = is_object ? scope::root : scope::skip;
		else if (current() == scope::root && !is_object && m_key == "sprites")
		{
			next = scope::sprites;
			m_have_sprites = true;
		}
		else if (current() == scope::sprites)
		{
			if (!is_object)
				return fail("sprite is not an object");
			next = scope::sprite;
			m_sprites.emplace_back();
			m_have_index = false;
			m_have_frames = false;
		}
		else if (current() == scope::sprite && m_key == "frames")
		{
			if (is_object)
				return fail("\"frames\" is not an array");
			if (m_have_frames)
				return fail("duplicate \"frames\"");
			next = scope::frames;
			m_have_frames = true;
		}
		else if (current() == scope::frames)
		{
			if (is_object)
				return fail("frame is not an array");
			next = scope::frame;
			m_values.clear();
		}
		else if (current() == scope::sprite && m_key == "image")
		{
			if (!is_object)
				return fail("\"image\" is not an object");
			next = scope::image;
		}
		m_stack.push_back(next);
		return true;
	}

	bool pop()
	{
		const scope done = current();
		m_stack.pop_back();
		if (done == scope::frame)
		{
			if (m_values.size() != 6)
				return fail("frame is not [x, y, w, h, pivot_x, pivot_y]");
			m_sprites.back().frames.push_back(SpriteRect{
				static_cast<uint16_t>(m_values[0]), static_cast<uint16_t>(m_values[1]),
				static_cast<uint16_t>(m_values[2]), static_cast<uint16_t>(m_values[3]),
				static_cast<int16_t>(m_values[4]), static_cast<int16_t>(m_values[5])});
		}
		else if (done == scope::sprite && !m_have_index)
			return fail("sprite has no index");
		return true;
	}

	bool number(int64_t value)
	{
		if (current() == scope::frame)
			m_values.push_back(value);
		else if (current() == scope::sprite && m_key == "index")
		{
			// Sprite indices are positional, so the list must be in index order with no gaps
			if (value != static_cast<int64_t>(m_sprites.size()) - 1)
				return fail("sprite list out of index order");
			m_have_index = true;
		}
		else if (current() == scope::sprite && m_key == "art_scale")
		{
			if (value < 1 || value > max_art_scale)
				return fail("art_scale out of range");
			m_sprites.back().art_scale = static_cast<int>(value);
		}
		return true;
	}

	bool fail(std::string message)
	{
		if (m_error.empty())
			m_error = std::move(message);
		return false;
	}

	std::vector<folder_sprite>& m_sprites;
	const std::string& m_dir;
	std::vector<scope> m_stack;
	std::vector<int64_t> m_values;
	std::string m_key;
	std::string m_error;
	bool m_have_index = false;
	bool m_have_frames = false;
	bool m_have_sprites = false;
};

} // namespace

std::vector<uint8_t> read_file_bytes(const std::string& path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file)
		return {};
	const std::streamoff size = file.tellg();
	if (size <= 0)
		return {};
	std::vector<uint8_t> bytes(static_cast<size_t>(size));
	file.seekg(0);
	if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
		return {};
	return bytes;
}

std::vector<folder_sprite> load_sprite_folder(const std::string& sprite_root, const std::string& name)
{
	if (!is_safe_relative(name))
		throw std::runtime_error("Invalid sprite asset name: " + name);

	const index_entry* entry = find_entry(sprite_root, name);
	const std::string dir = sprite_root + "/" + (entry ? entry->dir : name);
	const std::string manifest_path = entry ? sprite_root + "/" + entry->manifest : dir + "/manifest.json";

	const std::vector<uint8_t> bytes = read_file_bytes(manifest_path);
	if (bytes.empty())
		throw std::runtime_error("Failed to open sprite manifest: " + manifest_path);

	std::vector<folder_sprite> sprites;
	manifest_reader reader(sprites, dir);
	const bool parsed = json::sax_parse(bytes, &reader);
	if (!parsed || !reader.has_sprite_list())
	{
		const std::string why = reader.error().empty() ? "no sprite list" : reader.error();
		throw std::runtime_error("Malformed sprite manifest " + manifest_path + ": " + why);
	}
	return sprites;
}

int sprite_folder_count(const std::string& sprite_root, const std::string& name)
{
	const index_entry* entry = find_entry(sprite_root, name);
	if (entry && entry->sprite_count > 0)
		return entry->sprite_count;
	try
	{
		return static_cast<int>(load_sprite_folder(sprite_root, name).size());
	}
	catch (const std::exception&)
	{
		return 0;
	}
}

bool sprite_index_loaded(const std::string& sprite_root)
{
	return index_for(sprite_root).loaded;
}

} // namespace hb::shared::sprite
