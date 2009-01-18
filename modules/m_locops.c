/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_locops.c: Sends a message to all operators on the local server.
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
 *  $Id: m_locops.c 254 2005-09-21 23:35:12Z nenolod $
 */

#include "stdinc.h"
#include "client.h"
#include "ircd.h"
#include "match.h"
#include "numeric.h"
#include "send.h"
#include "s_user.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "hash.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_serv.h"

static int m_locops(struct Client *, struct Client *, int, const char **);
static int ms_locops(struct Client *, struct Client *, int, const char **);
static int me_locops(struct Client *, struct Client *, int, const char **);

struct Message locops_msgtab = {
	"LOCOPS", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, {ms_locops, 3}, mg_ignore, {me_locops, 2}, {m_locops, 2}}
};

mapi_clist_av1 locops_clist[] = { &locops_msgtab, NULL };
DECLARE_MODULE_AV1(locops, NULL, NULL, locops_clist, NULL, NULL, "$Revision: 254 $");

/*
 * m_locops - LOCOPS message handler
 * (write to *all* local opers currently online)
 *      parv[1] = message text
 */
static int
m_locops(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	sendto_wallops_flags(UMODE_LOCOPS, source_p, "LOCOPS - %s", parv[1]);
	
	if(rb_dlink_list_length(&cluster_conf_list) > 0)
		cluster_generic(source_p, "LOCOPS", SHARED_LOCOPS, CAP_CLUSTER,
				":%s", parv[1]);

	return 0;
}

static int
ms_locops(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	/* source_p  parv[1]      parv[2]
	 * oper      target serv  message
	 */
	propagate_generic(source_p, "LOCOPS", parv[1], CAP_CLUSTER, 
				":%s", parv[2]);

	if(!match(parv[1], me.name))
		return 0;

	if(find_shared_conf("*", "*", source_p->servptr->name, SHARED_LOCOPS))
		sendto_wallops_flags(UMODE_LOCOPS, source_p, "SLOCOPS - %s", parv[2]);

	return 0;
}

static int
me_locops(struct Client *client_p, struct Client *source_p,
		int parc, const char *parv[])
{
	if(!IsPerson(source_p))
		return 0;

	if(find_shared_conf("*", "*", source_p->servptr->name, SHARED_LOCOPS))
		sendto_wallops_flags(UMODE_LOCOPS, source_p, "SLOCOPS - %s", parv[1]);

	return 0;
}

