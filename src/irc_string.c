/*
 *  ircd-ratbox: A slightly useful ircd.
 *  irc_string.c: IRC string functions.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
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
 *  $Id: irc_string.c 678 2006-02-03 20:25:01Z jilles $
 */

#include "stdinc.h"
#include "sprintf_irc.h"
#include "irc_string.h"
#include "client.h"
#include "setup.h"

#ifndef INADDRSZ
#define INADDRSZ 4
#endif

#ifdef RB_IPV6
#ifndef IN6ADDRSZ
#define IN6ADDRSZ 16
#endif
#endif

#ifndef INT16SZ
#define INT16SZ 2
#endif
/*
 * myctime - This is like standard ctime()-function, but it zaps away
 *   the newline from the end of that string. Also, it takes
 *   the time value as parameter, instead of pointer to it.
 *   Note that it is necessary to copy the string to alternate
 *   buffer (who knows how ctime() implements it, maybe it statically
 *   has newline there and never 'refreshes' it -- zapping that
 *   might break things in other places...)
 *
 *
 * Thu Nov 24 18:22:48 1986 
 */
const char *
myctime(time_t value)
{
	static char buf[32];
	char *p;

	strcpy(buf, ctime(&value));
	if((p = strchr(buf, '\n')) != NULL)
		*p = '\0';
	return buf;
}


/*
 * clean_string - clean up a string possibly containing garbage
 *
 * *sigh* Before the kiddies find this new and exciting way of 
 * annoying opers, lets clean up what is sent to local opers
 * -Dianora
 */
char *
clean_string(char *dest, const unsigned char *src, size_t len)
{
	char *d = dest;
	s_assert(0 != dest);
	s_assert(0 != src);

	if(dest == NULL || src == NULL)
		return NULL;

	len -= 3;		/* allow for worst case, '^A\0' */

	while(*src && (len > 0))
	{
		if(*src & 0x80)	/* if high bit is set */
		{
			*d++ = '.';
			--len;
		}
		else if(!IsPrint(*src))	/* if NOT printable */
		{
			*d++ = '^';
			--len;
			*d++ = 0x40 + *src;	/* turn it into a printable */
		}
		else
			*d++ = *src;
		++src;
		--len;
	}
	*d = '\0';
	return dest;
}

/*
 * strip_tabs(dst, src, length)
 *
 *   Copies src to dst, while converting all \t (tabs) into spaces.
 *
 * NOTE: jdc: I have a gut feeling there's a faster way to do this.
 */
char *
strip_tabs(char *dest, const unsigned char *src, size_t len)
{
	char *d = dest;
	/* Sanity check; we don't want anything nasty... */
	s_assert(0 != dest);
	s_assert(0 != src);

	if(dest == NULL || src == NULL)
		return NULL;

	while(*src && (len > 0))
	{
		if(*src == '\t')
		{
			*d++ = ' ';	/* Translate the tab into a space */
		}
		else
		{
			*d++ = *src;	/* Copy src to dst */
		}
		++src;
		--len;
	}
	*d = '\0';		/* Null terminate, thanks and goodbye */
	return dest;
}

/*
 * strtoken - walk through a string of tokens, using a set of separators
 *   argv 9/90
 *
 */
char *
strtoken(char **save, char *str, const char *fs)
{
	char *pos = *save;	/* keep last position across calls */
	char *tmp;

	if(str)
		pos = str;	/* new string scan */

	while(pos && *pos && strchr(fs, *pos) != NULL)
		++pos;		/* skip leading separators */

	if(!pos || !*pos)
		return (pos = *save = NULL);	/* string contains only sep's */

	tmp = pos;		/* now, keep position of the token */

	while(*pos && strchr(fs, *pos) == NULL)
		++pos;		/* skip content of the token */

	if(*pos)
		*pos++ = '\0';	/* remove first sep after the token */
	else
		pos = NULL;	/* end of string */

	*save = pos;
	return tmp;
}

/*
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#define SPRINTF(x) ((size_t)rb_sprintf x)

/*
 * WARNING: Don't even consider trying to compile this on a system where
 * sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
 */

static const char *inet_ntop4(const u_char * src, char *dst, unsigned int size);
#ifdef RB_IPV6
static const char *inet_ntop6(const u_char * src, char *dst, unsigned int size);
#endif

