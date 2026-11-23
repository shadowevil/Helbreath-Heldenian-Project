// misc.h: Header-only implementation for CMisc utility functions.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include "global_def.h"
#include "direction_helpers.h"

using hb::shared::direction::direction;

enum {CODE_ENG,CODE_HAN1,CODE_HAN2};

namespace CMisc
{
	// Movement direction calculation using asymmetric zones (N/S 3:1, E/W 4:1 ratio)
	// Returns direction 1-8 (N, NE, E, SE, S, SW, W, NW) or 0 if same position
	static inline direction get_next_move_dir(short sX, short sY, short dX, short dY)
	{
		short diffX = dX - sX;
		short diffY = dY - sY;

		if (diffX == 0 && diffY == 0) return direction{};

		short absX = (diffX < 0) ? -diffX : diffX;
		short absY = (diffY < 0) ? -diffY : diffY;

		if (absY == 0) {
			return (diffX > 0) ? direction::east : direction::west;
		}
		if (absX == 0) {
			return (diffY < 0) ? direction::north : direction::south;
		}

		// Asymmetric ratios: N/S uses 3:1, E/W uses 4:1
		if (absY >= absX * 3) {
			return (diffY < 0) ? direction::north : direction::south;
		}
		if (absX >= absY * 4) {
			return (diffX > 0) ? direction::east : direction::west;
		}

		// Diagonal
		if (diffX > 0 && diffY < 0) return direction::northeast;
		if (diffX > 0 && diffY > 0) return direction::southeast;
		if (diffX < 0 && diffY > 0) return direction::southwest;
		return direction::northwest;
	}

	static inline void get_point(int x0, int y0, int x1, int y1, int * pX, int * pY, int * error_acc, int count)
	{
		int dx, dy, x_inc, y_inc, error, index;
		int result_x, result_y, cnt = 0;

		if ((x0 == x1) && (y0 == y1))
		{	*pX = x0;
			*pY = y0;
			return;
		}
		error = *error_acc;
		result_x = x0;
		result_y = y0;
		dx = x1-x0;
		dy = y1-y0;
		if(dx>=0) x_inc = 1;
		else
		{	x_inc = -1;
			dx = -dx;
		}
		if(dy>=0) y_inc = 1;
		else
		{	y_inc = -1;
			dy = -dy;
		}
		if(dx>dy)
		{	for(index = 0; index <= dx; index++)
			{	error += dy;
				if(error > dx)
				{	error -= dx;
					result_y += y_inc;
				}
				result_x += x_inc;
				cnt++;
				if (cnt >= count) break;
			}
		}else
		{	for(index = 0; index <= dy; index++)
			{	error += dx;
				if(error > dy)
				{	error -= dy;
					result_x += x_inc;
				}
				result_y += y_inc;
				cnt++;
				if (cnt >= count) break;
		}	}
		*pX = result_x;
		*pY = result_y;
		*error_acc = error;
	}

	static inline bool check_valid_string(const char * str)
	{
		size_t len = strlen(str);
		for (size_t i = 0; i < len; i++)
		{	if(str[i] == ' ') return false;
		}
		return true;
	}

	static inline void replace_string(char * str, char cFrom, char to)
	{
		size_t len = strlen(str);
		for (size_t i = 0; i < len; i++)
		{	if (str[i] == cFrom) str[i] = to;
		}
	}

	static inline direction calc_direction(short sX, short sY, short dX, short dY)
	{
		double tmp1, tmp2, tmp3;
		if ((sX == dX) && (sY == dY)) return direction::north;
		if ((sX == dX) && (sY != dY))
		{	if (sY > dY) return direction::north;
			else return direction::south;
		}
		if ((sX != dX) && (sY == dY))
		{	if (sX > dX) return direction::west;
			else return direction::east;
		}
		tmp1 = static_cast<double>(dX - sX);
		tmp2 = static_cast<double>(dY - sY);
		tmp3 = tmp1 / tmp2;
		if (tmp3 < -3)
		{	if (sX > dX) return direction::west;
			else return direction::east;
		}
		if (tmp3 > 3)
		{	if (sX > dX) return direction::west;
			else return direction::east;
		}
		if ((tmp3 > -0.3333f) && (tmp3 <= 0.3333f))
		{	if (sY > dY) return direction::north;
			else return direction::south;
		}
		if ((tmp3 > 0.3333f) && (tmp3 <= 3.0f))
		{	if (sX > dX) return direction::northwest;
			else return direction::southeast;
		}
		if ((tmp3 >= -0.3333f) && (tmp3 < 3.0f))
		{	if (sX > dX) return direction::west;
			else return direction::east;
		}
		if ((tmp3 >= -3.0f) && (tmp3 < -0.3333f))
		{	if (sX > dX) return direction::southwest;
			else return direction::northeast;
		}
		return direction::north;
	}

	static inline bool check_valid_name(const char *str)
	{
		size_t i, len;
		len = strlen(str);
		for (i = 0; i < len; i++)
		{	if ( static_cast<unsigned char>(str[i]) >= 128 )	return false;
			if ( (str[i] == ',')  || (str[i] == '=') || (str[i] == ' ')  || (str[i] == '\n') ||
				 (str[i] == '\t') || (str[i] == '.') || (str[i] == '\\') || (str[i] == '/')  ||
				 (str[i] == ':')  || (str[i] == '*') || (str[i] == '?')  || (str[i] == '<')  ||
				 (str[i] == '>')  || (str[i] == '|') || (str[i] == '"')  || (str[i] == '`')  ||
				 (str[i] == ';')  || (str[i] == '=') || (str[i] == '@')  || (str[i] == '[')  ||
				 (str[i] == ']')  || (str[i] == '^') || (str[i] == '_')  || (str[i] == '\'') ) return false;
			if( (str[i] < '0') || (str[i] > 'z')) return false;
		}
		return true;
	}

	static inline bool is_valid_email(const char *str)
	{
		size_t len = strlen( str );
		if( len < 7 ) return false;
		bool hasAt = false;
		bool hasDot = false;
		for( size_t i = 0; i < len; i++ )
		{
			if( str[i] == '@' ) hasAt = true;
			if( str[i] == '.' ) hasDot = true;
		}
		return hasAt && hasDot;
	}
}
