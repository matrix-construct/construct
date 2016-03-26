/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_whois.c: Shows who a user was.
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
#include "whowas.h"
#include "client.h"
#include "hash.h"
#include "match.h"
#include "ircd.h"
#include "ircd_defs.h"
#include "numeric.h"
#include "s_serv.h"
#include "s_user.h"
#include "send.h"
#include "s_conf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static const char whowas_desc[] =
	"Provides the WHOWAS command to display information on a disconnected user";

static void m_whowas(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message whowas_msgtab = {
	"WHOWAS", 0, 0, 0, 0,
	{mg_unreg, {m_whowas, 2}, {m_whowas, 4}, mg_ignore, mg_ignore, {m_whowas, 2}}
};

mapi_clist_av1 whowas_clist[] = { &whowas_msgtab, NULL };

DECLARE_MODULE_AV2(whowas, NULL, NULL, whowas_clist, NULL, NULL, NULL, NULL, whowas_desc);

/*
** m_whowas
**      parv[1] = nickname queried
*/
static void
m_whowas(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	rb_dlink_list *whowas_list;
	rb_dlink_node *ptr;
	int cur = 0;
	int max = -1;
	char *p;
	const char *nick;
	char tbuf[26];
	long sendq_limit;

	static time_t last_used = 0L;

	if(MyClient(source_p) && !IsOper(source_p))
	{
		if(last_used + (parc > 3 ? ConfigFileEntry.pace_wait :
						ConfigFileEntry.pace_wait_simple
				) > rb_current_time())
		{
			sendto_one(source_p, form_str(RPL_LOAD2HI),
				   me.name, source_p->name, "WHOWAS");
			sendto_one_numeric(source_p, RPL_ENDOFWHOWAS, form_str(RPL_ENDOFWHOWAS),
				   parv[1]);
			return;
		}
		else
			last_used = rb_current_time();
	}


	if(parc > 2)
		max = atoi(parv[2]);

	if(parc > 3)
		if(hunt_server(client_p, source_p, ":%s WHOWAS %s %s :%s", 3, parc, parv))
			return;

	if(!MyClient(source_p) && (max <= 0 || max > 20))
		max = 20;

	if((p = strchr(parv[1], ',')))
		*p = '\0';

	nick = parv[1];

	sendq_limit = get_sendq(client_p) * 9 / 10;
	whowas_list = whowas_get_list(nick);

	if(whowas_list == NULL)
	{
		sendto_one_numeric(source_p, ERR_WASNOSUCHNICK, form_str(ERR_WASNOSUCHNICK), nick);
		sendto_one_numeric(source_p, RPL_ENDOFWHOWAS, form_str(RPL_ENDOFWHOWAS), parv[1]);
		return;
	}

	RB_DLINK_FOREACH(ptr, whowas_list->head)
	{
		struct Whowas *temp = ptr->data;
		if(cur > 0 && rb_linebuf_len(&client_p->localClient->buf_sendq) > sendq_limit)
		{
			sendto_one(source_p, form_str(ERR_TOOMANYMATCHES),
				   me.name, source_p->name, "WHOWAS");
			break;
		}

		sendto_one(source_p, form_str(RPL_WHOWASUSER),
			   me.name, source_p->name, temp->name,
			   temp->username, temp->hostname, temp->realname);
		if (!EmptyString(temp->sockhost) &&
				strcmp(temp->sockhost, "0") &&
				show_ip_whowas(temp, source_p))
			sendto_one_numeric(source_p, RPL_WHOISACTUALLY,
					   form_str(RPL_WHOISACTUALLY),
					   temp->name, temp->sockhost);

		if (!EmptyString(temp->suser))
			sendto_one_numeric(source_p, RPL_WHOISLOGGEDIN,
					   "%s %s :was logged in as",
					   temp->name, temp->suser);

		sendto_one_numeric(source_p, RPL_WHOISSERVER,
				   form_str(RPL_WHOISSERVER),
				   temp->name, temp->servername,
				   rb_ctime(temp->logoff, tbuf, sizeof(tbuf)));

		cur++;
		if(max > 0 && cur >= max)
			break;
	}

	sendto_one_numeric(source_p, RPL_ENDOFWHOWAS, form_str(RPL_ENDOFWHOWAS), parv[1]);
}
