/*
 *  charybdis: A slightly useful ircd.
 *  extban.c: extended ban types ($type:data)
 *
 * Copyright (C) 2006 charybdis development team
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
 *
 *  $Id: extban.c 1389 2006-05-20 19:19:00Z nenolod $
 */

#include "stdinc.h"
#include "channel.h"
#include "client.h"
#include "common.h"

ExtbanFunc extban_table[256] = { NULL };

int
match_extban(const char *banstr, struct Client *client_p, struct Channel *chptr, long mode_type)
{
	const char *p;
	int invert = 0, result = EXTBAN_INVALID;
	ExtbanFunc f;

	if (*banstr != '$')
		return 0;
	p = banstr + 1;
	if (*p == '~')
	{
		invert = 1;
		p++;
	}
	f = extban_table[(unsigned char) ToLower(*p)];
	if (*p != '\0')
	{
		p++;
		if (*p == ':')
			p++;
		else
			p = NULL;
	}
	if (f != NULL)
		result = f(p, client_p, chptr, mode_type);
	else
		result = EXTBAN_INVALID;

	if (invert)
		return result == EXTBAN_NOMATCH;
	else
		return result == EXTBAN_MATCH;
}

int
valid_extban(const char *banstr, struct Client *client_p, struct Channel *chptr, long mode_type)
{
	const char *p;
	int result = EXTBAN_INVALID;
	ExtbanFunc f;

	if (*banstr != '$')
		return 0;
	p = banstr + 1;
	if (*p == '~')
		p++;
	f = extban_table[(unsigned char) ToLower(*p)];
	if (*p != '\0')
	{
		p++;
		if (*p == ':')
			p++;
		else
			p = NULL;
	}
	if (f != NULL)
		result = f(p, client_p, chptr, mode_type);
	else
		result = EXTBAN_INVALID;

	return result != EXTBAN_INVALID;
}

const char *
get_extban_string(void)
{
	static char e[256];
	int i, j;

	j = 0;
	for (i = 1; i < 256; i++)
		if (i == ToLower(i) && extban_table[i])
			e[j++] = i;
	e[j] = 0;
	return e;
}
