#pragma once

#include "PacketCommon.h"
#include "PacketHeaders.h"
#include "NetConstants.h"

#include <cstdint>

namespace hb {
namespace net {
	HB_PACK_BEGIN
	struct HB_PACKED LoginRequest : packet_base
	{
		PacketHeader header;
		char account_name[hb::shared::limits::AccountNameLen];
		char password[hb::shared::limits::AccountPassLen];
		char world_name[30];
		uint16_t version_major;
		uint16_t version_minor;
		uint16_t version_patch;
	};
	HB_PACK_END

	HB_PACK_BEGIN
	struct HB_PACKED CreateCharacterRequest : packet_base
	{
		PacketHeader header;
		char character_name[hb::shared::limits::CharNameLen];
		char account_name[hb::shared::limits::AccountNameLen];
		char password[hb::shared::limits::AccountPassLen];
		char world_name[30];
		std::uint8_t gender;
		std::uint8_t skin;
		std::uint8_t hairstyle;
		std::uint8_t haircolor;
		std::uint8_t underware;
		std::uint8_t str;
		std::uint8_t vit;
		std::uint8_t dex;
		std::uint8_t intl;
		std::uint8_t mag;
		std::uint8_t chr;
		std::uint8_t class_type; // 1=warrior, 2=mage, 3=master
	};
	HB_PACK_END

	HB_PACK_BEGIN
	struct HB_PACKED DeleteCharacterRequest : packet_base
	{
		PacketHeader header;
		char character_name[hb::shared::limits::CharNameLen];
		char account_name[hb::shared::limits::AccountNameLen];
		char password[hb::shared::limits::AccountPassLen];
		char world_name[30];
	};
	HB_PACK_END

	HB_PACK_BEGIN
	struct HB_PACKED ChangePasswordRequest : packet_base
	{
		PacketHeader header;
		char account_name[hb::shared::limits::AccountNameLen];
		char password[hb::shared::limits::AccountPassLen];
		char new_password[hb::shared::limits::AccountPassLen];
		char new_password_confirm[hb::shared::limits::AccountPassLen];
	};
	HB_PACK_END

	HB_PACK_BEGIN
	struct HB_PACKED CreateAccountRequest : packet_base
	{
		PacketHeader header;
		char account_name[hb::shared::limits::AccountNameLen];
		char password[hb::shared::limits::AccountPassLen];
		char email[hb::shared::limits::AccountEmailLen];
	};
	HB_PACK_END

	HB_PACK_BEGIN
	struct HB_PACKED EnterGameRequest : packet_base
	{
		PacketHeader header;
		char character_name[hb::shared::limits::CharNameLen];
		char map_name[hb::shared::limits::MapNameLen];
		char account_name[hb::shared::limits::AccountNameLen];
		char password[hb::shared::limits::AccountPassLen];
		std::int32_t level;
		char world_name[30];
		uint16_t version_major;
		uint16_t version_minor;
		uint16_t version_patch;
	};
	HB_PACK_END

	HB_PACK_BEGIN
	struct HB_PACKED EnterGameRequestFull : packet_base
	{
		PacketHeader header;
		char character_name[hb::shared::limits::CharNameLen];
		char map_name[hb::shared::limits::MapNameLen];
		char account_name[hb::shared::limits::AccountNameLen];
		char password[hb::shared::limits::AccountPassLen];
		std::int32_t level;
		char world_name[30];
		uint16_t version_major;
		uint16_t version_minor;
		uint16_t version_patch;
	};
	HB_PACK_END

	HB_PACK_BEGIN
	// Enter game response: server IP + port + server name (sent to login client)
	struct HB_PACKED EnterGameResponseData {
		char server_ip[16];
		std::uint16_t server_port;
		char server_name[20];
	};
	HB_PACK_END

	HB_PACK_BEGIN
	// Test log / account verify payload (after PacketHeader)
	struct HB_PACKED TestLogPayload {
		char account_name[hb::shared::limits::AccountNameLen];
		char account_password[hb::shared::limits::AccountPassLen];
		std::int32_t level;
	};
	HB_PACK_END
}
}
