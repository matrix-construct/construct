/**
 *	Copyright (C) 2014 Jason Volk
 *	Copyright (C) 2014 Svetlana Tkachenko
 *	Copyright (C) 2016 Charybdis Development Team
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#define HAVE_IRCD_COLOR_H

namespace ircd::color
{
	enum mode
	:uint8_t
	{
		OFF       = 0x0f,
		BOLD      = 0x02,
		COLOR     = 0x03,
		ITALIC    = 0x09,
		STRIKE    = 0x13,
		UNDER     = 0x15,
		UNDER2    = 0x1f,
		REVERSE   = 0x16,
	};

	enum class fg;
	enum class bg;
}

enum class ircd::color::fg
{
	WHITE,    BLACK,      BLUE,      GREEN,
	LRED,     RED,        MAGENTA,   ORANGE,
	YELLOW,   LGREEN,     CYAN,      LCYAN,
	LBLUE,    LMAGENTA,   GRAY,      LGRAY
};

enum class ircd::color::bg
{
	LGRAY_BLINK,     BLACK,           BLUE,          GREEN,
	RED_BLINK,       RED,             MAGENTA,       ORANGE,
	ORANGE_BLINK,    GREEN_BLINK,     CYAN,          CYAN_BLINK,
	BLUE_BLINK,      MAGENTA_BLINK,   BLACK_BLINK,   LGRAY,
};
