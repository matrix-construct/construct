/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_part.c: Parts a user from a channel.
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

static const char part_desc[] = "Provides the PART command to leave a channel";

static void m_part(struct MsgBuf *, client::client &, client::client &, int, const char **);

struct Message part_msgtab = {
	"PART", 0, 0, 0, 0,
	{mg_unreg, {m_part, 2}, {m_part, 2}, mg_ignore, mg_ignore, {m_part, 2}}
};

mapi_clist_av1 part_clist[] = { &part_msgtab, NULL };

DECLARE_MODULE_AV2(part, NULL, NULL, part_clist, NULL, NULL, NULL, NULL, part_desc);

static void part_one_client(client::client &client,
			    client::client &source, char *name,
			    const char *reason);
static bool can_send_part(client::client &source, chan::chan *chptr, chan::membership *msptr);
static bool do_message_hook(client::client &source, chan::chan *chptr, const char **reason);


/*
** m_part
**      parv[1] = channel
**      parv[2] = reason
*/
static void
m_part(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	char *p, *name;
	char reason[REASONLEN + 1];
	char *s = LOCAL_COPY(parv[1]);

	reason[0] = '\0';

	if(parc > 2)
		rb_strlcpy(reason, parv[2], sizeof(reason));

	name = rb_strtok_r(s, ",", &p);

	/* Finish the flood grace period... */
	if(my(source) && !is_flood_done(source))
		flood_endgrace(&source);

	while(name)
	{
		part_one_client(client, source, name, reason);
		name = rb_strtok_r(NULL, ",", &p);
	}
}

/*
 * part_one_client
 *
 * inputs	- pointer to server
 * 		- pointer to source client to remove
 *		- char pointer of name of channel to remove from
 * output	- none
 * side effects	- remove ONE client given the channel name
 */
static void
part_one_client(client::client &client, client::client &source, char *name, const char *reason)
{
	chan::chan *chptr;
	chan::membership *msptr;

	if((chptr = chan::get(name, std::nothrow)) == NULL)
	{
		sendto_one_numeric(&source, ERR_NOSUCHCHANNEL, form_str(ERR_NOSUCHCHANNEL), name);
		return;
	}

	msptr = get(chptr->members, source, std::nothrow);
	if(msptr == NULL)
	{
		sendto_one_numeric(&source, ERR_NOTONCHANNEL, form_str(ERR_NOTONCHANNEL), name);
		return;
	}

	if(my_connect(source) && !IsOper(&source) && !is_exempt_spambot(source))
		chan::check_spambot_warning(&source, NULL);

	/*
	 *  Remove user from the old channel (if any)
	 *  only allow /part reasons in -m chans
	 */
	if(!EmptyString(reason) &&
		(!my_connect(source) ||
		 (can_send_part(source, chptr, msptr) && do_message_hook(source, chptr, &reason))
		)
	  )
	{

		sendto_server(&client, chptr, CAP_TS6, NOCAPS,
		              ":%s PART %s :%s",
		              use_id(&source),
		              chptr->name.c_str(),
		              reason);

		sendto_channel_local(chan::ALL_MEMBERS, chptr,
		                     ":%s!%s@%s PART %s :%s",
		                     source.name,
		                     source.username,
		                     source.host,
		                     chptr->name.c_str(),
		                     reason);
	}
	else
	{
		sendto_server(&client, chptr, CAP_TS6, NOCAPS,
		              ":%s PART %s",
		              use_id(&source),
		              chptr->name.c_str());

		sendto_channel_local(chan::ALL_MEMBERS, chptr,
		                     ":%s!%s@%s PART %s",
		                     source.name,
		                     source.username,
		                     source.host,
		                     chptr->name.c_str());
	}

	del(*chptr, source);
}

/*
 * can_send_part - whether a part message can be sent.
 *
 * inputs:
 *    - client parting
 *    - channel being parted
 *    - membership pointer
 * outputs:
 *    - true if message allowed
 *    - false if message denied
 * side effects:
 *    - none.
 */
static bool
can_send_part(client::client &source, chan::chan *chptr, chan::membership *msptr)
{
	if (!can_send(chptr, &source, msptr))
		return false;
	/* Allow chanops to bypass anti_spam_exit_message_time for part messages. */
	if (is_chanop(msptr))
		return true;
	return (source.localClient->firsttime + ConfigFileEntry.anti_spam_exit_message_time) < rb_current_time();
}

/*
 * do_message_hook - execute the message hook on a part message reason.
 *
 * inputs:
 *    - client parting
 *    - channel being parted
 *    - pointer to reason
 * outputs:
 *    - true if message is allowed
 *    - false if message is denied or message is now empty
 * side effects:
 *    - reason may be modified.
 */
static bool
do_message_hook(client::client &source, chan::chan *chptr, const char **reason)
{
	hook_data_privmsg_channel hdata;

	hdata.msgtype = MESSAGE_TYPE_PART;
	hdata.source_p = &source;
	hdata.chptr = chptr;
	hdata.text = *reason;
	hdata.approved = 0;

	call_hook(h_privmsg_channel, &hdata);

	/* The reason may have been changed by a hook... */
	*reason = hdata.text;

	return (hdata.approved == 0 && !EmptyString(*reason));
}
