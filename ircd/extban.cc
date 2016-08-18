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
 */

namespace chan = ircd::chan;
namespace mode = chan::mode;
namespace ext = mode::ext;
using namespace ext;

ext::func ext::table[256] = { NULL };

int
chan::match_extban(const char *banstr, client *client_p, chan *chptr, long mode_type)
{
	const char *p;
	int invert(0), result(INVALID);
	func f;

	if (*banstr != '$')
		return 0;
	p = banstr + 1;
	if (*p == '~')
	{
		invert = 1;
		p++;
	}
	f = table[uint8_t(rfc1459::tolower(*p))];
	if (*p != '\0')
	{
		p++;
		if (*p == ':')
			p++;
		else
			p = NULL;
	}
	if (f != NULL)
		result = f(p, client_p, chptr, mode::type(mode_type));
	else
		result = INVALID;

	if (invert)
		return result == NOMATCH;
	else
		return result == MATCH;
}

int
chan::valid_extban(const char *banstr, client *client_p, chan *chptr, long mode_type)
{
	const char *p;
	int result(INVALID);
	func f;

	if (*banstr != '$')
		return 0;
	p = banstr + 1;
	if (*p == '~')
		p++;
	f = table[uint8_t(rfc1459::tolower(*p))];
	if (*p != '\0')
	{
		p++;
		if (*p == ':')
			p++;
		else
			p = NULL;
	}
	if (f != NULL)
		result = f(p, client_p, chptr, mode::type(mode_type));
	else
		result = INVALID;

	return result != INVALID;
}

const char *
chan::get_extban_string(void)
{
	static char e[256];
	int i, j;

	j = 0;
	for (i = 1; i < 256; i++)
		if (i == rfc1459::tolower(i) && table[i])
			e[j++] = i;
	e[j] = 0;
	return e;
}
