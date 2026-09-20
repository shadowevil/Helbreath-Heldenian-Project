#pragma once

// NetConstants.h - Shared Network Constants
//
// Shared between client and server to ensure buffer sizes, name lengths,
// view parameters, and game limits stay in sync.

namespace hb::shared::limits
{
	constexpr int MsgBufferSize        = 60000;
	constexpr int ItemNameLen          = 42;
	constexpr int CharNameLen          = 11;   // 10 chars + null (wire size is 10)
	constexpr int NpcNameLen           = 21;   // 20 chars + null (wire size is 20)
	constexpr int AccountNameLen       = 11;   // 10 chars + null (wire size is 10)
	constexpr int AccountPassLen       = 11;   // 10 chars + null (wire size is 10)
	constexpr int AccountEmailLen      = 51;   // 50 chars + null
	constexpr int MapNameLen           = 11;   // 10 chars + null
	constexpr int MapDisplayNameLen    = 31;   // 30 chars + null
	constexpr int MaxMagicType         = 100;
	constexpr int MaxSkillType         = 60;
	constexpr int MaxNpcConfigs        = 200;
	constexpr int PlayerMaxLevel       = 180;
	constexpr int MaxItems             = 50;
	constexpr int MaxBankItems         = 1000;
	constexpr int MaxBuildItems        = 300;
	constexpr int MaxPartyMembers      = 9;
	constexpr int MaxCharactersPerAccount = 4;
	constexpr int MaxCrusadeStructures = 300;
} // namespace hb::shared::limits

// View area - tiles visible to the client (Olympia style)
// Uses the LARGER resolution (800x600) values so both resolutions work
// Width: 800/32 = 25 tiles, Height: (600-53)/32 = 17 tiles
namespace hb::shared::view
{
	constexpr int TilesX             = 25;
	constexpr int TilesY             = 17;
	constexpr int RangeX             = TilesX / 2;                    // 12
	constexpr int RangeY             = TilesY / 2;                    // 8
	constexpr int InitDataTilesX     = TilesX;                        // 25
	constexpr int InitDataTilesY     = TilesY + 2;                    // 19
	constexpr int CenterX            = RangeX;                        // 12
	constexpr int CenterY            = RangeY + 1;                    // 9
	constexpr int MapDataBufferX     = 7;
	constexpr int MapDataBufferY     = 8;
	constexpr int PlayerPivotOffsetX = MapDataBufferX + CenterX;      // 19
	constexpr int PlayerPivotOffsetY = MapDataBufferY + CenterY;      // 17
	constexpr int MoveLocMaxEntries  = InitDataTilesX + InitDataTilesY; // 44
	constexpr int RangeBuffer        = 3;
} // namespace hb::shared::view

// Message buffer layout offsets
namespace hb::shared::net
{
	constexpr int MessageOffsetId   = 0;
	constexpr int MessageOffsetType = 4;
} // namespace hb::shared::net
