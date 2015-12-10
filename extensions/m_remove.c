/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_kick.c: Kicks a user from a channel.
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
 *  $Id: m_kick.c 3317 2007-03-28 23:17:06Z jilles $
 */

#include "stdinc.h"
#include "channel.h"
#include "client.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "msg.h"
#include "modules.h"
#include "parse.h"
#include "hash.h"
#include "packet.h"
#include "s_serv.h"
#include "s_conf.h"
#include "hook.h"
#include "messages.h"

unsigned int CAP_REMOVE;

static int m_remove(struct Client *, struct Client *, int, const char **);

struct Message remove_msgtab = {
	"REMOVE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_remove, 3}, {m_remove, 3}, {m_remove, 3}, mg_ignore, {m_remove, 3}}
};

mapi_clist_av1 remove_clist[] = { &remove_msgtab, NULL };

static int
modinit(void)
{
	CAP_REMOVE = capability_put(serv_capindex, "REMOVE");

	return 0;
}

static void
moddeinit(void)
{
	capability_orphan(serv_capindex, "REMOVE");
}

DECLARE_MODULE_AV1(remove, modinit, moddeinit, remove_clist, NULL, NULL, "$Revision: 3317 $");

static int
m_remove(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct membership *msptr;
	struct Client *who;
	struct Channel *chptr;
	int chasing = 0;
	char *comment;
	const char *name;
	char *p = NULL;
	const char *user;
	static char buf[BUFSIZE];

	if(MyClient(source_p) && !IsFloodDone(source_p))
		flood_endgrace(source_p);

	*buf = '\0';
	if((p = strchr(parv[1], ',')))
		*p = '\0';

	name = parv[1];

	chptr = find_channel(name);
	if(chptr == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL, form_str(ERR_NOSUCHCHANNEL), name);
		return 0;
	}

	if(!IsServer(source_p))
	{
		msptr = find_channel_membership(chptr, source_p);

		if((msptr == NULL) && MyConnect(source_p))
		{
			sendto_one_numeric(source_p, ERR_NOTONCHANNEL,
					   form_str(ERR_NOTONCHANNEL), name);
			return 0;
		}

		if(get_channel_access(source_p, msptr, MODE_ADD) < CHFL_CHANOP)
		{
			if(MyConnect(source_p))
			{
				sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
					   me.name, source_p->name, name);
				return 0;
			}

			/* If its a TS 0 channel, do it the old way */
			if(chptr->channelts == 0)
			{
				sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
					   get_id(&me, source_p), get_id(source_p, source_p), name);
				return 0;
			}
		}

		/* Its a user doing a kick, but is not showing as chanop locally
		 * its also not a user ON -my- server, and the channel has a TS.
		 * There are two cases we can get to this point then...
		 *
		 *     1) connect burst is happening, and for some reason a legit
		 *        op has sent a KICK, but the SJOIN hasn't happened yet or
		 *        been seen. (who knows.. due to lag...)
		 *
		 *     2) The channel is desynced. That can STILL happen with TS
		 *
		 *     Now, the old code roger wrote, would allow the KICK to
		 *     go through. Thats quite legit, but lets weird things like
		 *     KICKS by users who appear not to be chanopped happen,
		 *     or even neater, they appear not to be on the channel.
		 *     This fits every definition of a desync, doesn't it? ;-)
		 *     So I will allow the KICK, otherwise, things are MUCH worse.
		 *     But I will warn it as a possible desync.
		 *
		 *     -Dianora
		 */
	}

	if((p = strchr(parv[2], ',')))
		*p = '\0';

	user = parv[2];		/* strtoken(&p2, parv[2], ","); */

	if(!(who = find_chasing(source_p, user, &chasing)))
	{
		return 0;
	}

	msptr = find_channel_membership(chptr, who);

	if(msptr != NULL)
	{
		if(MyClient(source_p) && IsService(who))
		{
			sendto_one(source_p, form_str(ERR_ISCHANSERVICE),
				   me.name, source_p->name, who->name, chptr->chname);
			return 0;
		}

		if(MyClient(source_p))
		{
			hook_data_channel_approval hookdata;

			hookdata.client = source_p;
			hookdata.chptr = chptr;
			hookdata.msptr = msptr;
			hookdata.target = who;
			hookdata.approved = 1;
			hookdata.dir = MODE_ADD;	/* ensure modules like override speak up */

			call_hook(h_can_kick, &hookdata);

			if (!hookdata.approved)
				return 0;
		}

		comment = LOCAL_COPY((EmptyString(parv[3])) ? who->name : parv[3]);
		if(strlen(comment) > (size_t) REASONLEN)
			comment[REASONLEN] = '\0';

		/* jdc
		 * - In the case of a server kicking a user (i.e. CLEARCHAN),
		 *   the kick should show up as coming from the server which did
		 *   the kick.
		 * - Personally, flame and I believe that server kicks shouldn't
		 *   be sent anyways.  Just waiting for some oper to abuse it...
		 */
		sendto_channel_local(ALL_MEMBERS, chptr,
				     ":%s!%s@%s PART %s :requested by %s (%s)",
				     who->name, who->username,
				     who->host, name, source_p->name, comment);

		sendto_server(client_p, chptr, CAP_REMOVE, NOCAPS,
			      ":%s REMOVE %s %s :%s",
			      use_id(source_p), chptr->chname, use_id(who), comment);
		sendto_server(client_p, chptr, NOCAPS, CAP_REMOVE,
			      ":%s KICK %s %s :%s",
			      use_id(source_p), chptr->chname, use_id(who), comment);

		remove_user_from_channel(msptr);
	}
	else if (MyClient(source_p))
		sendto_one_numeric(source_p, ERR_USERNOTINCHANNEL,
				   form_str(ERR_USERNOTINCHANNEL), user, name);

	return 0;
}