/* const char *
 * inet_ntop4(src, dst, size)
 *	format an IPv4 address
 * return:
 *	`dst' (as a const)
 * notes:
 *	(1) uses no statics
 *	(2) takes a u_char* not an in_addr as input
 * author:
 *	Paul Vixie, 1996.
 */
static const char *
inet_ntop4(const unsigned char *src, char *dst, unsigned int size)
{
	if(size < 16)
		return NULL;
	return strcpy(dst, inetntoa((const char *) src));
}

/* const char *
 * inet_ntop6(src, dst, size)
 *	convert IPv6 binary address into presentation (printable) format
 * author:
 *	Paul Vixie, 1996.
 */
#ifdef RB_IPV6
static const char *
inet_ntop6(const unsigned char *src, char *dst, unsigned int size)
{
	/*
	 * Note that int32_t and int16_t need only be "at least" large enough
	 * to contain a value of the specified size.  On some systems, like
	 * Crays, there is no such thing as an integer variable with 16 bits.
	 * Keep this in mind if you think this function should have been coded
	 * to use pointer overlays.  All the world's not a VAX.
	 */
	char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
	struct
	{
		int base, len;
	}
	best, cur;
	u_int words[IN6ADDRSZ / INT16SZ];
	int i;

	/*
	 * Preprocess:
	 *      Copy the input (bytewise) array into a wordwise array.
	 *      Find the longest run of 0x00's in src[] for :: shorthanding.
	 */
	memset(words, '\0', sizeof words);
	for(i = 0; i < IN6ADDRSZ; i += 2)
		words[i / 2] = (src[i] << 8) | src[i + 1];
	best.base = -1;
	cur.base = -1;
	for(i = 0; i < (IN6ADDRSZ / INT16SZ); i++)
	{
		if(words[i] == 0)
		{
			if(cur.base == -1)
				cur.base = i, cur.len = 1;
			else
				cur.len++;
		}
		else
		{
			if(cur.base != -1)
			{
				if(best.base == -1 || cur.len > best.len)
					best = cur;
				cur.base = -1;
			}
		}
	}
	if(cur.base != -1)
	{
		if(best.base == -1 || cur.len > best.len)
			best = cur;
	}
	if(best.base != -1 && best.len < 2)
		best.base = -1;

	/*
	 * Format the result.
	 */
	tp = tmp;
	for(i = 0; i < (IN6ADDRSZ / INT16SZ); i++)
	{
		/* Are we inside the best run of 0x00's? */
		if(best.base != -1 && i >= best.base && i < (best.base + best.len))
		{
			if(i == best.base)
			{
				if(i == 0)
					*tp++ = '0';
				*tp++ = ':';
			}
			continue;
		}
		/* Are we following an initial run of 0x00s or any real hex? */
		if(i != 0)
			*tp++ = ':';
		/* Is this address an encapsulated IPv4? */
		if(i == 6 && best.base == 0 &&
		   (best.len == 6 || (best.len == 5 && words[5] == 0xffff)))
		{
			if(!inet_ntop4(src + 12, tp, sizeof tmp - (tp - tmp)))
				return (NULL);
			tp += strlen(tp);
			break;
		}
		tp += SPRINTF((tp, "%x", words[i]));
	}
	/* Was it a trailing run of 0x00's? */
	if(best.base != -1 && (best.base + best.len) == (IN6ADDRSZ / INT16SZ))
		*tp++ = ':';
	*tp++ = '\0';

	/*
	 * Check for overflow, copy, and we're done.
	 */

	if((unsigned int) (tp - tmp) > size)
	{
		return (NULL);
	}
	return strcpy(dst, tmp);
}
#endif

char *
strip_colour(char *string)
{
	char *c = string;
	char *c2 = string;
	char *last_non_space = NULL;
	/* c is source, c2 is target */
	for(; c && *c; c++)
		switch (*c)
		{
		case 3:
			if(isdigit(c[1]))
			{
				c++;
				if(isdigit(c[1]))
					c++;
				if(c[1] == ',' && isdigit(c[2]))
				{
					c += 2;
					if(isdigit(c[1]))
						c++;
				}
			}
			break;
		case 2:
		case 6:
		case 7:
		case 22:
		case 23:
		case 27:
		case 31:
			break;
		case 32:
			*c2++ = *c;
			break;
		default:
			*c2++ = *c;
			last_non_space = c2;
			break;
		}
	*c2 = '\0';
	if(last_non_space)
		*last_non_space = '\0';
	return string;
}
