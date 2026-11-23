#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class ConfigCacheType : uint8_t
{
	Items         = 0,
	Magic         = 1,
	Skills        = 2,
	Npcs          = 3,
	Maps          = 4,
	BalanceConfig  = 5,
	ColorPalette   = 6,
	AttributeTypes = 7,
	COUNT          = 8
};

class LocalCacheManager
{
public:
	static LocalCacheManager& get();

	void initialize();
	void shutdown();

	bool     has_cache(ConfigCacheType type) const;
	std::string get_hash(ConfigCacheType type) const;

	void accumulate_packet(ConfigCacheType type, const char* data, uint32_t size);
	bool finalize_and_save(ConfigCacheType type);

	using PacketCallback = bool(*)(char* data, uint32_t size, void* ctx);
	bool replay_from_cache(ConfigCacheType type, PacketCallback cb, void* ctx);

	void reset_accumulator(ConfigCacheType type);

	bool is_replaying() const { return m_bIsReplaying; }

private:
	static constexpr uint32_t CACHE_MAGIC   = 0x48424346;
	static constexpr uint32_t CACHE_VERSION = 2;

	struct CacheHeader
	{
		uint32_t magic;
		uint32_t version;
		uint32_t configType;
		char sha256[65];
		uint32_t payloadSize;
	};

	struct CacheState
	{
		bool        hasCache;
		std::string hash;
	};

	struct Accumulator
	{
		std::vector<uint8_t> data;
		bool active;
	};

	CacheState  m_state[8]{};
	Accumulator m_accum[8]{};
	bool m_bIsReplaying = false;

	const char* _GetFilename(ConfigCacheType type) const;
	bool _LoadHeader(ConfigCacheType type);
	std::string _ComputePayloadHash(const std::vector<uint8_t>& data) const;
};
