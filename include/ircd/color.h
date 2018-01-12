// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

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
