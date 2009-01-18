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
 *
 *   $Id: m_findforwards.c 986 2006-03-08 00:10:46Z jilles $
 */
#include "stdinc.h"
#include "channel.h"
#include "client.h"
#include "hash.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "s_user.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "packet.h"

static int m_findforwards(struct Client *client_p, struct Client *source_p,
			int parc, const char *parv[]);

struct Message findforwards_msgtab = {
	"FINDFORWARDS", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_findforwards, 2}, mg_ignore, mg_ignore, mg_ignore, {m_findforwards, 2}}
};

mapi_clist_av1 findforwards_clist[] = { &findforwards_msgtab, NULL };

DECLARE_MODULE_AV1(findforwards, NULL, NULL, findforwards_clist, NULL, NULL, "$Revision: 986 $");

/*
** mo_findforwards
**      parv[1] = channel
*/
static int
m_findforwards(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
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
			return 0;
		}

		if(!is_chanop(msptr))
		{
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
					me.name, source_p->name, parv[1]);
			return 0;
		}

		if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
		{
			sendto_one(source_p, form_str(RPL_LOAD2HI),
					me.name, source_p->name, "FINDFORWARDS");
			return 0;
		}
		else
			last_used = rb_current_time();
	}
	
	RB_DLINK_FOREACH(ptr, global_channel_list.head)
	{
		chptr = ptr->data;
		if(chptr->mode.forward && !irccmp(chptr->mode.forward, parv[1]))
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

	return 0;
}
