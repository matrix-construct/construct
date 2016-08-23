/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_invite.c: Invites the user to join a channel.
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

static const char invite_desc[] = "Provides facilities for invite and related notifications";

static void m_invite(struct MsgBuf *, client::client &, client::client &, int, const char **);
static unsigned int CAP_INVITE_NOTIFY = 0;

struct Message invite_msgtab = {
	"INVITE", 0, 0, 0, 0,
	{mg_unreg, {m_invite, 3}, {m_invite, 3}, mg_ignore, mg_ignore, {m_invite, 3}}
};

mapi_clist_av1 invite_clist[] = { &invite_msgtab, NULL };

mapi_cap_list_av2 invite_cap_list[] = {
	{ MAPI_CAP_CLIENT, "invite-notify", NULL, &CAP_INVITE_NOTIFY },
	{ 0, NULL, NULL, NULL }
};

DECLARE_MODULE_AV2(invite, NULL, NULL, invite_clist, NULL, NULL, invite_cap_list, NULL, invite_desc);

static bool add_invite(chan::chan &, client::client &);

/* m_invite()
 *      parv[1] - user to invite
 *      parv[2] - channel name
 */
static void
m_invite(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	client::client *target_p;
	chan::chan *chptr;
	chan::membership *msptr;
	int store_invite = 0;

	if(MyClient(&source) && !IsFloodDone(&source))
		flood_endgrace(&source);

	if(MyClient(&source))
		target_p = client::find_named_person(parv[1]);
	else
		target_p = client::find_person(parv[1]);
	if(target_p == NULL)
	{
		if(!MyClient(&source) && rfc1459::is_digit(parv[1][0]))
			sendto_one_numeric(&source, ERR_NOSUCHNICK,
					   "* :Target left IRC. Failed to invite to %s",
					   parv[2]);
		else
			sendto_one_numeric(&source, ERR_NOSUCHNICK,
					   form_str(ERR_NOSUCHNICK),
					   parv[1]);
		return;
	}

	if(chan::valid_name(parv[2]) == 0)
	{
		sendto_one_numeric(&source, ERR_BADCHANNAME,
				   form_str(ERR_BADCHANNAME),
				   parv[2]);
		return;
	}

	/* Do not send local channel invites to users if they are not on the
	 * same server as the person sending the INVITE message.
	 */
	if(parv[2][0] == '&' && !MyConnect(target_p))
	{
		sendto_one(&source, form_str(ERR_USERNOTONSERV),
			   me.name, source.name, target_p->name);
		return;
	}

	if(((MyConnect(&source) && !IsExemptResv(&source)) ||
			(MyConnect(target_p) && !IsExemptResv(target_p))) &&
		hash_find_resv(parv[2]))
	{
		sendto_one_numeric(&source, ERR_BADCHANNAME,
				   form_str(ERR_BADCHANNAME),
				   parv[2]);
		return;
	}

	if((chptr = chan::get(parv[2], std::nothrow)) == NULL)
	{
		sendto_one_numeric(&source, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[2]);
		return;
	}

	msptr = get(chptr->members, source, std::nothrow);
	if(MyClient(&source) && (msptr == NULL))
	{
		sendto_one_numeric(&source, ERR_NOTONCHANNEL,
				   form_str(ERR_NOTONCHANNEL), parv[2]);
		return;
	}

	if(is_member(chptr, target_p))
	{
		sendto_one_numeric(&source, ERR_USERONCHANNEL,
				   form_str(ERR_USERONCHANNEL),
				   target_p->name, parv[2]);
		return;
	}

	/* unconditionally require ops, unless the channel is +g */
	/* treat remote clients as chanops */
	if(MyClient(&source) && !is_chanop(msptr) &&
			!(chptr->mode.mode & chan::mode::FREEINVITE))
	{
		sendto_one(&source, form_str(ERR_CHANOPRIVSNEEDED),
			   me.name, source.name, parv[2]);
		return;
	}

	/* store invites when they could affect the ability to join
	 * for +l/+j just check if the mode is set, this varies over time
	 */
	if(chptr->mode.mode & chan::mode::INVITEONLY ||
			(chptr->mode.mode & chan::mode::REGONLY && suser(user(*target_p)).empty()) ||
			chptr->mode.limit || chptr->mode.join_num)
		store_invite = 1;

	if(MyConnect(&source))
	{
		if (ConfigFileEntry.target_change && !IsOper(&source) &&
				!find_allowing_channel(&source, target_p) &&
				!add_target(&source, target_p))
		{
			sendto_one(&source, form_str(ERR_TARGCHANGE),
				   me.name, source.name, target_p->name);
			return;
		}
		sendto_one(&source, form_str(RPL_INVITING),
			   me.name, source.name,
			   target_p->name, parv[2]);
		if(away(user(*target_p)).size())
			sendto_one_numeric(&source, RPL_AWAY, form_str(RPL_AWAY),
					   target_p->name, away(user(*target_p)).c_str());
	}
	/* invite timestamp */
	else if(parc > 3 && !EmptyString(parv[3]))
	{
		/* this should never be less than */
		if(atol(parv[3]) > chptr->channelts)
			return;
	}

	if(MyConnect(target_p))
	{
		if(!IsOper(&source) && (IsSetCallerId(target_p) ||
					(IsSetRegOnlyMsg(target_p) && !suser(user(source))[0])) &&
				!accept_message(&source, target_p))
		{
			if (IsSetRegOnlyMsg(target_p) && !suser(user(source))[0])
			{
				sendto_one_numeric(&source, ERR_NONONREG,
						form_str(ERR_NONONREG),
						target_p->name);
				return;
			}
			else
			{
				/* instead of sending RPL_UMODEGMSG,
				 * just let the invite through
				 */
				if((target_p->localClient->last_caller_id_time +
				    ConfigFileEntry.caller_id_wait) >= rb_current_time())
				{
					sendto_one_numeric(&source, ERR_TARGUMODEG,
							   form_str(ERR_TARGUMODEG),
							   target_p->name);
					return;
				}
				target_p->localClient->last_caller_id_time = rb_current_time();
			}
		}
		add_reply_target(target_p, &source);
		sendto_one(target_p, ":%s!%s@%s INVITE %s :%s",
			   source.name, source.username, source.host,
			   target_p->name, chptr->name.c_str());

		if(store_invite)
		{
			if (!add_invite(*chptr, *target_p))
				return;

			sendto_channel_local_with_capability(chan::CHANOP, 0, CAP_INVITE_NOTIFY, chptr,
				":%s NOTICE %s :%s is inviting %s to %s.",
				me.name, chptr->name.c_str(), source.name, target_p->name, chptr->name.c_str());
			sendto_channel_local_with_capability(chan::CHANOP, CAP_INVITE_NOTIFY, 0, chptr,
				":%s!%s@%s INVITE %s %s", source.name, source.username,
				source.host, target_p->name, chptr->name.c_str());
		}
	}

	sendto_server(&source, chptr, CAP_TS6, 0, ":%s INVITE %s %s %lu",
		      use_id(&source), use_id(target_p),
		      chptr->name.c_str(), (unsigned long) chptr->channelts);
}

/* add_invite()
 *
 * input	- channel to add invite to, client to add
 * output	- true if it is a new invite, else false
 * side effects - client is added to invite list.
 */
static bool
add_invite(chan::chan &chan, client::client &client)
{
	if (invites(user(client)).size() >= ConfigChannel.max_chans_per_user)
		return false;

	if (chan.invites.size() >= ConfigChannel.max_chans_per_user)
		return false;

	chan.invites.emplace(&client);
	invites(user(client)).emplace(&chan);
	return true;
}
