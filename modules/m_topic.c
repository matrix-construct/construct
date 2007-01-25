/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_topic.c: Sets a channel topic.
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
 *  $Id: m_topic.c 254 2005-09-21 23:35:12Z nenolod $
 */

#include "stdinc.h"
#include "tools.h"
#include "channel.h"
#include "client.h"
#include "hash.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "s_serv.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "packet.h"

static int m_topic(struct Client *, struct Client *, int, const char **);
static int ms_topic(struct Client *, struct Client *, int, const char **);

struct Message topic_msgtab = {
	"TOPIC", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_topic, 2}, {m_topic, 2}, {ms_topic, 5}, mg_ignore, {m_topic, 2}}
};

mapi_clist_av1 topic_clist[] = { &topic_msgtab, NULL };
DECLARE_MODULE_AV1(topic, NULL, NULL, topic_clist, NULL, NULL, "$Revision: 254 $");

/*
 * m_topic
 *      parv[0] = sender prefix
 *      parv[1] = channel name
 *	parv[2] = new topic, if setting topic
 */
static int
m_topic(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr = NULL;
	struct membership *msptr;
	char *p = NULL;

	if((p = strchr(parv[1], ',')))
		*p = '\0';

	if(MyClient(source_p) && !IsFloodDone(source_p))
		flood_endgrace(source_p);

	if(!IsChannelName(parv[1]))
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[1]);
		return 0;
	}

	chptr = find_channel(parv[1]);

	if(chptr == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				form_str(ERR_NOSUCHCHANNEL), parv[1]);
		return 0;
	}

	/* setting topic */
	if(parc > 2)
	{
		msptr = find_channel_membership(chptr, source_p);

		if(msptr == NULL)
		{
			sendto_one_numeric(source_p, ERR_NOTONCHANNEL,
					form_str(ERR_NOTONCHANNEL), parv[1]);
			return 0;
		}

		if((chptr->mode.mode & MODE_TOPICLIMIT) == 0 || is_chanop(msptr))
		{
			char topic_info[USERHOST_REPLYLEN];
			ircsprintf(topic_info, "%s!%s@%s",
					source_p->name, source_p->username, source_p->host);
			set_channel_topic(chptr, parv[2], topic_info, CurrentTime);

			sendto_server(client_p, chptr, CAP_TS6, NOCAPS,
					":%s TOPIC %s :%s",
					use_id(source_p), chptr->chname,
					chptr->topic == NULL ? "" : chptr->topic);
			sendto_server(client_p, chptr, NOCAPS, CAP_TS6,
					":%s TOPIC %s :%s",
					source_p->name, chptr->chname,
					chptr->topic == NULL ? "" : chptr->topic);
			sendto_channel_local(ALL_MEMBERS,
					chptr, ":%s!%s@%s TOPIC %s :%s",
					source_p->name, source_p->username,
					source_p->host, chptr->chname,
					chptr->topic == NULL ? "" : chptr->topic);
		}
		else
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
					me.name, source_p->name, parv[1]);
	}
	else if(MyClient(source_p))
	{
		if(!IsMember(source_p, chptr) && SecretChannel(chptr))
		{
			sendto_one_numeric(source_p, ERR_NOTONCHANNEL,
					form_str(ERR_NOTONCHANNEL), parv[1]);
			return 0;
		}
		if(chptr->topic == NULL)
			sendto_one(source_p, form_str(RPL_NOTOPIC),
					me.name, source_p->name, parv[1]);
		else
		{
			sendto_one(source_p, form_str(RPL_TOPIC),
					me.name, source_p->name, chptr->chname, chptr->topic);

			sendto_one(source_p, form_str(RPL_TOPICWHOTIME),
					me.name, source_p->name, chptr->chname,
					chptr->topic_info, chptr->topic_time);
		}
	}

	return 0;
}

/*
 * ms_topic
 *      parv[0] = sender prefix
 *      parv[1] = channel name
 *	parv[2] = topic_info
 *	parv[3] = topic_info time
 *	parv[4] = new channel topic
 *
 * Let servers always set a topic
 */
static int
ms_topic(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr = NULL;

	if(IsChannelName(parv[1]))
	{
		if((chptr = find_channel(parv[1])) == NULL)
			return 0;

		set_channel_topic(chptr, parv[4], parv[2], atoi(parv[3]));

		sendto_channel_local(ALL_MEMBERS, chptr, ":%s TOPIC %s :%s",
				     source_p->name, parv[1], 
				     chptr->topic == NULL ? "" : chptr->topic);
	}

	return 0;
}
