/*
 *   IRC - Internet Relay Chat, contrib/m_clearchan.c
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
 *   $Id: m_clearchan.c 1425 2006-05-23 16:41:33Z jilles $
 */
#include "stdinc.h"
#include "tools.h"
#include "channel.h"
#include "client.h"
#include "hash.h"
#include "irc_string.h"
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

static int mo_clearchan(struct Client *client_p, struct Client *source_p,
			int parc, const char *parv[]);

struct Message clearchan_msgtab = {
	"CLEARCHAN", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_clearchan, 2}}
};

mapi_clist_av1 clearchan_clist[] = { &clearchan_msgtab, NULL };

DECLARE_MODULE_AV1(clearchan, NULL, NULL, clearchan_clist, NULL, NULL, "$Revision: 1425 $");

/*
** mo_clearchan
**      parv[0] = sender prefix
**      parv[1] = channel
*/
static int
mo_clearchan(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr;
	struct membership *msptr;
	struct Client *target_p;
	dlink_node *ptr;
	dlink_node *next_ptr;

	/* admins only */
	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, ":%s NOTICE %s :You have no A flag", me.name, parv[0]);
		return 0;
	}


	if((chptr = find_channel(parv[1])) == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[1]);
		return 0;
	}

	if(IsMember(source_p, chptr))
	{
		sendto_one(source_p, ":%s NOTICE %s :*** Please part %s before using CLEARCHAN",
			   me.name, source_p->name, parv[1]);
		return 0;
	}

	/* quickly make everyone a peon.. */
	DLINK_FOREACH(ptr, chptr->members.head)
	{
		msptr = ptr->data;
		msptr->flags &= ~CHFL_CHANOP | CHFL_VOICE;
	}

	sendto_wallops_flags(UMODE_WALLOP, &me,
			     "CLEARCHAN called for [%s] by %s!%s@%s",
			     parv[1], source_p->name, source_p->username, source_p->host);
	ilog(L_MAIN, "CLEARCHAN called for [%s] by %s!%s@%s",
	     parv[1], source_p->name, source_p->username, source_p->host);

	if(*chptr->chname != '&')
	{
		sendto_server(NULL, NULL, NOCAPS, NOCAPS,
			      ":%s WALLOPS :CLEARCHAN called for [%s] by %s!%s@%s",
			      me.name, parv[1], source_p->name, source_p->username, source_p->host);

		/* SJOIN the user to give them ops, and lock the channel */
		sendto_server(client_p, chptr, NOCAPS, NOCAPS,
			      ":%s SJOIN %ld %s +ntsi :@%s",
			      me.name, (long) (chptr->channelts - 1),
			      chptr->chname, source_p->name);
	}

	sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s JOIN %s",
			     source_p->name, source_p->username, source_p->host, chptr->chname);
	sendto_channel_local(ALL_MEMBERS, chptr, ":%s MODE %s +o %s",
			     me.name, chptr->chname, source_p->name);

	add_user_to_channel(chptr, source_p, CHFL_CHANOP);

	/* Take the TS down by 1, so we don't see the channel taken over
	 * again. */
	if(chptr->channelts)
		chptr->channelts--;

	chptr->mode.mode = MODE_SECRET | MODE_TOPICLIMIT | MODE_INVITEONLY | MODE_NOPRIVMSGS;
	chptr->mode.key[0] = '\0';

	DLINK_FOREACH_SAFE(ptr, next_ptr, chptr->members.head)
	{
		msptr = ptr->data;
		target_p = msptr->client_p;

		/* skip the person we just added.. */
		if(is_chanop(msptr))
			continue;

		sendto_channel_local(ALL_MEMBERS, chptr,
				     ":%s KICK %s %s :CLEARCHAN",
				     source_p->name, chptr->chname, target_p->name);

		if(*chptr->chname != '&')
			sendto_server(NULL, chptr, NOCAPS, NOCAPS,
				      ":%s KICK %s %s :CLEARCHAN",
				      source_p->name, chptr->chname, target_p->name);

		remove_user_from_channel(msptr);
	}

	/* Join the user themselves to the channel down here, so they dont see a nicklist 
	 * or people being kicked */
	sendto_one(source_p, ":%s!%s@%s JOIN %s",
		   source_p->name, source_p->username, source_p->host, chptr->chname);

	channel_member_names(chptr, source_p, 1);

	return 0;
}
