#pragma once
#include <cstdint>
#include "direction_helpers.h"

using hb::shared::direction::direction;

class CMapData;

class CPlayerController
{
public:
	CPlayerController();
	void reset();

	// Destination
	void set_destination(short x, short y) { m_dest_x = x; m_dest_y = y; }
	void move_destination(short dx, short dy) { m_dest_x += dx; m_dest_y += dy; }
	short get_destination_x() const { return m_dest_x; }
	short get_destination_y() const { return m_dest_y; }
	void clear_destination() { m_dest_x = 0; m_dest_y = 0; }

	// Command State
	char get_command() const { return m_command; }
	void set_command(char cmd) { m_command = cmd; }

	bool is_command_available() const { return m_command_available; }
	void set_command_available(bool available) { m_command_available = available; }

	uint32_t get_command_time() const { return m_command_time; }
	void set_command_time(uint32_t time) { m_command_time = time; }

	// Command Throttling
	uint8_t get_command_count() const { return m_command_count; }
	void set_command_count(uint8_t count) { m_command_count = count; }
	void increment_command_count() { m_command_count++; }
	void decrement_command_count() { if (m_command_count > 0) m_command_count--; }
	void reset_command_count() { m_command_count = 0; }

	// Previous Move Tracking (for blocked move detection)
	int get_prev_move_x() const { return m_prev_move_x; }
	int get_prev_move_y() const { return m_prev_move_y; }
	void set_prev_move(int x, int y) { m_prev_move_x = x; m_prev_move_y = y; }

	bool is_prev_move_blocked() const { return m_is_prev_move_blocked; }
	void set_prev_move_blocked(bool blocked) { m_is_prev_move_blocked = blocked; }

	// Turn Direction (0 = clockwise, 1 = counter-clockwise for obstacle avoidance)
	char get_player_turn() const { return m_player_turn; }
	void set_player_turn(char turn) { m_player_turn = turn; }

	// Pending stop direction (for right-click stop while moving)
	direction get_pending_stop_dir() const { return m_pending_stop_dir; }
	void set_pending_stop_dir(direction dir) { m_pending_stop_dir = dir; }
	void clear_pending_stop_dir() { m_pending_stop_dir = direction{}; }

	// Attack animation cooldown (time-based minimum to match server validation)
	uint32_t get_attack_end_time() const { return m_attack_end_time; }
	void set_attack_end_time(uint32_t time) { m_attack_end_time = time; }

	// Direction Calculation
	// Returns direction 1-8 to move from (sX,sY) toward (dstX,dstY), or 0 if no valid move
	// move_check: if true, considers previously blocked moves
	// mim: if true, reverses source/destination for direction calculation (illusion movement)
	direction get_next_move_dir(short sX, short sY, short dstX, short dstY,
	                    CMapData* map_data, bool move_check = false, bool mim = false);

	// Calculates optimal turn direction (clockwise vs counter-clockwise) to reach destination
	// Sets m_player_turn based on which direction reaches destination in fewer steps
	void calculate_player_turn(short playerX, short playerY, CMapData* map_data);

private:
	// Destination coordinates
	short m_dest_x;
	short m_dest_y;

	// Current command
	char m_command;

	// Command availability and timing
	bool m_command_available;
	uint32_t m_command_time;
	uint8_t m_command_count;

	// Previous move tracking
	int m_prev_move_x;
	int m_prev_move_y;
	bool m_is_prev_move_blocked;

	// Turn direction for obstacle avoidance
	char m_player_turn;

	// Pending stop direction (applied when movement ends)
	direction m_pending_stop_dir;

	// Earliest time player can act after an attack (prevents false positive server disconnects)
	uint32_t m_attack_end_time;
};
