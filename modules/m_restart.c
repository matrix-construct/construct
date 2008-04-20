/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_restart.c: Exits and re-runs ircd.
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
 *  $Id: m_restart.c 3161 2007-01-25 07:23:01Z nenolod $
 */

#include "stdinc.h"
#include "client.h"
#include "common.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "restart.h"
#include "logger.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static int mo_restart(struct Client *, struct Client *, int, const char **);

struct Message restart_msgtab = {
	"RESTART", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_restart, 0}}
};

mapi_clist_av1 restart_clist[] = { &restart_msgtab, NULL };
DECLARE_MODULE_AV1(restart, NULL, NULL, restart_clist, NULL, NULL, "$Revision: 3161 $");

/*
 * mo_restart
 *
 */
static int
mo_restart(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	char buf[BUFSIZE];
	rb_dlink_node *ptr;
	struct Client *target_p;

	if(!IsOperDie(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "die");
		return 0;
	}

	if(parc < 2 || EmptyString(parv[1]))
	{
		sendto_one_notice(source_p, ":Need server name /restart %s", me.name);
		return 0;
	}
	else if(irccmp(parv[1], me.name))
	{
		sendto_one_notice(source_p, ":Mismatch on /restart %s", me.name);
		return 0;
	}

	RB_DLINK_FOREACH(ptr, lclient_list.head)
	{
		target_p = ptr->data;

		sendto_one_notice(target_p, ":Server Restarting. %s", get_client_name(source_p, HIDE_IP));
	}

	RB_DLINK_FOREACH(ptr, serv_list.head)
	{
		target_p = ptr->data;

		sendto_one(target_p, ":%s ERROR :Restart by %s",
			   me.name, get_client_name(source_p, HIDE_IP));
	}

	rb_sprintf(buf, "Server RESTART by %s", get_client_name(source_p, HIDE_IP));
	restart(buf);

	return 0;
}
