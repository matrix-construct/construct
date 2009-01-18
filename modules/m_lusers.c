/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_lusers.c: Sends user statistics.
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
 *  $Id: m_lusers.c 254 2005-09-21 23:35:12Z nenolod $
 */

#include "stdinc.h"
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"		/* hunt_server */
#include "s_user.h"		/* show_lusers */
#include "send.h"
#include "s_conf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static int m_lusers(struct Client *, struct Client *, int, const char **);
static int ms_lusers(struct Client *, struct Client *, int, const char **);

struct Message lusers_msgtab = {
	"LUSERS", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_lusers, 0}, {ms_lusers, 0}, mg_ignore, mg_ignore, {ms_lusers, 0}}
};

mapi_clist_av1 lusers_clist[] = { &lusers_msgtab, NULL };
DECLARE_MODULE_AV1(lusers, NULL, NULL, lusers_clist, NULL, NULL, "$Revision: 254 $");

/*
 * m_lusers - LUSERS message handler
 * parv[1] = host/server mask.
 * parv[2] = server to query
 * 
 * 199970918 JRL hacked to ignore parv[1] completely and require parc > 3
 * to cause a force
 */
static int
m_lusers(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0;

	if (parc > 2)
	{
		if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
		{
			/* safe enough to give this on a local connect only */
			sendto_one(source_p, form_str(RPL_LOAD2HI),
				   me.name, source_p->name, "LUSERS");
			return 0;
		}
		else
			last_used = rb_current_time();

		if(hunt_server(client_p, source_p, ":%s LUSERS %s :%s", 2, parc, parv) !=
			   HUNTED_ISME)
			return 0;
	}

	show_lusers(source_p);

	return 0;
}

/*
 * ms_lusers - LUSERS message handler for servers and opers
 * parv[1] = host/server mask.
 * parv[2] = server to query
 * 
 * 199970918 JRL hacked to ignore parv[1] completely and require parc > 3
 * to cause a force
 */
static int
ms_lusers(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(parc > 2)
	{
		if(hunt_server(client_p, source_p, ":%s LUSERS %s :%s", 2, parc, parv)
		   != HUNTED_ISME)
			return 0;
	}

	show_lusers(source_p);

	return 0;
}
