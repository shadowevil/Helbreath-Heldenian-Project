#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace PAKLib {
	// Only pack POD structs that are read directly from binary files
	// DO NOT pack structs containing std::string, std::vector, or other complex types
	// as they require proper alignment (critical for WebAssembly with -sSAFE_HEAP=1)
#pragma pack(push, 1)
	struct sprite_rect {
		uint16_t x;
		uint16_t y;
		uint16_t width;
		uint16_t height;
		int16_t pivotX;
		int16_t pivotY;
	};

	constexpr size_t file_header_size = 17;
	constexpr char file_header_signature[file_header_size + 1] = "<Pak file header>";
	struct file_header {
		char signature[file_header_size];
		uint8_t padding[3]{ 0 };
	};

	constexpr size_t sprite_header_size = 20;
	constexpr char sprite_header_signature[sprite_header_size + 1] = "<Sprite File Header>";
	struct sprite_header {
		char signature[sprite_header_size];
		uint8_t padding[80]{ 0 };
	};

	struct sprite_entry {
		uint32_t offset{ 0 };
		uint32_t size{ 0 };
	};

	constexpr size_t SPRITE_HEADER_SKIP = sizeof(sprite_header);
	constexpr size_t RECT_COUNT_SIZE = sizeof(uint32_t);
	constexpr size_t PADDING_SIZE = sizeof(uint32_t);
