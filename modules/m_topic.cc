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

static void m_topic(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void ms_topic(struct MsgBuf *, client::client &, client::client &, int, const char **);

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
m_topic(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	chan::chan *chptr = NULL;
	chan::membership *msptr;
	char *p = NULL;
	const char *name;
	int operspy = 0;

	if((p = (char *)strchr(parv[1], ',')))
		*p = '\0';

	name = parv[1];

	if(IsOperSpy(&source) && parv[1][0] == '!')
	{
		name++;
		operspy = 1;

		if(EmptyString(name))
		{
			sendto_one(&source, form_str(ERR_NEEDMOREPARAMS),
					me.name, source.name, "TOPIC");
			return;
		}
	}

	if(my(source) && !is_flood_done(source))
		flood_endgrace(&source);

	chptr = chan::get(name, std::nothrow);

	if(chptr == NULL)
	{
		sendto_one_numeric(&source, ERR_NOSUCHCHANNEL,
				form_str(ERR_NOSUCHCHANNEL), name);
		return;
	}

	/* setting topic */
	if(parc > 2)
	{
		msptr = get(chptr->members, source, std::nothrow);

		if(msptr == NULL)
		{
			sendto_one_numeric(&source, ERR_NOTONCHANNEL,
					form_str(ERR_NOTONCHANNEL), name);
			return;
		}

		if (my(source) &&
		    !is_chanop(msptr) &&
		    !is_voiced(msptr) &&
		    !is(source, umode::OPER) &&
		    !tgchange::add_target(source, *chptr))
		{
			sendto_one(&source, form_str(ERR_TARGCHANGE),
				   me.name, source.name, chptr->name.c_str());
			return;
		}

		if(((chptr->mode.mode & chan::mode::TOPICLIMIT) == 0 ||
					get_channel_access(&source, chptr, msptr, MODE_ADD, NULL) >= chan::CHANOP) &&
				(!my(source) ||
				 can_send(chptr, &source, msptr)))
		{
			char topic[TOPICLEN + 1];
			char topic_info[USERHOST_REPLYLEN];
			rb_strlcpy(topic, parv[2], sizeof(topic));
			sprintf(topic_info, "%s!%s@%s",
					source.name, source.username, source.host);

			if (ConfigChannel.strip_topic_colors)
				strip_colour(topic);

			set_channel_topic(chptr, topic, topic_info, rb_current_time());

			sendto_server(&client, chptr, CAP_TS6, NOCAPS,
					":%s TOPIC %s :%s",
					use_id(&source), chptr->name.c_str(),
					chptr->topic? "" : chptr->topic.text.c_str());
			sendto_channel_local(chan::ALL_MEMBERS,
					chptr, ":%s!%s@%s TOPIC %s :%s",
					source.name, source.username,
					source.host, chptr->name.c_str(),
					chptr->topic? "" : chptr->topic.text.c_str());
		}
		else
			sendto_one(&source, form_str(ERR_CHANOPRIVSNEEDED),
					get_id(&me, &source),
					get_id(&source, &source), name);
	}
	else if(my(source))
	{
//		if(operspy)
//			report_operspy(&source, "TOPIC", chptr->name.c_str());

		if(!is_member(chptr, &source) && is_secret(chptr) && !operspy)
		{
			sendto_one_numeric(&source, ERR_NOTONCHANNEL, form_str(ERR_NOTONCHANNEL), name);
			return;
		}

		if(!chptr->topic)
			sendto_one(&source, form_str(RPL_NOTOPIC),
			           me.name,
			           source.name,
			           name);
		else
		{
			sendto_one(&source, form_str(RPL_TOPIC),
			           me.name,
			           source.name,
			           chptr->name.c_str(),
			           chptr->topic.text.c_str());

			sendto_one(&source, form_str(RPL_TOPICWHOTIME),
			           me.name,
			           source.name,
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
ms_topic(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	chan::chan *chptr = NULL;

	if((chptr = chan::get(parv[1], std::nothrow)) == NULL)
		return;

	set_channel_topic(chptr, parv[4], parv[2], atoi(parv[3]));

	sendto_channel_local(chan::ALL_MEMBERS, chptr,
	                     ":%s TOPIC %s :%s",
	                     source.name,
	                     parv[1],
	                     chptr->topic.text.c_str());
}
