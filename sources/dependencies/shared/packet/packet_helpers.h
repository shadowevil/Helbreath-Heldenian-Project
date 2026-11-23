#pragma once

#include "packet_common.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>
#include <cstring>

namespace hb {
namespace net {
	template <typename T>
	inline bool IsPacketType()
	{
		return std::is_trivially_copyable<T>::value && std::is_standard_layout<T>::value;
	}

	template <typename T>
	inline const T* PacketCast(const void* data, std::size_t size)
	{
		if (!IsPacketType<T>() || data == nullptr || size < sizeof(T)) return nullptr;
		return reinterpret_cast<const T*>(data);
	}

	template <typename T>
	inline T* PacketCast(void* data, std::size_t size)
	{
		if (!IsPacketType<T>() || data == nullptr || size < sizeof(T)) return nullptr;
		return reinterpret_cast<T*>(data);
	}

	class PacketWriter
	{
	public:
		void Reset()
		{
			m_buffer.clear();
		}

		void Reserve(std::size_t size)
		{
			m_buffer.reserve(size);
		}

		template <typename T>
		T* Append()
		{
			static_assert(std::is_trivially_copyable<T>::value && std::is_standard_layout<T>::value,
				"PacketWriter only supports packet types.");
			const auto offset = m_buffer.size();
			m_buffer.resize(offset + sizeof(T));
			std::memset(m_buffer.data() + offset, 0, sizeof(T));
			return reinterpret_cast<T*>(m_buffer.data() + offset);
		}

		void AppendBytes(const void* data, std::size_t size)
		{
			if (size == 0) return;
			const auto offset = m_buffer.size();
			m_buffer.resize(offset + size);
			if (data) {
				std::memcpy(m_buffer.data() + offset, data, size);
			} else {
				std::memset(m_buffer.data() + offset, 0, size);
			}
		}

		const char* Data() const
		{
			return reinterpret_cast<const char*>(m_buffer.data());
		}

		char* Data()
		{
			return reinterpret_cast<char*>(m_buffer.data());
		}

		std::size_t size() const
		{
			return m_buffer.size();
		}

	private:
		std::vector<std::uint8_t> m_buffer;
	};
}
}
