/*   contrib/m_opme.c
 *   Copyright (C) 2002 Hybrid Development Team
 *   Copyright (C) 2004 ircd-ratbox development team
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <ircd/stdinc.h>
#include <ircd/channel.h>
#include <ircd/client.h>
#include <ircd/ircd.h>
#include <ircd/numeric.h>
#include <ircd/logger.h>
#include <ircd/s_serv.h>
#include <ircd/send.h>
#include <ircd/whowas.h>
#include <ircd/match.h>
#include <ircd/hash.h>
#include <ircd/msg.h>
#include <ircd/parse.h>
#include <ircd/modules.h>
#include <ircd/s_conf.h>
#include <ircd/s_newconf.h>
#include <ircd/messages.h>

static const char opme_desc[] = "Allow admins to op themselves on opless channels";

static void mo_opme(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message opme_msgtab = {
	"OPME", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_opme, 2}}
};

mapi_clist_av1 opme_clist[] = { &opme_msgtab, NULL };

DECLARE_MODULE_AV2(opme, NULL, NULL, opme_clist, NULL, NULL, NULL, NULL, opme_desc);

/*
** mo_opme
**      parv[1] = channel
*/
static void
mo_opme(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr;
	struct membership *msptr;
	rb_dlink_node *ptr;

	/* admins only */
	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "admin");
		return;
	}

	if((chptr = find_channel(parv[1])) == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[1]);
		return;
	}

	RB_DLINK_FOREACH(ptr, chptr->members.head)
	{
		msptr = (membership *)ptr->data;

		if(is_chanop(msptr))
		{
			sendto_one_notice(source_p, ":%s Channel is not opless", parv[1]);
			return;
		}
	}

	msptr = find_channel_membership(chptr, source_p);

	if(msptr == NULL)
		return;

	msptr->flags |= CHFL_CHANOP;

	sendto_wallops_flags(UMODE_WALLOP, &me,
			     "OPME called for [%s] by %s!%s@%s",
			     parv[1], source_p->name, source_p->username, source_p->host);
	ilog(L_MAIN, "OPME called for [%s] by %s",
	     parv[1], get_oper_name(source_p));

	/* dont send stuff for local channels remotely. */
	if(*chptr->chname != '&')
	{
		sendto_server(NULL, NULL, NOCAPS, NOCAPS,
			      ":%s WALLOPS :OPME called for [%s] by %s!%s@%s",
			      me.name, parv[1], source_p->name, source_p->username, source_p->host);
		sendto_server(NULL, chptr, CAP_TS6, NOCAPS, ":%s PART %s", source_p->id, parv[1]);
		sendto_server(NULL, chptr, CAP_TS6, NOCAPS,
			      ":%s SJOIN %ld %s + :@%s",
			      me.id, (long) chptr->channelts, parv[1], source_p->id);
	}

	sendto_channel_local(ALL_MEMBERS, chptr,
			     ":%s MODE %s +o %s", me.name, parv[1], source_p->name);
}
