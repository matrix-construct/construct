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
 */

using namespace ircd;

static const char topic_desc[] =
	"Provides the TOPIC command to set, remove, and inspect channel topics";

static void m_topic(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void ms_topic(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message topic_msgtab = {
	"TOPIC", 0, 0, 0, 0,
	{mg_unreg, {m_topic, 2}, {m_topic, 2}, {ms_topic, 5}, mg_ignore, {m_topic, 2}}
};

mapi_clist_av1 topic_clist[] = { &topic_msgtab, NULL };
DECLARE_MODULE_AV2(topic, NULL, NULL, topic_clist, NULL, NULL, NULL, NULL, topic_desc);

/*
 * m_topic
 *      parv[1] = channel name
 *	parv[2] = new topic, if setting topic
 */
static void
m_topic(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	chan::chan *chptr = NULL;
	chan::membership *msptr;
	char *p = NULL;
	const char *name;
	int operspy = 0;

	if((p = (char *)strchr(parv[1], ',')))
		*p = '\0';

	name = parv[1];

	if(IsOperSpy(source_p) && parv[1][0] == '!')
	{
		name++;
		operspy = 1;

		if(EmptyString(name))
		{
			sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
					me.name, source_p->name, "TOPIC");
			return;
		}
	}

	if(MyClient(source_p) && !IsFloodDone(source_p))
		flood_endgrace(source_p);

	chptr = find_channel(name);

	if(chptr == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				form_str(ERR_NOSUCHCHANNEL), name);
		return;
	}

	/* setting topic */
	if(parc > 2)
	{
		msptr = find_channel_membership(chptr, source_p);

		if(msptr == NULL)
		{
			sendto_one_numeric(source_p, ERR_NOTONCHANNEL,
					form_str(ERR_NOTONCHANNEL), name);
			return;
		}

		if(MyClient(source_p) && !is_chanop_voiced(msptr) &&
				!IsOper(source_p) &&
				!add_channel_target(source_p, chptr))
		{
			sendto_one(source_p, form_str(ERR_TARGCHANGE),
				   me.name, source_p->name, chptr->name.c_str());
			return;
		}

		if(((chptr->mode.mode & chan::mode::TOPICLIMIT) == 0 ||
					get_channel_access(source_p, chptr, msptr, MODE_ADD, NULL) >= chan::CHANOP) &&
				(!MyClient(source_p) ||
				 can_send(chptr, source_p, msptr)))
		{
			char topic[TOPICLEN + 1];
			char topic_info[USERHOST_REPLYLEN];
			rb_strlcpy(topic, parv[2], sizeof(topic));
			sprintf(topic_info, "%s!%s@%s",
					source_p->name, source_p->username, source_p->host);

			if (ConfigChannel.strip_topic_colors)
				strip_colour(topic);

			set_channel_topic(chptr, topic, topic_info, rb_current_time());

			sendto_server(client_p, chptr, CAP_TS6, NOCAPS,
					":%s TOPIC %s :%s",
					use_id(source_p), chptr->name.c_str(),
					chptr->topic? "" : chptr->topic.text.c_str());
			sendto_channel_local(chan::ALL_MEMBERS,
					chptr, ":%s!%s@%s TOPIC %s :%s",
					source_p->name, source_p->username,
					source_p->host, chptr->name.c_str(),
					chptr->topic? "" : chptr->topic.text.c_str());
		}
		else
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
					get_id(&me, source_p),
					get_id(source_p, source_p), name);
	}
	else if(MyClient(source_p))
	{
		if(operspy)
			report_operspy(source_p, "TOPIC", chptr->name.c_str());

		if(!is_member(chptr, source_p) && secret(chptr) && !operspy)
		{
			sendto_one_numeric(source_p, ERR_NOTONCHANNEL, form_str(ERR_NOTONCHANNEL), name);
			return;
		}

		if(!chptr->topic)
			sendto_one(source_p, form_str(RPL_NOTOPIC),
			           me.name,
			           source_p->name,
			           name);
		else
		{
			sendto_one(source_p, form_str(RPL_TOPIC),
			           me.name,
			           source_p->name,
			           chptr->name.c_str(),
			           chptr->topic.text.c_str());

			sendto_one(source_p, form_str(RPL_TOPICWHOTIME),
			           me.name,
			           source_p->name,
			           chptr->name.c_str(),
			           chptr->topic.info.c_str(),
			           ulong(chptr->topic.time));
		}
	}
}

/*
 * ms_topic
 *      parv[1] = channel name
 *	parv[2] = topic_info
 *	parv[3] = topic_info time
 *	parv[4] = new channel topic
 *
 * Let servers always set a topic
 */
static void
ms_topic(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	chan::chan *chptr = NULL;

	if((chptr = find_channel(parv[1])) == NULL)
		return;

	set_channel_topic(chptr, parv[4], parv[2], atoi(parv[3]));

	sendto_channel_local(chan::ALL_MEMBERS, chptr,
	                     ":%s TOPIC %s :%s",
	                     source_p->name,
	                     parv[1],
	                     chptr->topic.text.c_str());
}
