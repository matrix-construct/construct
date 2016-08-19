/*
 *  charybdis: An advanced ircd.
 *  chmode.h: The ircd channel header.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2008-2016 Charybdis Development Team
 *  Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

#pragma once
#define HAVE_IRCD_CHMODE_H

#ifdef __cplusplus
namespace ircd {

//TODO: XXX: is actually used for umode and chmode etc
enum : int
{
	MODE_DEL    = -1,
	MODE_QUERY  = 0,
	MODE_ADD    = 1,
};

namespace chan {

struct chan;
using client = Client; //TODO: XXX: temp

namespace mode {

// Maximum mode changes allowed per client, per server is different
constexpr auto MAXPARAMS = 4;
constexpr auto MAXPARAMSSERV = 10;
constexpr auto BUFLEN = 200;

// something not included in messages.tab to change some hooks behaviour when needed -- dwr
constexpr auto ERR_CUSTOM = 1000;

// Channel mode classification
enum class category
{
	A,    // Mode has a parameter apropos a list (or no param for xfer)
	B,    // Always has a parameter
	C,    // Only has a parameter on MODE_ADD
	D,    // Never has a parameter
};

enum type : uint
{
	PRIVATE     = 0x00000001,
	SECRET      = 0x00000002,
	MODERATED   = 0x00000004,
	TOPICLIMIT  = 0x00000008,
	INVITEONLY  = 0x00000010,
	NOPRIVMSGS  = 0x00000020,
	REGONLY     = 0x00000040,
	EXLIMIT     = 0x00000100,  // exempt from list limits, +b/+e/+I/+q
	PERMANENT   = 0x00000200,  // permanant channel, +P
	OPMODERATE  = 0x00000400,  // send rejected messages to ops
	FREEINVITE  = 0x00000800,  // allow free use of /invite
	FREETARGET  = 0x00001000,  // can be forwarded to without authorization
	DISFORWARD  = 0x00002000,  // disable channel forwarding
	BAN         = 0x10000000,
	EXCEPTION   = 0x20000000,
	INVEX       = 0x40000000,
	QUIET       = 0x80000000,
};

struct letter
{
	enum type type  = (enum type)0;
	char letter     = '\0';
};

struct change
{
	char letter      = '\0';
	const char *arg  = nullptr;
	const char *id   = nullptr;
	int dir          = 0;
	int mems         = 0;
};

using func = void (*)(client *, struct chan *, int alevel, int parc, int *parn, const char **parv, int *errors, int dir, char c, type type);

struct mode
{
	enum type type;
	enum category category;
	func set_func;
};

extern mode table[256];
extern char arity[2][256];
extern char categories[4][256];

namespace ext
{
	// extban function results
	enum result
	{
		INVALID   = -1,  // invalid mask, false even if negated
		NOMATCH   = 0,   // valid mask, no match
		MATCH     = 1,   // matches
	};

	using func = int (*)(const char *data, client *, chan *, type type);
	extern func table[256];
}

#define CHM_FUNCTION(_NAME_)                                      \
void _NAME_(client *source_p, chan *chptr,                     \
            int alevel, int parc, int *parn, const char **parv,   \
            int *errors, int dir, char c, type type);

namespace functor
{
	CHM_FUNCTION(nosuch)
	CHM_FUNCTION(orphaned)
	CHM_FUNCTION(simple)
	CHM_FUNCTION(ban)
	CHM_FUNCTION(hidden)
	CHM_FUNCTION(staff)
	CHM_FUNCTION(forward)
	CHM_FUNCTION(throttle)
	CHM_FUNCTION(key)
	CHM_FUNCTION(limit)
	CHM_FUNCTION(op)
	CHM_FUNCTION(voice)
}

type add(const uint8_t &c, const category &category, const func &set_func);
void orphan(const uint8_t &c);
void init(void);

}      // namespace mode
}      // namespace chan
}      // namespace ircd
#endif // __cplusplus
