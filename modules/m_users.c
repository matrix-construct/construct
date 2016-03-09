/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_users.c: Gives some basic user statistics.
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
 */

#include "stdinc.h"
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"
#include "s_conf.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static const char users_desc[] =
	"Provides the USERS command to display connection statistics locally and globally";

static void m_users(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message users_msgtab = {
	"USERS", 0, 0, 0, 0,
	{mg_unreg, {m_users, 0}, {m_users, 0}, mg_ignore, mg_ignore, {m_users, 0}}
};

mapi_clist_av1 users_clist[] = { &users_msgtab, NULL };

DECLARE_MODULE_AV2(users, NULL, NULL, users_clist, NULL, NULL, NULL, NULL, users_desc);

/*
 * m_users
 *      parv[1] = servername
 */
static void
m_users(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(hunt_server(client_p, source_p, ":%s USERS :%s", 1, parc, parv) == HUNTED_ISME)
	{
		sendto_one_numeric(source_p, RPL_LOCALUSERS,
				   form_str(RPL_LOCALUSERS),
				   (int)rb_dlink_list_length(&lclient_list),
				   Count.max_loc,
				   (int)rb_dlink_list_length(&lclient_list),
				   Count.max_loc);

		sendto_one_numeric(source_p, RPL_GLOBALUSERS,
				   form_str(RPL_GLOBALUSERS),
				   Count.total, Count.max_tot,
				   Count.total, Count.max_tot);
	}
}