#pragma pack(pop)

	// Structs with std::vector and std::string must use normal alignment
	// alignas ensures proper alignment for WebAssembly (-sSAFE_HEAP=1)
	struct alignas(8) sprite {
	public:
		sprite_header header{};
		std::vector<sprite_rect> sprite_rectangles{};
		std::vector<uint8_t> image_data; // Raw image data (e.g., PNG, JPEG)
	};

	struct alignas(8) pak {
	public:
		std::string pak_file_path{};
		file_header header{};
		uint32_t sprite_count{ 0 };
		std::vector<sprite_entry> sprite_entries;
		std::vector<sprite> sprites{};
	};

	inline static pak loadpak(const std::string& filepath) {
		auto pak_file = pak{};
		std::ifstream file(filepath, std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open PAK file: " + filepath);
		}

		// Read file header
		file.read(reinterpret_cast<char*>(&pak_file.header), sizeof(file_header));
		if (std::string(pak_file.header.signature, file_header_size) != file_header_signature) {
			throw std::runtime_error("Invalid PAK file header in file: " + filepath);
		}

		// Read sprite count
		file.read(reinterpret_cast<char*>(&pak_file.sprite_count), sizeof(uint32_t));
		if (pak_file.sprite_count == 0) {
			throw std::runtime_error("No sprites found in PAK file: " + filepath);
		}
		if(pak_file.sprite_count > 1500) {
			throw std::runtime_error("Unreasonable number of sprites detected, overflow?: " + filepath);
		}

		// Read sprite entries
		pak_file.sprite_entries.resize(pak_file.sprite_count);
		file.read(reinterpret_cast<char*>(pak_file.sprite_entries.data()), pak_file.sprite_count * sizeof(sprite_entry));
		if(file.fail()) {
			throw std::runtime_error("Failed to read sprite entries from PAK file: " + filepath);
		}
		if(pak_file.sprite_entries.size() != pak_file.sprite_count) {
			throw std::runtime_error("Mismatch in sprite entries count in PAK file: " + filepath);
		}

		// Read each sprite
		pak_file.sprites.resize(pak_file.sprite_count);
		for (auto it = pak_file.sprite_entries.begin(); it != pak_file.sprite_entries.end(); it++) {
			auto& entry = *it;
			size_t index = std::distance(pak_file.sprite_entries.begin(), it);
			auto& offset = entry.offset;
			auto& size = entry.size;

			if(file.tellg() != static_cast<std::streampos>(offset)) {
				throw std::runtime_error("Sprite offset mismatch in PAK file, corrupt PAK file?: " + filepath);
			}

			auto& sprite_obj = pak_file.sprites[index];

			// Read sprite header
			file.read(reinterpret_cast<char*>(&sprite_obj.header), sizeof(sprite_header));
			if (std::string(sprite_obj.header.signature, sprite_header_size) != sprite_header_signature) {
				throw std::runtime_error("Invalid sprite header in PAK file: " + filepath);
			}

			// Read sprite rectangles count
			uint32_t rect_count = 0;
			file.read(reinterpret_cast<char*>(&rect_count), sizeof(uint32_t));
			if (rect_count == 0 || rect_count > 1000) {
				throw std::runtime_error("Unreasonable number of sprite rectangles detected, overflow?: " + filepath);
			}

			// Read sprite rectangles
			sprite_obj.sprite_rectangles.resize(rect_count);
			file.read(reinterpret_cast<char*>(sprite_obj.sprite_rectangles.data()), rect_count * sizeof(sprite_rect));
			if(file.fail()) {
				throw std::runtime_error("Failed to read sprite rectangles from PAK file: " + filepath);
			}
			if(sprite_obj.sprite_rectangles.size() != rect_count) {
				throw std::runtime_error("Mismatch in sprite rectangles count in PAK file: " + filepath);
			}

			// Skip 4 bytes of padding
			file.seekg(sizeof(uint32_t), std::ios::cur);

			// Read image data
			int sizeOfSpriteHeader = sizeof(sprite_header);
			int sizeOfRectangleCount = sizeof(uint32_t);
			int sizeOfSpriteRectangles = (rect_count * sizeof(sprite_rect)) + sizeOfRectangleCount;
			int sizeOfPadding = sizeof(uint32_t);
			long newLength = size - (sizeOfSpriteHeader + sizeOfSpriteRectangles) - sizeOfPadding;
			sprite_obj.image_data.resize(newLength);
			file.read(reinterpret_cast<char*>(sprite_obj.image_data.data()), newLength);
			if(file.fail()) {
				throw std::runtime_error("Failed to read image data from PAK file: " + filepath);
			}
		}

		pak_file.pak_file_path = filepath;
		return pak_file;
	}

	inline static pak loadpak_fast(const std::string& filepath) {
		pak pak_file{};
		std::ifstream file(filepath, std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open PAK file: " + filepath);
		}

		file.seekg(sizeof(file_header), std::ios::beg);

		file.read(reinterpret_cast<char*>(&pak_file.sprite_count), sizeof(uint32_t));
		if (pak_file.sprite_count == 0 || pak_file.sprite_count > 1500) {
			throw std::runtime_error("Invalid sprite count in: " + filepath);
		}

		pak_file.sprite_entries.resize(pak_file.sprite_count);
		file.read(reinterpret_cast<char*>(pak_file.sprite_entries.data()), pak_file.sprite_count * sizeof(sprite_entry));
		if (file.fail()) {
			throw std::runtime_error("Failed to read sprite entries from: " + filepath);
		}

		pak_file.sprites.resize(pak_file.sprite_count);
		for (size_t i = 0; i < pak_file.sprite_count; ++i) {
			const auto& entry = pak_file.sprite_entries[i];
			file.seekg(entry.offset + SPRITE_HEADER_SKIP, std::ios::beg);

			uint32_t rect_count = 0;
			file.read(reinterpret_cast<char*>(&rect_count), sizeof(uint32_t));
			if (rect_count > 1000) {
				throw std::runtime_error("Invalid rect count in: " + filepath);
			}

			auto& sprite_obj = pak_file.sprites[i];
			if (rect_count == 0) continue; // empty sprite (no rectangles/image)

			sprite_obj.sprite_rectangles.resize(rect_count);
			file.read(reinterpret_cast<char*>(sprite_obj.sprite_rectangles.data()), rect_count * sizeof(sprite_rect));
			if (file.fail()) {
				throw std::runtime_error("Failed to read sprite rectangles from: " + filepath);
			}

			file.seekg(PADDING_SIZE, std::ios::cur);

			const long image_byte_size =
				entry.size - (SPRITE_HEADER_SKIP + (rect_count * sizeof(sprite_rect)) + RECT_COUNT_SIZE + PADDING_SIZE);

			sprite_obj.image_data.resize(image_byte_size);
			file.read(reinterpret_cast<char*>(sprite_obj.image_data.data()), image_byte_size);
			if (file.fail()) {
				throw std::runtime_error("Failed to read image data from: " + filepath);
			}
		}

		pak_file.pak_file_path = filepath;
		return pak_file;
	}

	inline static sprite get_sprite(const std::string& filepath, size_t sprite_index) {
		std::ifstream file(filepath, std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open PAK file: " + filepath);
		}

		// Read file header
		auto pak_file_header = file_header{};
		file.read(reinterpret_cast<char*>(&pak_file_header), sizeof(file_header));
		if (std::string(pak_file_header.signature, file_header_size) != file_header_signature) {
			throw std::runtime_error("Invalid PAK file header in file: " + filepath);
		}

		// Read sprite count
		uint32_t sprite_count = 0;
		file.read(reinterpret_cast<char*>(&sprite_count), sizeof(uint32_t));
		if (sprite_count == 0) {
			throw std::runtime_error("No sprites found in PAK file: " + filepath);
		}
		if (sprite_count > 1500) {
			throw std::runtime_error("Unreasonable number of sprites detected, overflow?: " + filepath);
		}
		if(sprite_index >= sprite_count) {
			throw std::runtime_error("Sprite index out of bounds in PAK file: " + filepath);
		}

		// Read sprite entries
		std::vector<sprite_entry> sprite_entries{};
		sprite_entries.resize(sprite_count);
		file.read(reinterpret_cast<char*>(sprite_entries.data()), sprite_count * sizeof(sprite_entry));
		if (file.fail()) {
			throw std::runtime_error("Failed to read sprite entries from PAK file: " + filepath);
		}
		if (sprite_entries.size() != sprite_count) {
			throw std::runtime_error("Mismatch in sprite entries count in PAK file: " + filepath);
		}

		// Seek to the desired sprite
		auto& entry = sprite_entries[sprite_index];
		file.seekg(entry.offset, std::ios::beg);
		auto sprite_obj = sprite{};

		// Read sprite header
		file.read(reinterpret_cast<char*>(&sprite_obj.header), sizeof(sprite_header));
		if (std::string(sprite_obj.header.signature, sprite_header_size) != sprite_header_signature) {
			throw std::runtime_error("Invalid sprite header in PAK file: " + filepath);
		}

		// Read sprite rectangles count
		uint32_t rect_count = 0;
		file.read(reinterpret_cast<char*>(&rect_count), sizeof(uint32_t));
		if (rect_count == 0 || rect_count > 1000) {
			throw std::runtime_error("Unreasonable number of sprite rectangles detected, overflow?: " + filepath);
		}

		// Read sprite rectangles
		sprite_obj.sprite_rectangles.resize(rect_count);
		file.read(reinterpret_cast<char*>(sprite_obj.sprite_rectangles.data()), rect_count * sizeof(sprite_rect));
		if (file.fail()) {
			throw std::runtime_error("Failed to read sprite rectangles from PAK file: " + filepath);
		}
		if (sprite_obj.sprite_rectangles.size() != rect_count) {
			throw std::runtime_error("Mismatch in sprite rectangles count in PAK file: " + filepath);
		}

		// Skip 4 bytes of padding
		file.seekg(sizeof(uint32_t), std::ios::cur);

		// Read image data
		long image_byte_size = entry.size - (sizeof(sprite_header) + ((rect_count * sizeof(sprite_rect)) + sizeof(uint32_t))) - sizeof(uint32_t);
		sprite_obj.image_data.resize(image_byte_size);
		file.read(reinterpret_cast<char*>(sprite_obj.image_data.data()), image_byte_size);
		if (file.fail()) {
			throw std::runtime_error("Failed to read image data from PAK file: " + filepath);
		}

		return sprite_obj;
	}

	inline static sprite get_sprite_fast(const std::string& filepath, size_t sprite_index) {
		std::ifstream file(filepath, std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open PAK file: " + filepath);
		}

		sprite_entry entry{};
		file.seekg(sizeof(file_header) + sizeof(uint32_t) + (sprite_index * sizeof(sprite_entry)), std::ios::beg);
		file.read(reinterpret_cast<char*>(&entry), sizeof(entry));
		if (file.fail()) {
			throw std::runtime_error("Failed to read sprite entry from: " + filepath);
		}

		file.seekg(entry.offset + SPRITE_HEADER_SKIP, std::ios::beg);

		sprite sprite_obj{};
		uint32_t rect_count = 0;
		file.read(reinterpret_cast<char*>(&rect_count), sizeof(uint32_t));
		if (rect_count > 1000) {
			throw std::runtime_error("Invalid rect count in: " + filepath);
		}

		if (rect_count == 0) return sprite_obj; // empty sprite (no rectangles/image)

		sprite_obj.sprite_rectangles.resize(rect_count);
		file.read(reinterpret_cast<char*>(sprite_obj.sprite_rectangles.data()), rect_count * sizeof(sprite_rect));
		if (file.fail()) {
			throw std::runtime_error("Failed to read sprite rectangles from: " + filepath);
		}

		file.seekg(PADDING_SIZE, std::ios::cur);

		const long image_byte_size =
			entry.size - (SPRITE_HEADER_SKIP + (rect_count * sizeof(sprite_rect)) + RECT_COUNT_SIZE + PADDING_SIZE);

		sprite_obj.image_data.resize(image_byte_size);
		file.read(reinterpret_cast<char*>(sprite_obj.image_data.data()), image_byte_size);
		if (file.fail()) {
			throw std::runtime_error("Failed to read image data from: " + filepath);
		}

		return sprite_obj;
	}

	// Load only metadata (sprite entries + frame rects) from a PAK file.
	// Skips all image data — sprites store only frame rects and reload from disk on demand.
	inline static pak loadpak_metadata_fast(const std::string& filepath) {
		pak pak_file{};
		std::ifstream file(filepath, std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open PAK file: " + filepath);
		}

		file.seekg(sizeof(file_header), std::ios::beg);

		file.read(reinterpret_cast<char*>(&pak_file.sprite_count), sizeof(uint32_t));
		if (pak_file.sprite_count == 0 || pak_file.sprite_count > 1500) {
			throw std::runtime_error("Invalid sprite count in: " + filepath);
		}

		pak_file.sprite_entries.resize(pak_file.sprite_count);
		file.read(reinterpret_cast<char*>(pak_file.sprite_entries.data()), pak_file.sprite_count * sizeof(sprite_entry));
		if (file.fail()) {
			throw std::runtime_error("Failed to read sprite entries from: " + filepath);
		}

		pak_file.sprites.resize(pak_file.sprite_count);
		for (size_t i = 0; i < pak_file.sprite_count; ++i) {
			const auto& entry = pak_file.sprite_entries[i];
			file.seekg(entry.offset + SPRITE_HEADER_SKIP, std::ios::beg);

			uint32_t rect_count = 0;
			file.read(reinterpret_cast<char*>(&rect_count), sizeof(uint32_t));
			if (rect_count > 1000) {
				throw std::runtime_error("Invalid rect count in: " + filepath);
			}

			auto& sprite_obj = pak_file.sprites[i];
			if (rect_count == 0) continue;

			sprite_obj.sprite_rectangles.resize(rect_count);
			file.read(reinterpret_cast<char*>(sprite_obj.sprite_rectangles.data()), rect_count * sizeof(sprite_rect));
			if (file.fail()) {
				throw std::runtime_error("Failed to read sprite rectangles from: " + filepath);
			}

			// Skip image data — not needed for metadata-only load
		}

		pak_file.pak_file_path = filepath;
		return pak_file;
	}

	// Load only metadata (frame rects) for a single sprite from a PAK file.
	// Skips image data entirely.
	inline static sprite get_sprite_metadata_fast(const std::string& filepath, size_t sprite_index) {
		std::ifstream file(filepath, std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open PAK file: " + filepath);
		}

		sprite_entry entry{};
		file.seekg(sizeof(file_header) + sizeof(uint32_t) + (sprite_index * sizeof(sprite_entry)), std::ios::beg);
		file.read(reinterpret_cast<char*>(&entry), sizeof(entry));
		if (file.fail()) {
			throw std::runtime_error("Failed to read sprite entry from: " + filepath);
		}

		file.seekg(entry.offset + SPRITE_HEADER_SKIP, std::ios::beg);

		sprite sprite_obj{};
		uint32_t rect_count = 0;
		file.read(reinterpret_cast<char*>(&rect_count), sizeof(uint32_t));
		if (rect_count > 1000) {
			throw std::runtime_error("Invalid rect count in: " + filepath);
		}

		if (rect_count == 0) return sprite_obj;

		sprite_obj.sprite_rectangles.resize(rect_count);
		file.read(reinterpret_cast<char*>(sprite_obj.sprite_rectangles.data()), rect_count * sizeof(sprite_rect));
		if (file.fail()) {
			throw std::runtime_error("Failed to read sprite rectangles from: " + filepath);
		}

		// Skip image data — not needed for metadata-only load
		return sprite_obj;
	}

	// Soft-locks PAK files by holding open file handles to prevent external modification.
	// On Windows, an open ifstream prevents deletion/rename of the file.
	// On Linux, this is advisory only — provides a held file descriptor.
	class pak_lock_manager {
	public:
		static void lock(const std::string& filepath) {
			std::lock_guard<std::mutex> guard(s_mutex);
			if (s_locks.find(filepath) != s_locks.end())
				return;
			auto file = std::make_unique<std::ifstream>(filepath, std::ios::binary);
			if (file->is_open()) {
				s_locks[filepath] = std::move(file);
			}
		}

		static void unlock(const std::string& filepath) {
			std::lock_guard<std::mutex> guard(s_mutex);
			s_locks.erase(filepath);
		}

		static void unlock_all() {
			std::lock_guard<std::mutex> guard(s_mutex);
			s_locks.clear();
		}

		static bool is_locked(const std::string& filepath) {
			std::lock_guard<std::mutex> guard(s_mutex);
			return s_locks.find(filepath) != s_locks.end();
		}

	private:
		static inline std::mutex s_mutex;
		static inline std::unordered_map<std::string, std::unique_ptr<std::ifstream>> s_locks;
	};

	inline uint8_t rotl8(uint8_t v, unsigned int s) {
		return static_cast<uint8_t>((v << s) | (v >> (8 - s)));
	}

	inline uint8_t rotr8(uint8_t v, unsigned int s) {
		return static_cast<uint8_t>((v >> s) | (v << (8 - s)));
	}

	inline void decrypt_bytes(std::vector<uint8_t>& data,
		const std::string& key,
		size_t start_offset = 0) {
		if (key.empty())
			throw std::invalid_argument("Decryption key cannot be empty.");
		if (data.empty())
			return;

		const size_t key_len = key.size();
		for (size_t i = 0; i < data.size(); ++i) {
			size_t global_index = start_offset + i;
			uint8_t key_byte = static_cast<uint8_t>(key[global_index % key_len]);
			uint8_t rot = key_byte % 8;
			if (rot)
				data[i] = rotr8(data[i], rot);
			data[i] ^= key_byte;
		}
	}

	inline void encrypt_bytes(std::vector<uint8_t>& data,
		const std::string& key,
		size_t start_offset = 0) {
		if (key.empty())
			throw std::invalid_argument("Encryption key cannot be empty.");
		if (data.empty())
			return;

		const size_t key_len = key.size();
		for (size_t i = 0; i < data.size(); ++i) {
			size_t global_index = start_offset + i;
			uint8_t key_byte = static_cast<uint8_t>(key[global_index % key_len]);
			data[i] ^= key_byte;
			uint8_t rot = key_byte % 8;
			if (rot)
				data[i] = rotl8(data[i], rot);
		}
	}

	inline void encrypt_file(const std::string& input_path,
		const std::string& output_path,
		const std::string& key) {
		if (key.empty())
			throw std::invalid_argument("Encryption key cannot be empty.");

		std::ifstream in(input_path, std::ios::binary);
		if (!in.is_open())
			throw std::runtime_error("Failed to open input file: " + input_path);

		std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), {});
		in.close();

		encrypt_bytes(data, key, 0); // start from offset 0

		std::ofstream out(output_path, std::ios::binary);
		if (!out.is_open())
			throw std::runtime_error("Failed to open output file: " + output_path);

		out.write(reinterpret_cast<const char*>(data.data()), data.size());
		out.close();
	}

	inline void decrypt_file(const std::string& input_path,
		const std::string& output_path,
		const std::string& key) {
		if (key.empty())
			throw std::invalid_argument("Decryption key cannot be empty.");

		std::ifstream in(input_path, std::ios::binary);
		if (!in.is_open())
			throw std::runtime_error("Failed to open input file: " + input_path);

		std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), {});
		in.close();

		decrypt_bytes(data, key, 0); // start from offset 0

		std::ofstream out(output_path, std::ios::binary);
		if (!out.is_open())
			throw std::runtime_error("Failed to open output file: " + output_path);

		out.write(reinterpret_cast<const char*>(data.data()), data.size());
		out.close();
	}

	inline std::vector<uint8_t>& read_and_decrypt_section(std::ifstream& file,
		size_t offset,
		size_t size,
		const std::string& key) {
		static std::vector<uint8_t> buffer; // persistent buffer
		buffer.resize(size);                 // grow if needed

		file.seekg(offset, std::ios::beg);
		file.read(reinterpret_cast<char*>(buffer.data()), size);
		if (file.fail())
			throw std::runtime_error("Failed to read encrypted section.");

		decrypt_bytes(buffer, key, offset); // deterministic offset-aware decryption
		return buffer;                      // reuse, avoid reallocation
	}

	inline static pak loadpak_encrypted(const std::string& filepath, const std::string& key) {
		pak pak_file{};
		std::ifstream file(filepath, std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open encrypted PAK file: " + filepath);
		}

		// decrypt header + count
		auto header_buf = read_and_decrypt_section(file, 0, sizeof(file_header) + sizeof(uint32_t), key);
		std::memcpy(&pak_file.header, header_buf.data(), sizeof(file_header));

		if (std::string(pak_file.header.signature, file_header_size) != file_header_signature)
			throw std::runtime_error("Invalid encrypted PAK header: " + filepath);

		std::memcpy(&pak_file.sprite_count, header_buf.data() + sizeof(file_header), sizeof(uint32_t));
		if (pak_file.sprite_count == 0 || pak_file.sprite_count > 1500)
			throw std::runtime_error("Invalid sprite count in encrypted file: " + filepath);

		// decrypt sprite entries
		auto entries_buf = read_and_decrypt_section(file, sizeof(file_header) + sizeof(uint32_t),
			pak_file.sprite_count * sizeof(sprite_entry), key);

		pak_file.sprite_entries.resize(pak_file.sprite_count);
		std::memcpy(pak_file.sprite_entries.data(), entries_buf.data(), entries_buf.size());

		pak_file.sprites.resize(pak_file.sprite_count);
		for (size_t i = 0; i < pak_file.sprite_count; ++i) {
			const auto& entry = pak_file.sprite_entries[i];
			auto sprite_data = read_and_decrypt_section(file, entry.offset, entry.size, key);

			std::istringstream sprite_stream(std::string(reinterpret_cast<char*>(sprite_data.data()), sprite_data.size()),
				std::ios::binary);

			sprite sprite_obj{};
			sprite_stream.read(reinterpret_cast<char*>(&sprite_obj.header), sizeof(sprite_header));
			uint32_t rect_count = 0;
			sprite_stream.read(reinterpret_cast<char*>(&rect_count), sizeof(uint32_t));

			if (rect_count > 1000)
				throw std::runtime_error("Invalid rect count in encrypted PAK: " + filepath);

			if (rect_count > 0) {
				sprite_obj.sprite_rectangles.resize(rect_count);
				sprite_stream.read(reinterpret_cast<char*>(sprite_obj.sprite_rectangles.data()), rect_count * sizeof(sprite_rect));

				sprite_stream.seekg(PADDING_SIZE, std::ios::cur);

				const long image_size = entry.size -
					(sizeof(sprite_header) + (rect_count * sizeof(sprite_rect)) + RECT_COUNT_SIZE + PADDING_SIZE);
				sprite_obj.image_data.resize(image_size);
				sprite_stream.read(reinterpret_cast<char*>(sprite_obj.image_data.data()), image_size);
			}

			pak_file.sprites[i] = sprite_obj;
		}

		pak_file.pak_file_path = filepath;
		return pak_file;
	}

	inline static pak loadpak_fast_encrypted(const std::string& filepath, const std::string& key) {
		pak pak_file{};
		std::ifstream file(filepath, std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open encrypted PAK file: " + filepath);
		}

		// decrypt header and entries
		auto header_buf = read_and_decrypt_section(file, 0, sizeof(file_header) + sizeof(uint32_t), key);
		std::memcpy(&pak_file.sprite_count, header_buf.data() + sizeof(file_header), sizeof(uint32_t));

		if (pak_file.sprite_count == 0 || pak_file.sprite_count > 1500)
			throw std::runtime_error("Invalid sprite count in encrypted PAK: " + filepath);

		auto entries_buf = read_and_decrypt_section(file, sizeof(file_header) + sizeof(uint32_t),
			pak_file.sprite_count * sizeof(sprite_entry), key);

		pak_file.sprite_entries.resize(pak_file.sprite_count);
		std::memcpy(pak_file.sprite_entries.data(), entries_buf.data(), entries_buf.size());

		pak_file.sprites.resize(pak_file.sprite_count);
		for (size_t i = 0; i < pak_file.sprite_count; ++i) {
			const auto& entry = pak_file.sprite_entries[i];
			auto sprite_data = read_and_decrypt_section(file, entry.offset, entry.size, key);

			std::istringstream ss(std::string(reinterpret_cast<char*>(sprite_data.data()), sprite_data.size()),
				std::ios::binary);

			sprite sprite_obj{};
			ss.seekg(SPRITE_HEADER_SKIP, std::ios::beg);

			uint32_t rect_count = 0;
			ss.read(reinterpret_cast<char*>(&rect_count), sizeof(uint32_t));
			if (rect_count > 1000)
				throw std::runtime_error("Invalid rect count in encrypted fast load.");

			if (rect_count > 0) {
				sprite_obj.sprite_rectangles.resize(rect_count);
				ss.read(reinterpret_cast<char*>(sprite_obj.sprite_rectangles.data()), rect_count * sizeof(sprite_rect));

				ss.seekg(PADDING_SIZE, std::ios::cur);
				const long image_size = entry.size -
					(SPRITE_HEADER_SKIP + (rect_count * sizeof(sprite_rect)) + RECT_COUNT_SIZE + PADDING_SIZE);

				sprite_obj.image_data.resize(image_size);
				ss.read(reinterpret_cast<char*>(sprite_obj.image_data.data()), image_size);
			}

			pak_file.sprites[i] = sprite_obj;
		}

		pak_file.pak_file_path = filepath;
		return pak_file;
	}

	inline static sprite get_sprite_encrypted(const std::string& filepath, size_t sprite_index, const std::string& key) {
		std::ifstream file(filepath, std::ios::binary);
		if (!file.is_open())
			throw std::runtime_error("Failed to open encrypted PAK file: " + filepath);

		// decrypt header + entries to locate sprite
		auto header_buf = read_and_decrypt_section(file, 0, sizeof(file_header) + sizeof(uint32_t), key);
		uint32_t sprite_count = 0;
		std::memcpy(&sprite_count, header_buf.data() + sizeof(file_header), sizeof(uint32_t));

		if (sprite_count == 0 || sprite_index >= sprite_count)
			throw std::runtime_error("Invalid sprite index in encrypted PAK.");

		auto entries_buf = read_and_decrypt_section(file, sizeof(file_header) + sizeof(uint32_t),
			sprite_count * sizeof(sprite_entry), key);

		std::vector<sprite_entry> entries(sprite_count);
		std::memcpy(entries.data(), entries_buf.data(), entries_buf.size());

		const auto& entry = entries[sprite_index];
		auto sprite_data = read_and_decrypt_section(file, entry.offset, entry.size, key);

		std::istringstream ss(std::string(reinterpret_cast<char*>(sprite_data.data()), sprite_data.size()), std::ios::binary);
		sprite sprite_obj{};
		ss.read(reinterpret_cast<char*>(&sprite_obj.header), sizeof(sprite_header));

		uint32_t rect_count = 0;
		ss.read(reinterpret_cast<char*>(&rect_count), sizeof(uint32_t));

		sprite_obj.sprite_rectangles.resize(rect_count);
		ss.read(reinterpret_cast<char*>(sprite_obj.sprite_rectangles.data()), rect_count * sizeof(sprite_rect));
		ss.seekg(PADDING_SIZE, std::ios::cur);

		const long image_size = entry.size -
			(sizeof(sprite_header) + (rect_count * sizeof(sprite_rect)) + RECT_COUNT_SIZE + PADDING_SIZE);
		sprite_obj.image_data.resize(image_size);
		ss.read(reinterpret_cast<char*>(sprite_obj.image_data.data()), image_size);

		return sprite_obj;
	}

	inline static sprite get_sprite_fast_encrypted(const std::string& filepath, size_t sprite_index, const std::string& key) {
		std::ifstream file(filepath, std::ios::binary);
		if (!file.is_open())
			throw std::runtime_error("Failed to open encrypted PAK file: " + filepath);

		auto header_buf = read_and_decrypt_section(file, 0, sizeof(file_header) + sizeof(uint32_t), key);
		uint32_t sprite_count = 0;
		std::memcpy(&sprite_count, header_buf.data() + sizeof(file_header), sizeof(uint32_t));

		if (sprite_count == 0 || sprite_index >= sprite_count)
			throw std::runtime_error("Invalid sprite index in encrypted fast path.");

		auto entries_buf = read_and_decrypt_section(file, sizeof(file_header) + sizeof(uint32_t),
			sprite_count * sizeof(sprite_entry), key);

		std::vector<sprite_entry> entries(sprite_count);
		std::memcpy(entries.data(), entries_buf.data(), entries_buf.size());

		const auto& entry = entries[sprite_index];
		auto sprite_data = read_and_decrypt_section(file, entry.offset, entry.size, key);

		std::istringstream ss(std::string(reinterpret_cast<char*>(sprite_data.data()), sprite_data.size()), std::ios::binary);

		sprite sprite_obj{};
		ss.seekg(SPRITE_HEADER_SKIP, std::ios::beg);

		uint32_t rect_count = 0;
		ss.read(reinterpret_cast<char*>(&rect_count), sizeof(uint32_t));

		sprite_obj.sprite_rectangles.resize(rect_count);
		ss.read(reinterpret_cast<char*>(sprite_obj.sprite_rectangles.data()), rect_count * sizeof(sprite_rect));
		ss.seekg(PADDING_SIZE, std::ios::cur);

		const long image_size = entry.size -
			(SPRITE_HEADER_SKIP + (rect_count * sizeof(sprite_rect)) + RECT_COUNT_SIZE + PADDING_SIZE);
		sprite_obj.image_data.resize(image_size);
		ss.read(reinterpret_cast<char*>(sprite_obj.image_data.data()), image_size);

		return sprite_obj;
	}
}