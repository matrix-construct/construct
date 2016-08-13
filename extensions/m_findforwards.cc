/*
 *   IRC - Internet Relay Chat, contrib/m_findforwards.c
 *   Copyright (C) 2002 Hybrid Development Team
 *   Copyright (C) 2004 ircd-ratbox Development Team
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
#include <ircd/hash.h>
#include <ircd/match.h>
#include <ircd/ircd.h>
#include <ircd/numeric.h>
#include <ircd/s_user.h>
#include <ircd/s_conf.h>
#include <ircd/s_newconf.h>
#include <ircd/send.h>
#include <ircd/msg.h>
#include <ircd/parse.h>
#include <ircd/modules.h>
#include <ircd/packet.h>
#include <ircd/messages.h>

using namespace ircd;

static const char findfowards_desc[] = "Allows operators to find forwards to a given channel";

static void m_findforwards(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p,
			int parc, const char *parv[]);

struct Message findforwards_msgtab = {
	"FINDFORWARDS", 0, 0, 0, 0,
	{mg_unreg, {m_findforwards, 2}, mg_ignore, mg_ignore, mg_ignore, {m_findforwards, 2}}
};

mapi_clist_av1 findforwards_clist[] = { &findforwards_msgtab, NULL };

DECLARE_MODULE_AV2(findforwards, NULL, NULL, findforwards_clist, NULL, NULL, NULL, NULL, findfowards_desc);

/*
** mo_findforwards
**      parv[1] = channel
*/
static void
m_findforwards(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0;
	struct Channel *chptr;
	struct membership *msptr;
	rb_dlink_node *ptr;
	char buf[414];
	char *p = buf, *end = buf + sizeof buf - 1;
	*p = '\0';

	/* Allow ircops to search for forwards to nonexistent channels */
	if(!IsOper(source_p))
	{
		if((chptr = find_channel(parv[1])) == NULL || (msptr = find_channel_membership(chptr, source_p)) == NULL)
		{
			sendto_one_numeric(source_p, ERR_NOTONCHANNEL,
					form_str(ERR_NOTONCHANNEL), parv[1]);
			return;
		}

		if(!is_chanop(msptr))
		{
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
					me.name, source_p->name, parv[1]);
			return;
		}

		if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
		{
			sendto_one(source_p, form_str(RPL_LOAD2HI),
					me.name, source_p->name, "FINDFORWARDS");
			return;
		}
		else
			last_used = rb_current_time();
	}

	RB_DLINK_FOREACH(ptr, global_channel_list.head)
	{
		chptr = (Channel *)ptr->data;
		if(!irccmp(chptr->mode.forward, parv[1]))
		{
			if(p + strlen(chptr->chname) >= end - 13)
			{
				strcpy(p, "<truncated> ");
				p += 12;
				break;
			}
			strcpy(p, chptr->chname);
			p += strlen(chptr->chname);
			*p++ = ' ';
		}
	}

	if(buf[0])
		*(--p) = '\0';

	sendto_one_notice(source_p, ":Forwards for %s: %s", parv[1], buf);
}
