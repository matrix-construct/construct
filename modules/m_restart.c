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
#include "hash.h"

static int mo_restart(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static int me_restart(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static int do_restart(struct Client *source_p, const char *servername);

struct Message restart_msgtab = {
	"RESTART", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_restart, 1}, {mo_restart, 0}}
};

mapi_clist_av1 restart_clist[] = { &restart_msgtab, NULL };
DECLARE_MODULE_AV1(restart, NULL, NULL, restart_clist, NULL, NULL, "$Revision: 3161 $");

/*
 * mo_restart
 *
 */
static int
mo_restart(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
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

	if(parc > 2)
	{
		/* Remote restart. Pass it along. */
		struct Client *server_p = find_server(NULL, parv[2]);
		if (!server_p)
		{
			sendto_one_numeric(source_p, ERR_NOSUCHSERVER, form_str(ERR_NOSUCHSERVER), parv[2]);
			return 0;
		}

		if (!IsMe(server_p))
		{
			sendto_one(server_p, ":%s ENCAP %s RESTART %s", source_p->name, parv[2], parv[1]);
			return 0;
		}
	}

	return do_restart(source_p, parv[1]);
}

static int
me_restart(struct MsgBuf *msgbuf_p __unused, struct Client *client_p __unused, struct Client *source_p, int parc, const char *parv[])
{
	if(!find_shared_conf(source_p->username, source_p->host, source_p->servptr->name, SHARED_DIE))
	{
		sendto_one_notice(source_p, ":*** You do not have an appropriate shared block to "
				"remotely restart this server.");
		return 0;
	}

	return do_restart(source_p, parv[1]);
}

static int
do_restart(struct Client *source_p, const char *servername)
{
	char buf[BUFSIZE];
	rb_dlink_node *ptr;
	struct Client *target_p;

	if(irccmp(servername, me.name))
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

	sprintf(buf, "Server RESTART by %s", get_client_name(source_p, HIDE_IP));
	restart(buf);

	return 0;
}
