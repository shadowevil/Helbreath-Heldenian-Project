#pragma once

#include <cstdint>

// server_messages.h - Server-Only Network Messages
//
// Server infrastructure messages, internal tracking constants, and
// inter-server communication protocol IDs.
//
// For shared protocol messages, see dependencies/shared/net/net_messages.h

namespace hb::server::net
{

// Server-Only Message IDs (infrastructure, inter-server, config loading)
namespace ServerMsgId
{
	enum : uint32_t
	{
		// Configuration content messages
		PortionConfigurationContents            = 0x0FA314DE,
		QuestConfigurationContents              = 0x0FA40001,
		BuildItemConfigurationContents          = 0x0FA40002,
		NoticementFileContents                  = 0x0FA40004,
		NpcItemConfigContents                   = 0x0FA40006,

		// Server registration (server-to-server)
		RequestRegisterGameServer               = 0x0512A3F4,
		ResponseRegisterGameServer              = 0x0512A3F5,
		RequestRegisterDbServer                 = 0x0512A3F6,
		ResponseRegisterDbServer                = 0x0512A3F7,

		// Guild update messages (inter-server)
		RequestUpdateGuildInfoNewGuildsman       = 0x0FC9420C,
		RequestUpdateGuildInfoDelGuildsman       = 0x0FC9420D,

		// Player data messages (database communication)
		RequestPlayerData                       = 0x0C152210,
		ResponsePlayerData                      = 0x0C152211,
		ResponseSavePlayerDataReply             = 0x0C152212,
		RequestSavePlayerData                   = 0x0DF3076F,
		RequestSavePlayerDataReply              = 0x0DF30770,
		RequestSavePlayerDataLogout             = 0x0DF3074F,
		RequestNoSaveLogout                     = 0x0DF30750,

		// Guild notifications (inter-server)
		GuildNotify                             = 0x0DF30760,

		// Server teleport messages
		RequestCityHallTeleport                 = 0x0EA03202,
		request_heldenian_teleport                = 0x0EA03206,
		RequestHeldenianWinner                  = 0x3D001240,

		// Game server status messages
		GameServerAlive                         = 0x12A01002,
		game_server_down                          = 0x12A01004,
		TotalGameServerClients                  = 0x12A01005,
		EnterGameConfirm                        = 0x12A01006,

		// Account and server management
		AnnounceAccount                         = 0x13000000,
		AccountInfoChange                       = 0x13000001,
		IpInfoChange                            = 0x13000002,
		GameServerShutdowned                    = 0x14000000,
		AnnounceAccountNewPassword              = 0x14000010,
		RequestIpIdStatus                       = 0x14E91200,
		ResponseIpIdStatus                      = 0x14E91201,
		RequestAccountConnStatus                = 0x14E91202,
		ResponseAccountConnStatus               = 0x14E91203,
		RequestClearAccountConnStatus           = 0x14E91204,
		ResponseClearAccountConnStatus          = 0x14E91205,
		RequestForceDisconnectAccount           = 0x15000000,
		RequestNoCountingSaveLogout             = 0x15000001,

		// Occupy flag data (territory control)
		OccupyFlagData                          = 0x167C0A30,
		RequestSaveAresdenOccupyFlagData        = 0x167C0A31,
		RequestSaveElvineOccupyFlagData         = 0x167C0A32,
		AresdenOccupyFlagSaveFileContents       = 0x17081034,
		ElvineOccupyFlagSaveFileContents        = 0x17081035,

		// Misc server messages
		SendServerShutdownMsg                   = 0x20000000,
		ItemLog                                 = 0x210A914D,
		GameItemLog                             = 0x210A914F,

		// World server messages
		RegisterWorldServer                     = 0x23AA210E,
		RegisterWorldServerSocket               = 0x23AA210F,
		RegisterWorldServerGameServer           = 0x23AB211F,
		RequestCharInfoList                     = 0x23AB2200,
		ResponseCharInfoList                    = 0x23AB2201,
		RequestRemoveGameServer                 = 0x2400000A,
		RequestClearAccountStatus               = 0x24021EE0,
		RequestSetAccountInitStatus             = 0x25000198,
		RequestSetAccountWaitStatus             = 0x25000199,
		RequestCheckAccountPassword             = 0x2654203A,
		WorldServerActivated                    = 0x27049D0C,
		ResponseRegisterWorldServerSocket       = 0x280120A0,

		// IP/Account time and block management
		RequestBlockAccount                     = 0x2900AD10,
		IpTimeChange                            = 0x2900AD20,
		AccountTimeChange                       = 0x2900AD22,
		RequestIpTime                           = 0x2900AD30,
		ResponseIpTime                          = 0x2900AD31,

		// Server reboot and monitor
		RequestAllServerReboot                  = 0x3AE8270A,
		RequestExec1DotBat                      = 0x3AE8370A,
		RequestExec2DotBat                      = 0x3AE8470A,
		MonitorAlive                            = 0x3AE8570A,
		CollectedMana                           = 0x3AE90000,
		MeteorStrike                            = 0x3AE90001,
		ServerStockMsg                          = 0x3AE90013,

		// Party operation
		party_operation                          = 0x3C00123A,

		// Crusade log
		GameCrusadeLog                          = 0x210A914F,
	};
}

// Guild notification sub-types
namespace GuildNotify
{
	enum : uint16_t
	{
		NewGuildsman                            = 0x1F00,
	};
}

// Party status
namespace PartyStatus
{
	enum : int32_t
	{
		Null                                    = 0,
		Processing                              = 1,
		Confirm                                 = 2,
	};
}

// Crusade log types
namespace CrusadeLog
{
	enum : int32_t
	{
		EndCrusade                              = 1,
		StartCrusade                            = 2,
		SelectDuty                              = 3,
		get_exp                                  = 4,
	};
}

// Item spread types
namespace ItemSpread
{
	enum : int32_t
	{
		Random                                  = 1,
		Fixed                                   = 2,
	};
}

// Item log action types
// Named ItemLogAction to avoid collision with the ItemLog class
namespace ItemLogAction
{
	enum : int32_t
	{
		Give                                    = 1,
		Drop                                    = 2,
		get                                     = 3,
		Deplete                                 = 4,
		NewGenDrop                              = 5,
		Buy                                     = 7,
		Sell                                    = 8,
		Retrieve                                = 9,
		Deposit                                 = 10,
		Exchange                                = 11,
		SkillLearn                              = 12,
		Make                                    = 13,
		SummonMonster                           = 14,
		Poisoned                                = 15,
		MagicLearn                              = 16,
		Repair                                  = 17,
		UpgradeFail                             = 29,
		UpgradeSuccess                          = 30,
		Use                                     = 32,
	};
}

// PK log types
namespace PkLog
{
	enum : int32_t
	{
		ReduceCriminal                          = 1,
		ByPlayer                                = 2,
		ByPk                                    = 3,
		ByEnemy                                 = 4,
		ByNpc                                   = 5,
		ByOther                                 = 6,
	};
}

// Standalone constants
constexpr int32_t MaxNpcItemDrop = 25;
constexpr int32_t SlateClearNotify = 99;

} // namespace hb::server::net
