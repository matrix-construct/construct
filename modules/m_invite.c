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

#include "stdinc.h"
#include "channel.h"
#include "client.h"
#include "hash.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "s_conf.h"
#include "s_serv.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "packet.h"
#include "tgchange.h"

static const char invite_desc[] = "Provides facilities for invite and related notifications";

static void m_invite(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
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

static bool add_invite(struct Channel *, struct Client *);

/* m_invite()
 *      parv[1] - user to invite
 *      parv[2] - channel name
 */
static void
m_invite(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	struct Channel *chptr;
	struct membership *msptr;
	int store_invite = 0;

	if(MyClient(source_p) && !IsFloodDone(source_p))
		flood_endgrace(source_p);

	if(MyClient(source_p))
		target_p = find_named_person(parv[1]);
	else
		target_p = find_person(parv[1]);
	if(target_p == NULL)
	{
		if(!MyClient(source_p) && IsDigit(parv[1][0]))
			sendto_one_numeric(source_p, ERR_NOSUCHNICK,
					   "* :Target left IRC. Failed to invite to %s",
					   parv[2]);
		else
			sendto_one_numeric(source_p, ERR_NOSUCHNICK,
					   form_str(ERR_NOSUCHNICK),
					   parv[1]);
		return;
	}

	if(check_channel_name(parv[2]) == 0)
	{
		sendto_one_numeric(source_p, ERR_BADCHANNAME,
				   form_str(ERR_BADCHANNAME),
				   parv[2]);
		return;
	}

	/* Do not send local channel invites to users if they are not on the
	 * same server as the person sending the INVITE message.
	 */
	if(parv[2][0] == '&' && !MyConnect(target_p))
	{
		sendto_one(source_p, form_str(ERR_USERNOTONSERV),
			   me.name, source_p->name, target_p->name);
		return;
	}

	if(((MyConnect(source_p) && !IsExemptResv(source_p)) ||
			(MyConnect(target_p) && !IsExemptResv(target_p))) &&
		hash_find_resv(parv[2]))
	{
		sendto_one_numeric(source_p, ERR_BADCHANNAME,
				   form_str(ERR_BADCHANNAME),
				   parv[2]);
		return;
	}

	if((chptr = find_channel(parv[2])) == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[2]);
		return;
	}

	msptr = find_channel_membership(chptr, source_p);
	if(MyClient(source_p) && (msptr == NULL))
	{
		sendto_one_numeric(source_p, ERR_NOTONCHANNEL,
				   form_str(ERR_NOTONCHANNEL), parv[2]);
		return;
	}

	if(IsMember(target_p, chptr))
	{
		sendto_one_numeric(source_p, ERR_USERONCHANNEL,
				   form_str(ERR_USERONCHANNEL),
				   target_p->name, parv[2]);
		return;
	}

	/* unconditionally require ops, unless the channel is +g */
	/* treat remote clients as chanops */
	if(MyClient(source_p) && !is_chanop(msptr) &&
			!(chptr->mode.mode & MODE_FREEINVITE))
	{
		sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
			   me.name, source_p->name, parv[2]);
		return;
	}

	/* store invites when they could affect the ability to join
	 * for +l/+j just check if the mode is set, this varies over time
	 */
	if(chptr->mode.mode & MODE_INVITEONLY ||
			(chptr->mode.mode & MODE_REGONLY && EmptyString(target_p->user->suser)) ||
			chptr->mode.limit || chptr->mode.join_num)
		store_invite = 1;

	if(MyConnect(source_p))
	{
		if (ConfigFileEntry.target_change && !IsOper(source_p) &&
				!find_allowing_channel(source_p, target_p) &&
				!add_target(source_p, target_p))
		{
			sendto_one(source_p, form_str(ERR_TARGCHANGE),
				   me.name, source_p->name, target_p->name);
			return;
		}
		sendto_one(source_p, form_str(RPL_INVITING),
			   me.name, source_p->name,
			   target_p->name, parv[2]);
		if(target_p->user->away)
			sendto_one_numeric(source_p, RPL_AWAY, form_str(RPL_AWAY),
					   target_p->name, target_p->user->away);
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
		if(!IsOper(source_p) && (IsSetCallerId(target_p) ||
					(IsSetRegOnlyMsg(target_p) && !source_p->user->suser[0])) &&
				!accept_message(source_p, target_p))
		{
			if (IsSetRegOnlyMsg(target_p) && !source_p->user->suser[0])
			{
				sendto_one_numeric(source_p, ERR_NONONREG,
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
					sendto_one_numeric(source_p, ERR_TARGUMODEG,
							   form_str(ERR_TARGUMODEG),
							   target_p->name);
					return;
				}
				target_p->localClient->last_caller_id_time = rb_current_time();
			}
		}
		add_reply_target(target_p, source_p);
		sendto_one(target_p, ":%s!%s@%s INVITE %s :%s",
			   source_p->name, source_p->username, source_p->host,
			   target_p->name, chptr->chname);

		if(store_invite)
		{
			if (!add_invite(chptr, target_p))
				return;

			sendto_channel_local_with_capability(CHFL_CHANOP, 0, CAP_INVITE_NOTIFY, chptr,
				":%s NOTICE %s :%s is inviting %s to %s.",
				me.name, chptr->chname, source_p->name, target_p->name, chptr->chname);
			sendto_channel_local_with_capability(CHFL_CHANOP, CAP_INVITE_NOTIFY, 0, chptr,
				":%s!%s@%s INVITE %s %s", source_p->name, source_p->username,
				source_p->host, target_p->name, chptr->chname);
		}
	}

	sendto_server(source_p, chptr, CAP_TS6, 0, ":%s INVITE %s %s %lu",
		      use_id(source_p), use_id(target_p),
		      chptr->chname, (unsigned long) chptr->channelts);
}

/* add_invite()
 *
 * input	- channel to add invite to, client to add
 * output	- true if it is a new invite, else false
 * side effects - client is added to invite list.
 */
static bool
add_invite(struct Channel *chptr, struct Client *who)
{
	rb_dlink_node *ptr;

	/* already invited? */
	RB_DLINK_FOREACH(ptr, who->user->invited.head)
	{
		if(ptr->data == chptr)
			return false;
	}

	/* ok, if their invite list is too long, remove the tail */
	if((int)rb_dlink_list_length(&who->user->invited) >=
	   ConfigChannel.max_chans_per_user)
	{
		ptr = who->user->invited.tail;
		del_invite(ptr->data, who);
	}

	/* add user to channel invite list */
	rb_dlinkAddAlloc(who, &chptr->invites);

	/* add channel to user invite list */
	rb_dlinkAddAlloc(chptr, &who->user->invited);

	return true;
}
