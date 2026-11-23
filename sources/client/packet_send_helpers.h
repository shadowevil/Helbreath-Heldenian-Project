#pragma once

#include "packet/packet_request.h"
#include "common_types.h"
#include "net_messages.h"

namespace hb::net {

// Build a CommandCommon packet (no text). Fills header, player pos, timestamp.
inline PacketCommandCommonWithTime make_common_command(
	uint16_t command, int16_t player_x, int16_t player_y, uint8_t dir = 0)
{
	PacketCommandCommonWithTime pkt{};
	pkt.base.header.msg_id = hb::shared::net::MsgId::CommandCommon;
	pkt.base.header.msg_type = command;
	pkt.base.x = player_x;
	pkt.base.y = player_y;
	pkt.base.dir = dir;
	pkt.time_ms = GameClock::get_time_ms();
	return pkt;
}

// Build a CommandCommon packet (with text). Fills header, player pos.
inline PacketCommandCommonWithString make_common_command_str(
	uint16_t command, int16_t player_x, int16_t player_y, uint8_t dir = 0)
{
	PacketCommandCommonWithString pkt{};
	pkt.base.header.msg_id = hb::shared::net::MsgId::CommandCommon;
	pkt.base.header.msg_type = command;
	pkt.base.x = player_x;
	pkt.base.y = player_y;
	pkt.base.dir = dir;
	return pkt;
}

// Build a simple motion packet (stop, move, run, getitem, magic). Fills header, player pos, timestamp.
inline PacketCommandMotionSimple make_motion(
	uint16_t command, int16_t player_x, int16_t player_y, uint8_t dir,
	int16_t dx = 0, int16_t dy = 0, int16_t type = 0)
{
	PacketCommandMotionSimple pkt{};
	pkt.base.header.msg_id = hb::shared::net::MsgId::CommandMotion;
	pkt.base.header.msg_type = command;
	pkt.base.x = player_x;
	pkt.base.y = player_y;
	pkt.base.dir = dir;
	pkt.base.dx = dx;
	pkt.base.dy = dy;
	pkt.base.type = type;
	pkt.time_ms = GameClock::get_time_ms();
	return pkt;
}

// Build an attack/attackmove motion packet. Fills header, player pos, target, timestamp.
inline PacketCommandMotionAttack make_motion_attack(
	uint16_t command, int16_t player_x, int16_t player_y, uint8_t dir,
	int16_t dx, int16_t dy, int16_t type, uint16_t target_id)
{
	PacketCommandMotionAttack pkt{};
	pkt.base.header.msg_id = hb::shared::net::MsgId::CommandMotion;
	pkt.base.header.msg_type = command;
	pkt.base.x = player_x;
	pkt.base.y = player_y;
	pkt.base.dir = dir;
	pkt.base.dx = dx;
	pkt.base.dy = dy;
	pkt.base.type = type;
	pkt.target_id = target_id;
	pkt.time_ms = GameClock::get_time_ms();
	return pkt;
}

// Build a panning request packet.
inline PacketRequestPanning make_panning_request(uint8_t dir)
{
	PacketRequestPanning pkt{};
	pkt.header.msg_id = hb::shared::net::MsgId::RequestPanning;
	pkt.dir = dir;
	return pkt;
}

} // namespace hb::net
