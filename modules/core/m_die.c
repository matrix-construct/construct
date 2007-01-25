/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_die.c: Kills off this server.
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
 *  $Id: m_die.c 98 2005-09-11 03:37:47Z nenolod $
 */

#include "stdinc.h"
#include "tools.h"
#include "client.h"
#include "ircd.h"
#include "irc_string.h"
#include "numeric.h"
#include "commio.h"
#include "s_log.h"
#include "s_conf.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_newconf.h"

static int mo_die(struct Client *, struct Client *, int, const char **);

static struct Message die_msgtab = {
	"DIE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_die, 0}}
};

mapi_clist_av1 die_clist[] = { &die_msgtab, NULL };

DECLARE_MODULE_AV1(die, NULL, NULL, die_clist, NULL, NULL, "$Revision: 98 $");

/*
 * mo_die - DIE command handler
 */
static int
mo_die(struct Client *client_p __unused, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	dlink_node *ptr;

	if(!IsOperDie(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "die");
		return 0;
	}

	if(parc < 2 || EmptyString(parv[1]))
	{
		sendto_one(source_p, ":%s NOTICE %s :Need server name /die %s",
			   me.name, source_p->name, me.name);
		return 0;
	}
	else if(irccmp(parv[1], me.name))
	{
		sendto_one(source_p, ":%s NOTICE %s :Mismatch on /die %s",
			   me.name, source_p->name, me.name);
		return 0;
	}

	DLINK_FOREACH(ptr, lclient_list.head)
	{
		target_p = ptr->data;

		sendto_one(target_p,
			   ":%s NOTICE %s :Server Terminating. %s",
			   me.name, target_p->name, get_client_name(source_p, HIDE_IP));
	}

	DLINK_FOREACH(ptr, serv_list.head)
	{
		target_p = ptr->data;

		sendto_one(target_p, ":%s ERROR :Terminated by %s",
			   me.name, get_client_name(source_p, HIDE_IP));
	}

	/*
	 * XXX we called flush_connections() here. Read server_reboot()
	 * for an explanation as to what we should do.
	 *     -- adrian
	 */
	ilog(L_MAIN, "Server terminated by %s", get_oper_name(source_p));

	/* this is a normal exit, tell the os it's ok */
	unlink(pidFileName);
	exit(0);
	/* NOT REACHED */

	return 0;
}
