/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_pong.c: The reply to a ping message.
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
 *  $Id: m_pong.c 3181 2007-02-01 00:49:07Z jilles $
 */

#include "stdinc.h"
#include "ircd.h"
#include "s_user.h"
#include "client.h"
#include "hash.h"		/* for find_client() */
#include "hook.h"
#include "numeric.h"
#include "s_conf.h"
#include "send.h"
#include "channel.h"
#include "match.h"
#include "msg.h"
#include "parse.h"
#include "hash.h"
#include "modules.h"

static int mr_pong(struct Client *, struct Client *, int, const char **);
static int ms_pong(struct Client *, struct Client *, int, const char **);

struct Message pong_msgtab = {
	"PONG", 0, 0, 0, MFLG_SLOW | MFLG_UNREG,
	{{mr_pong, 0}, mg_ignore, mg_ignore, {ms_pong, 2}, mg_ignore, mg_ignore}
};

mapi_clist_av1 pong_clist[] = { &pong_msgtab, NULL };
DECLARE_MODULE_AV1(pong, NULL, NULL, pong_clist, NULL, NULL, "$Revision: 3181 $");

static int
ms_pong(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	const char *destination;

	destination = parv[2];
	source_p->flags &= ~FLAGS_PINGSENT;

	/* Now attempt to route the PONG, comstud pointed out routable PING
	 * is used for SPING.  routable PING should also probably be left in
	 *        -Dianora
	 * That being the case, we will route, but only for registered clients (a
	 * case can be made to allow them only from servers). -Shadowfax
	 */
	if(!EmptyString(destination) && !match(destination, me.name) &&
	   irccmp(destination, me.id))
	{
		if((target_p = find_client(destination)))
			sendto_one(target_p, ":%s PONG %s %s", 
				   get_id(source_p, target_p), parv[1], 
				   get_id(target_p, target_p));
		else
		{
			if(!IsDigit(*destination))
				sendto_one_numeric(source_p, ERR_NOSUCHSERVER,
						   form_str(ERR_NOSUCHSERVER), destination);
			return 0;
		}
	}

	/* destination is us, emulate EOB */
	if(IsServer(source_p) && !HasSentEob(source_p))
	{
		if(MyConnect(source_p))
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					     "End of burst (emulated) from %s (%d seconds)",
					     source_p->name,
					     (signed int) (rb_current_time() - source_p->localClient->firsttime));
		SetEob(source_p);
		eob_count++;
		call_hook(h_server_eob, source_p);
	}

	return 0;
}

static int
mr_pong(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(parc == 2 && !EmptyString(parv[1]))
	{
		if(ConfigFileEntry.ping_cookie && source_p->flags & FLAGS_SENTUSER && source_p->name[0])
		{
			unsigned long incoming_ping = strtoul(parv[1], NULL, 16);
			if(incoming_ping)
			{
				if(source_p->localClient->random_ping == incoming_ping)
				{
					char buf[USERLEN + 1];
					rb_strlcpy(buf, source_p->username, sizeof(buf));
					source_p->flags |= FLAGS_PING_COOKIE;
					register_local_user(client_p, source_p, buf);
				}
				else
				{
					sendto_one(source_p, form_str(ERR_WRONGPONG),
						   me.name, source_p->name,
						   source_p->localClient->random_ping);
					return 0;
				}
			}
		}

	}
	else
		sendto_one(source_p, form_str(ERR_NOORIGIN), me.name, source_p->name);

	source_p->flags &= ~FLAGS_PINGSENT;

	return 0;
}
