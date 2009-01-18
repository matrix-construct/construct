/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_ping.c: Requests that a PONG message be sent back.
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
 *  $Id: m_ping.c 254 2005-09-21 23:35:12Z nenolod $
 */

#include "stdinc.h"
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "match.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "hash.h"
#include "s_conf.h"
#include "s_serv.h"

static int m_ping(struct Client *, struct Client *, int, const char **);
static int ms_ping(struct Client *, struct Client *, int, const char **);

struct Message ping_msgtab = {
	"PING", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_ping, 2}, {ms_ping, 2}, {ms_ping, 2}, mg_ignore, {m_ping, 2}}
};

mapi_clist_av1 ping_clist[] = { &ping_msgtab, NULL };
DECLARE_MODULE_AV1(ping, NULL, NULL, ping_clist, NULL, NULL, "$Revision: 254 $");

/*
** m_ping
**      parv[1] = origin
**      parv[2] = destination
*/
static int
m_ping(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	const char *destination;

	destination = parv[2];	/* Will get NULL or pointer (parc >= 2!!) */

	if(!EmptyString(destination) && !match(destination, me.name))
	{
		if((target_p = find_server(source_p, destination)))
		{
			sendto_one(target_p, ":%s PING %s :%s",
				   get_id(source_p, target_p),
				   source_p->name, get_id(target_p, target_p));
		}
		else
		{
			sendto_one_numeric(source_p, ERR_NOSUCHSERVER,
					   form_str(ERR_NOSUCHSERVER),
					   destination);
			return 0;
		}
	}
	else
		sendto_one(source_p, ":%s PONG %s :%s", me.name,
			   (destination) ? destination : me.name, parv[1]);

	return 0;
}

static int
ms_ping(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	const char *destination;

	destination = parv[2];	/* Will get NULL or pointer (parc >= 2!!) */

	if(!EmptyString(destination) && irccmp(destination, me.name) &&
	   irccmp(destination, me.id))
	{
		if((target_p = find_client(destination)) && IsServer(target_p))
			sendto_one(target_p, ":%s PING %s :%s", 
				   get_id(source_p, target_p), source_p->name,
				   get_id(target_p, target_p));
		/* not directed at an id.. */
		else if(!IsDigit(*destination))
			sendto_one_numeric(source_p, ERR_NOSUCHSERVER,
					   form_str(ERR_NOSUCHSERVER),
					   destination);
	}
	else
		sendto_one(source_p, ":%s PONG %s :%s", 
			   get_id(&me, source_p), me.name, 
			   get_id(source_p, source_p));

	return 0;
}
