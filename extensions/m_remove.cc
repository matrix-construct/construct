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
 */

using namespace ircd;

static const char description[] = "Provides the REMOVE command, an alternative to KICK";

static void m_remove(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void remove_quote_part(hook_data_privmsg_channel *);

unsigned int CAP_REMOVE;
static char part_buf[REASONLEN + 1];

struct Message remove_msgtab = {
	"REMOVE", 0, 0, 0, 0,
	{mg_unreg, {m_remove, 3}, {m_remove, 3}, {m_remove, 3}, mg_ignore, {m_remove, 3}}
};

mapi_clist_av1 remove_clist[] = { &remove_msgtab, NULL };
mapi_hfn_list_av1 remove_hfnlist[] = {
	{ "privmsg_channel", (hookfn) remove_quote_part },
	{ NULL, NULL }
};
mapi_cap_list_av2 remove_cap_list[] = {
	{ MAPI_CAP_SERVER, "REMOVE", NULL, &CAP_REMOVE },
	{ 0, NULL, NULL, NULL }
};

DECLARE_MODULE_AV2(remove, NULL, NULL, remove_clist, NULL, remove_hfnlist, remove_cap_list, NULL, description);

static void
m_remove(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	chan::membership *msptr;
	client::client *who;
	chan::chan *chptr;
	int chasing = 0;
	char *comment;
	const char *name;
	char *p = NULL;
	const char *user;
	static char buf[BUFSIZE];

	if(MyClient(&source) && !IsFloodDone(&source))
		flood_endgrace(&source);

	*buf = '\0';
	if((p = (char *)strchr(parv[1], ',')))
		*p = '\0';

	name = parv[1];

	chptr = chan::get(name, std::nothrow);
	if(chptr == NULL)
	{
		sendto_one_numeric(&source, ERR_NOSUCHCHANNEL, form_str(ERR_NOSUCHCHANNEL), name);
		return;
	}

	if(!IsServer(&source))
	{
		msptr = get(chptr->members, source, std::nothrow);

		if((msptr == NULL) && MyConnect(&source))
		{
			sendto_one_numeric(&source, ERR_NOTONCHANNEL,
					   form_str(ERR_NOTONCHANNEL), name);
			return;
		}

		if(get_channel_access(&source, chptr, msptr, MODE_ADD, NULL) < chan::CHANOP)
		{
			if(MyConnect(&source))
			{
				sendto_one(&source, form_str(ERR_CHANOPRIVSNEEDED),
					   me.name, source.name, name);
				return;
			}

			/* If its a TS 0 channel, do it the old way */
			if(chptr->channelts == 0)
			{
				sendto_one(&source, form_str(ERR_CHANOPRIVSNEEDED),
					   get_id(&me, &source), get_id(&source, &source), name);
				return;
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

	if((p = (char *)strchr(parv[2], ',')))
		*p = '\0';

	user = parv[2];		/* strtoken(&p2, parv[2], ","); */

	if(!(who = find_chasing(&source, user, &chasing)))
	{
		return;
	}

	msptr = get(chptr->members, *who, std::nothrow);

	if(msptr != NULL)
	{
		if(MyClient(&source) && IsService(who))
		{
			sendto_one(&source, form_str(ERR_ISCHANSERVICE),
				   me.name, source.name, who->name, chptr->name.c_str());
			return;
		}

		if(MyClient(&source))
		{
			hook_data_channel_approval hookdata;

			hookdata.client = &source;
			hookdata.chptr = chptr;
			hookdata.msptr = msptr;
			hookdata.target = who;
			hookdata.approved = 1;
			hookdata.dir = MODE_ADD;	/* ensure modules like override speak up */

			call_hook(h_can_kick, &hookdata);

			if (!hookdata.approved)
				return;
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
		sendto_channel_local(chan::ALL_MEMBERS, chptr,
				     ":%s!%s@%s PART %s :requested by %s (%s)",
				     who->name, who->username,
				     who->host, name, source.name, comment);

		sendto_server(&client, chptr, CAP_REMOVE, NOCAPS,
			      ":%s REMOVE %s %s :%s",
			      use_id(&source), chptr->name.c_str(), use_id(who), comment);
		sendto_server(&client, chptr, NOCAPS, CAP_REMOVE,
			      ":%s KICK %s %s :%s",
			      use_id(&source), chptr->name.c_str(), use_id(who), comment);

		del(*chptr, get_client(*msptr));
	}
	else if (MyClient(&source))
		sendto_one_numeric(&source, ERR_USERNOTINCHANNEL,
				   form_str(ERR_USERNOTINCHANNEL), user, name);
}

static void
remove_quote_part(hook_data_privmsg_channel *data)
{
	if (data->approved || EmptyString(data->text) || data->msgtype != MESSAGE_TYPE_PART)
		return;

	rb_strlcpy(part_buf, "\"", sizeof(part_buf) - 1);
	rb_strlcat(part_buf, data->text, sizeof(part_buf) - 1);
	rb_strlcat(part_buf, "\"", sizeof(part_buf));

	data->text = part_buf;
}
