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
#include "ipv4_from_ipv6.h"

ExtbanFunc extban_table[256] = { NULL };

int
match_child(const char *banstr, struct Client *client_p, struct Channel *chptr, long mode_type)
{
	char src_host[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_iphost[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_althost[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_ip4host[NICKLEN + USERLEN + HOSTLEN + 6];
	struct sockaddr_in ip4;
	char *s = src_host, *s2 = src_iphost, *s3 = NULL, *s4 = NULL;

	if (*banstr == '$')
		return match_extban(banstr, client_p, chptr, mode_type) ? EXTBAN_MATCH : EXTBAN_NOMATCH;

	rb_sprintf(src_host, "%s!%s@%s", client_p->name, client_p->username, client_p->host);
	rb_sprintf(src_iphost, "%s!%s@%s", client_p->name, client_p->username, client_p->sockhost);

	/* handle hostmangling if necessary */
	if (client_p->localClient->mangledhost != NULL)
	{
		if (!strcmp(client_p->host, client_p->localClient->mangledhost))
			rb_sprintf(src_althost, "%s!%s@%s", client_p->name, client_p->username, client_p->orighost);
		else if (!IsDynSpoof(client_p))
			rb_sprintf(src_althost, "%s!%s@%s", client_p->name, client_p->username, client_p->localClient->mangledhost);

		s3 = src_althost;
	}

#ifdef RB_IPV6
	/* handle Teredo if necessary */
	if (client_p->localClient->ip.ss_family == AF_INET6 && ipv4_from_ipv6((const struct sockaddr_in6 *) &client_p->localClient->ip, &ip4))
	{
		rb_sprintf(src_ip4host, "%s!%s@", client_p->name, client_p->username);
		s4 = src_ip4host + strlen(src_ip4host);
		rb_inet_ntop_sock((struct sockaddr *)&ip4,
				s4, src_ip4host + sizeof src_ip4host - s4);
		s4 = src_ip4host;
	}
#endif

	return match(banstr, s) || match(banstr, s2) || (s3 != NULL && match(banstr, s3)) || (s4 != NULL && match(banstr, s4)) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}

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
