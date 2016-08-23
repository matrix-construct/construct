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

static const char kick_desc[] = "Provides the KICK command to remove a user from a channel";

static void m_kick(struct MsgBuf *, client::client &, client::client &, int, const char **);
#define mg_kick { m_kick, 3 }

struct Message kick_msgtab = {
	"KICK", 0, 0, 0, 0,
	{mg_unreg, mg_kick, mg_kick, mg_kick, mg_ignore, mg_kick}
};

mapi_clist_av1 kick_clist[] = { &kick_msgtab, NULL };

DECLARE_MODULE_AV2(kick, NULL, NULL, kick_clist, NULL, NULL, NULL, NULL, kick_desc);

/*
** m_kick
**      parv[1] = channel
**      parv[2] = client to kick
**      parv[3] = kick comment
*/
static void
m_kick(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
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

	if(my(source) && !is_flood_done(source))
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

	if(!is_server(source))
	{
		msptr = get(chptr->members, source, std::nothrow);

		if((msptr == NULL) && my_connect(source))
		{
			sendto_one_numeric(&source, ERR_NOTONCHANNEL,
					   form_str(ERR_NOTONCHANNEL), name);
			return;
		}

		if(get_channel_access(&source, chptr, msptr, MODE_ADD, NULL) < chan::CHANOP)
		{
			if(my_connect(source))
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
		if(my(source) && IsService(who))
		{
			sendto_one(&source, form_str(ERR_ISCHANSERVICE),
			           me.name,
			           source.name,
			           who->name,
			           chptr->name.c_str());
			return;
		}

		if(my(source))
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
		if(is_server(source))
			sendto_channel_local(chan::ALL_MEMBERS, chptr, ":%s KICK %s %s :%s",
					     source.name, name, who->name, comment);
		else
			sendto_channel_local(chan::ALL_MEMBERS, chptr,
					     ":%s!%s@%s KICK %s %s :%s",
					     source.name, source.username,
					     source.host, name, who->name, comment);

		sendto_server(&client, chptr, CAP_TS6, NOCAPS,
			      ":%s KICK %s %s :%s",
			      use_id(&source), chptr->name.c_str(), use_id(who), comment);

		del(*chptr, *who);
	}
	else if (my(source))
		sendto_one_numeric(&source, ERR_USERNOTINCHANNEL,
				   form_str(ERR_USERNOTINCHANNEL), user, name);
}
