// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_COLOR_H

/// Legacy mIRC color swatch
namespace ircd::color
{
	enum mode :uint8_t;
	enum class fg;
	enum class bg;
}

enum ircd::color::mode
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
