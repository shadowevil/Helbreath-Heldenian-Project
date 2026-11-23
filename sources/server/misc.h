// misc.h: interface for the CMisc namespace.

#pragma once

#include "common_types.h"
#include "game_geometry.h"
#include "direction_helpers.h"
using hb::shared::direction::direction;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace CMisc
{
	static inline direction get_next_move_dir(short sX, short sY, short dX, short dY)
	{
		short absX, absY;
		direction ret = direction{};

		absX = sX - dX;
		absY = sY - dY;

		if ((absX == 0) && (absY == 0)) ret = direction{};

		if (absX == 0) {
			if (absY > 0) ret = direction::north;
			if (absY < 0) ret = direction::south;
		}
		if (absY == 0) {
			if (absX > 0) ret = direction::west;
			if (absX < 0) ret = direction::east;
		}
		if ( (absX > 0)	&& (absY > 0) ) ret = direction::northwest;
		if ( (absX < 0)	&& (absY > 0) ) ret = direction::northeast;
		if ( (absX > 0)	&& (absY < 0) ) ret = direction::southwest;
		if ( (absX < 0)	&& (absY < 0) ) ret = direction::southeast;

		return ret;
	}

	static inline void GetPoint(int x0, int y0, int x1, int y1, int * pX, int * pY, int * error_acc)
	{
		int dx, dy, x_inc, y_inc, error, index;
		int result_x, result_y, dst_cnt;

		if ((x0 == x1) && (y0 == y1)) {
			*pX = x0;
			*pY = y0;
			return;
		}

		error = *error_acc;

		result_x = x0;
		result_y = y0;
		dst_cnt  = 0;

		dx = x1-x0;
		dy = y1-y0;

		if(dx>=0)
		{
			x_inc = 1;
		}
		else
		{
			x_inc = -1;
			dx = -dx;
		}

		if(dy>=0)
		{
			y_inc = 1;
		}
		else
		{
			y_inc = -1;
			dy = -dy;
		}

		if(dx>dy)
		{
			for(index = 0; index<=dx; index++)
			{
				error+=dy;
				if(error>dx)
				{
					error-=dx;
					result_y += y_inc;
				}
				result_x += x_inc;
				break;
			}
		}
		else
		{
			for(index=0; index<=dy; index++)
			{
				error+=dx;
				if(error>0)
				{
					error-=dy;
					result_x += x_inc;
				}
				result_y += y_inc;
				break;
			}
		}

		*pX = result_x;
		*pY = result_y;
		*error_acc = error;
	}

	// Forwards to shared implementation in game_geometry.h
	static inline void GetPoint2(int x0, int y0, int x1, int y1, int* pX, int* pY, int* error_acc, int count)
	{
		hb::shared::geometry::GetPoint2(x0, y0, x1, y1, pX, pY, error_acc, count);
	}

	static inline void GetDirPoint(direction dir, int * pX, int * pY)
	{
		hb::shared::direction::ApplyOffset(dir, *pX, *pY);
	}

	static inline bool check_valid_name(char *str)
	{
		size_t len = strlen(str);
		for(int i = 0; i < len; i++)
		{
			if ( (str[i] == ',')  || (str[i] == '=')  || (str[i] == ' ') ||
				 (str[i] == '\n') || (str[i] == '\t') || /*(str[i] == '.') ||*/
				 (str[i] == '\\') || (str[i] == '/')  || (str[i] == ':') ||
				 (str[i] == '*')  || (str[i] == '?')  || (str[i] == '<') ||
				 (str[i] == '>')  || (str[i] == '|')  || (str[i] == '"') ) return false;

			if ((i <= len-2) && ((unsigned char)str[i] >= 128)) {
				if (((unsigned char)str[i] == 164) && ((unsigned char)str[i+1] >= 161) &&
					((unsigned char)str[i+1] <= 211)) {

				}
				else
				if (((unsigned char)str[i] >= 176) && ((unsigned char)str[i] <= 200) &&
					((unsigned char)str[i+1] >= 161) && ((unsigned char)str[i+1] <= 254)) {

				}
				else return false;
				i++;
			}
		}

		return true;
	}

	static inline void Temp()
	{
		FILE * src_file, * dest_file, * src_file_a, * src_file_b;
		
		char temp[100000];

		src_file = fopen("middleland.amd", "rb");
		dest_file = fopen("middleland.amd.result", "wb");

		src_file_a = fopen("middleland1.amd", "rb");
		src_file_b = fopen("middleland2.amd", "rb");

		fread(temp, 1, 256, src_file);
		fread(temp, 1, 256, src_file_a);
		fread(temp, 1, 256, src_file_b);
		for(int i = 1; i <= 444; i++)
			fread(temp, 1, 5240, src_file_b);

		std::memset(temp, 0, sizeof(temp));
		strcpy(temp, "MAPSIZEX = 824 MAPSIZEY = 824 TILESIZE = 10");

		fwrite(temp, 1, 256, dest_file);

		for(int i = 1; i <= 80; i++) {
			std::memset(temp, 0, sizeof(temp));
			fread((temp + 1500), 1, 5240, src_file_a);
			fwrite(temp, 1, 824*10, dest_file);
		}

		std::memset(temp, 0, sizeof(temp));
		for(int i = 1; i <= 68; i++) fwrite(temp, 1, 824*10, dest_file);

		//148
		/*
		std::memset(temp, 0, sizeof(temp));
		for(int i = 1; i <= 150; i++) fwrite(temp, 1, 824*10, dest_file);
		*/

		for(int i = 1; i <= 524; i++) {
			std::memset(temp, 0, sizeof(temp));
			fread((temp + 1500), 1, 5240, src_file);
			fwrite(temp, 1, 824*10, dest_file);
		}

		std::memset(temp, 0, sizeof(temp));
		for(int i = 1; i <= 68; i++) fwrite(temp, 1, 824*10, dest_file);

		for(int i = 1; i <= 80; i++) {
			std::memset(temp, 0, sizeof(temp));
			fread((temp + 1500), 1, 5240, src_file_b);
			fwrite(temp, 1, 824*10, dest_file);
		}

		std::memset(temp, 0, sizeof(temp));
		for(int i = 1; i <= 2; i++) fwrite(temp, 1, 824*10, dest_file);

		/*
		std::memset(temp, 0, sizeof(temp));
		for(int i = 1; i <= 150; i++) fwrite(temp, 1, 824*10, dest_file);
		*/

		fclose(src_file);
		fclose(dest_file);
		fclose(src_file_a);
		fclose(src_file_b);
	}
}
