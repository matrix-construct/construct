/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_knock.c: Requests to be invited to a channel.
 *
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
 *  $Id: m_knock.c 3570 2007-09-09 19:19:23Z jilles $
 */

#include "stdinc.h"
#include "channel.h"
#include "client.h"
#include "hash.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_serv.h"

static int m_knock(struct Client *, struct Client *, int, const char **);

struct Message knock_msgtab = {
	"KNOCK", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_knock, 2}, {m_knock, 2}, mg_ignore, mg_ignore, {m_knock, 2}}
};

mapi_clist_av1 knock_clist[] = { &knock_msgtab, NULL };
DECLARE_MODULE_AV1(knock, NULL, NULL, knock_clist, NULL, NULL, "$Revision: 3570 $");

/* m_knock
 *    parv[1] = channel
 *
 *  The KNOCK command has the following syntax:
 *   :<sender> KNOCK <channel>
 *
 *  If a user is not banned from the channel they can use the KNOCK
 *  command to have the server NOTICE the channel operators notifying
 *  they would like to join.  Helpful if the channel is invite-only, the
 *  key is forgotten, or the channel is full (INVITE can bypass each one
 *  of these conditions.  Concept by Dianora <db@db.net> and written by
 *  <anonymous>
 */
static int
m_knock(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr;
	char *p, *name;

	if(MyClient(source_p) && ConfigChannel.use_knock == 0)
	{
		sendto_one(source_p, form_str(ERR_KNOCKDISABLED),
			   me.name, source_p->name);
		return 0;
	}

	name = LOCAL_COPY(parv[1]);

	/* dont allow one knock to multiple chans */
	if((p = strchr(name, ',')))
		*p = '\0';

	if(!IsChannelName(name))
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), name);
		return 0;
	}

	if((chptr = find_channel(name)) == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), name);
		return 0;
	}

	if(IsMember(source_p, chptr))
	{
		if(MyClient(source_p))
			sendto_one(source_p, form_str(ERR_KNOCKONCHAN),
				   me.name, source_p->name, name);
		return 0;
	}

	if(!((chptr->mode.mode & MODE_INVITEONLY) || (*chptr->mode.key) || 
	     (chptr->mode.limit && 
	      rb_dlink_list_length(&chptr->members) >= (unsigned long)chptr->mode.limit)))
	{
		sendto_one_numeric(source_p, ERR_CHANOPEN,
				   form_str(ERR_CHANOPEN), name);
		return 0;
	}

	/* cant knock to a +p channel */
	if(HiddenChannel(chptr))
	{
		sendto_one_numeric(source_p, ERR_CANNOTSENDTOCHAN,
				   form_str(ERR_CANNOTSENDTOCHAN), name);
		return 0;
	}

	
	if(MyClient(source_p))
	{
		/* don't allow a knock if the user is banned */
		if(is_banned(chptr, source_p, NULL, NULL, NULL) == CHFL_BAN ||
				is_quieted(chptr, source_p, NULL, NULL, NULL) == CHFL_BAN)
		{
			sendto_one_numeric(source_p, ERR_CANNOTSENDTOCHAN,
					   form_str(ERR_CANNOTSENDTOCHAN), name);
			return 0;
		}

		/* local flood protection:
		 * allow one knock per user per knock_delay
		 * allow one knock per channel per knock_delay_channel
		 */
		if(!IsOper(source_p) && 
		   (source_p->localClient->last_knock + ConfigChannel.knock_delay) > rb_current_time())
		{
			sendto_one(source_p, form_str(ERR_TOOMANYKNOCK),
					me.name, source_p->name, name, "user");
			return 0;
		}
		else if((chptr->last_knock + ConfigChannel.knock_delay_channel) > rb_current_time())
		{
			sendto_one(source_p, form_str(ERR_TOOMANYKNOCK),
					me.name, source_p->name, name, "channel");
			return 0;
		}

		/* ok, we actually can send the knock, tell client */
		source_p->localClient->last_knock = rb_current_time();

		sendto_one(source_p, form_str(RPL_KNOCKDLVR),
			   me.name, source_p->name, name);
	}

	chptr->last_knock = rb_current_time();

	if(ConfigChannel.use_knock)
		sendto_channel_local(chptr->mode.mode & MODE_FREEINVITE ? ALL_MEMBERS : ONLY_CHANOPS,
				     chptr, form_str(RPL_KNOCK),
				     me.name, name, name, source_p->name,
				     source_p->username, source_p->host);

	sendto_server(client_p, chptr, CAP_KNOCK|CAP_TS6, NOCAPS,
		      ":%s KNOCK %s", use_id(source_p), name);
	sendto_server(client_p, chptr, CAP_KNOCK, CAP_TS6,
		      ":%s KNOCK %s", source_p->name, name);
	return 0;
}

