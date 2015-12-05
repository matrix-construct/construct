/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_message.c: Sends a (PRIVMSG|NOTICE) message to a user or channel.
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
 *  $Id: m_message.c 3173 2007-01-31 23:57:18Z jilles $
 */

#include "stdinc.h"
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "common.h"
#include "s_conf.h"
#include "s_serv.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "channel.h"
#include "match.h"
#include "hash.h"
#include "class.h"
#include "msg.h"
#include "packet.h"
#include "send.h"
#include "s_newconf.h"
#include "s_stats.h"
#include "tgchange.h"
#include "inline/stringops.h"

static int m_message(enum message_type, struct Client *, struct Client *, int, const char **);
static int m_privmsg(struct Client *, struct Client *, int, const char **);
static int m_notice(struct Client *, struct Client *, int, const char **);

static void expire_tgchange(void *unused);
static struct ev_entry *expire_tgchange_event;

static int
modinit(void)
{
	expire_tgchange_event = rb_event_addish("expire_tgchange", expire_tgchange, NULL, 300);
	expire_tgchange(NULL);
	return 0;
}

static void
moddeinit(void)
{
	rb_event_delete(expire_tgchange_event);
}

struct Message privmsg_msgtab = {
	"PRIVMSG", 0, 0, 0, MFLG_SLOW | MFLG_UNREG,
	{mg_unreg, {m_privmsg, 0}, {m_privmsg, 0}, mg_ignore, mg_ignore, {m_privmsg, 0}}
};
struct Message notice_msgtab = {
	"NOTICE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_notice, 0}, {m_notice, 0}, {m_notice, 0}, mg_ignore, {m_notice, 0}}
};

mapi_clist_av1 message_clist[] = { &privmsg_msgtab, &notice_msgtab, NULL };

DECLARE_MODULE_AV1(message, modinit, moddeinit, message_clist, NULL, NULL, "$Revision: 3173 $");

struct entity
{
	void *ptr;
	int type;
	int flags;
};

static int build_target_list(enum message_type msgtype,
			     struct Client *client_p,
			     struct Client *source_p, const char *nicks_channels, const char *text);

static int flood_attack_client(enum message_type msgtype, struct Client *source_p, struct Client *target_p);

/* Fifteen seconds should be plenty for a client to reply a ctcp */
#define LARGE_CTCP_TIME 15

#define ENTITY_NONE    0
#define ENTITY_CHANNEL 1
#define ENTITY_CHANNEL_OPMOD 2
#define ENTITY_CHANOPS_ON_CHANNEL 3
#define ENTITY_CLIENT  4

static struct entity targets[512];
static int ntargets = 0;

static int duplicate_ptr(void *);

static void msg_channel(enum message_type msgtype,
			struct Client *client_p,
			struct Client *source_p, struct Channel *chptr, const char *text);

static void msg_channel_opmod(enum message_type msgtype,
			      struct Client *client_p,
			      struct Client *source_p, struct Channel *chptr,
			      const char *text);

static void msg_channel_flags(enum message_type msgtype,
			      struct Client *client_p,
			      struct Client *source_p,
			      struct Channel *chptr, int flags, const char *text);

static void msg_client(enum message_type msgtype,
		       struct Client *source_p, struct Client *target_p, const char *text);

static void handle_special(enum message_type msgtype,
			   struct Client *client_p, struct Client *source_p, const char *nick,
			   const char *text);

/*
** m_privmsg
**
** massive cleanup
** rev argv 6/91
**
**   Another massive cleanup Nov, 2000
** (I don't think there is a single line left from 6/91. Maybe.)
** m_privmsg and m_notice do basically the same thing.
** in the original 2.8.2 code base, they were the same function
** "m_message.c." When we did the great cleanup in conjuncton with bleep
** of ircu fame, we split m_privmsg.c and m_notice.c.
** I don't see the point of that now. Its harder to maintain, its
** easier to introduce bugs into one version and not the other etc.
** Really, the penalty of an extra function call isn't that big a deal folks.
** -db Nov 13, 2000
**
*/
const char *cmdname[MESSAGE_TYPE_COUNT] = {
	[MESSAGE_TYPE_PRIVMSG] = "PRIVMSG",
	[MESSAGE_TYPE_NOTICE] = "NOTICE",
};

static int
m_privmsg(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	return m_message(MESSAGE_TYPE_PRIVMSG, client_p, source_p, parc, parv);
}

static int
m_notice(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	return m_message(MESSAGE_TYPE_NOTICE, client_p, source_p, parc, parv);
}

/*
 * inputs	- flag privmsg or notice
 *		- pointer to client_p
 *		- pointer to source_p
 *		- pointer to channel
 */
static int
m_message(enum message_type msgtype,
	  struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	int i;

	if(parc < 2 || EmptyString(parv[1]))
	{
		if(msgtype != MESSAGE_TYPE_NOTICE)
			sendto_one(source_p, form_str(ERR_NORECIPIENT), me.name,
				   source_p->name, cmdname[msgtype]);
		return 0;
	}

	if(parc < 3 || EmptyString(parv[2]))
	{
		if(msgtype != MESSAGE_TYPE_NOTICE)
			sendto_one(source_p, form_str(ERR_NOTEXTTOSEND), me.name, source_p->name);
		return 0;
	}

	/* Finish the flood grace period if theyre not messaging themselves
	 * as some clients (ircN) do this as a "lag check"
	 */
	if(MyClient(source_p) && !IsFloodDone(source_p) && irccmp(source_p->name, parv[1]))
		flood_endgrace(source_p);

	if(build_target_list(msgtype, client_p, source_p, parv[1], parv[2]) < 0)
	{
		return 0;
	}

	for(i = 0; i < ntargets; i++)
	{
		switch (targets[i].type)
		{
		case ENTITY_CHANNEL:
			msg_channel(msgtype, client_p, source_p,
				    (struct Channel *) targets[i].ptr, parv[2]);
			break;

		case ENTITY_CHANNEL_OPMOD:
			msg_channel_opmod(msgtype, client_p, source_p,
				   (struct Channel *) targets[i].ptr, parv[2]);
			break;

		case ENTITY_CHANOPS_ON_CHANNEL:
			msg_channel_flags(msgtype, client_p, source_p,
					  (struct Channel *) targets[i].ptr,
					  targets[i].flags, parv[2]);
			break;

		case ENTITY_CLIENT:
			msg_client(msgtype, source_p,
				   (struct Client *) targets[i].ptr, parv[2]);
			break;
		}
	}

	return 0;
}

/*
 * build_target_list
 *
 * inputs	- pointer to given client_p (server)
 *		- pointer to given source (oper/client etc.)
 *		- pointer to list of nicks/channels
 *		- pointer to table to place results
 *		- pointer to text (only used if source_p is an oper)
 * output	- number of valid entities
 * side effects	- target_table is modified to contain a list of
 *		  pointers to channels or clients
 *		  if source client is an oper
 *		  all the classic old bizzare oper privmsg tricks
 *		  are parsed and sent as is, if prefixed with $
 *		  to disambiguate.
 *
 */

static int
build_target_list(enum message_type msgtype, struct Client *client_p,
		  struct Client *source_p, const char *nicks_channels, const char *text)
{
	int type;
	char *p, *nick, *target_list;
	struct Channel *chptr = NULL;
	struct Client *target_p;

	target_list = LOCAL_COPY(nicks_channels);	/* skip strcpy for non-lazyleafs */

	ntargets = 0;

	for(nick = rb_strtok_r(target_list, ",", &p); nick; nick = rb_strtok_r(NULL, ",", &p))
	{
		char *with_prefix;
		/*
		 * channels are privmsg'd a lot more than other clients, moved up
		 * here plain old channel msg?
		 */

		if(IsChanPrefix(*nick))
		{
			/* ignore send of local channel to a server (should not happen) */
			if(IsServer(client_p) && *nick == '&')
				continue;

			if((chptr = find_channel(nick)) != NULL)
			{
				if(!duplicate_ptr(chptr))
				{
					if(ntargets >= ConfigFileEntry.max_targets)
					{
						sendto_one(source_p, form_str(ERR_TOOMANYTARGETS),
							   me.name, source_p->name, nick);
						return (1);
					}
					targets[ntargets].ptr = (void *) chptr;
					targets[ntargets++].type = ENTITY_CHANNEL;
				}
			}

			/* non existant channel */
			else if(msgtype != MESSAGE_TYPE_NOTICE)
				sendto_one_numeric(source_p, ERR_NOSUCHNICK,
						   form_str(ERR_NOSUCHNICK), nick);

			continue;
		}

		if(MyClient(source_p))
			target_p = find_named_person(nick);
		else
			target_p = find_person(nick);

		/* look for a privmsg to another client */
		if(target_p)
		{
			if(!duplicate_ptr(target_p))
			{
				if(ntargets >= ConfigFileEntry.max_targets)
				{
					sendto_one(source_p, form_str(ERR_TOOMANYTARGETS),
						   me.name, source_p->name, nick);
					return (1);
				}
				targets[ntargets].ptr = (void *) target_p;
				targets[ntargets].type = ENTITY_CLIENT;
				targets[ntargets++].flags = 0;
			}
			continue;
		}

		/* @#channel or +#channel message ? */

		type = 0;
		with_prefix = nick;
		/*  allow %+@ if someone wants to do that */
		for(;;)
		{
			if(*nick == '@')
				type |= CHFL_CHANOP;
			else if(*nick == '+')
				type |= CHFL_CHANOP | CHFL_VOICE;
			else
				break;
			nick++;
		}

		if(type != 0)
		{
			/* no recipient.. */
			if(EmptyString(nick))
			{
				sendto_one(source_p, form_str(ERR_NORECIPIENT),
					   me.name, source_p->name, cmdname[msgtype]);
				continue;
			}

			/* At this point, nick+1 should be a channel name i.e. #foo or &foo
			 * if the channel is found, fine, if not report an error
			 */

			if((chptr = find_channel(nick)) != NULL)
			{
				struct membership *msptr;

				msptr = find_channel_membership(chptr, source_p);

				if(!IsServer(source_p) && !IsService(source_p) && !is_chanop_voiced(msptr))
				{
					sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
						   get_id(&me, source_p),
						   get_id(source_p, source_p),
						   with_prefix);
					continue;
				}

				if(!duplicate_ptr(chptr))
				{
					if(ntargets >= ConfigFileEntry.max_targets)
					{
						sendto_one(source_p, form_str(ERR_TOOMANYTARGETS),
							   me.name, source_p->name, nick);
						return (1);
					}
					targets[ntargets].ptr = (void *) chptr;
					targets[ntargets].type = ENTITY_CHANOPS_ON_CHANNEL;
					targets[ntargets++].flags = type;
				}
			}
			else if(msgtype != MESSAGE_TYPE_NOTICE)
			{
				sendto_one_numeric(source_p, ERR_NOSUCHNICK,
						   form_str(ERR_NOSUCHNICK), nick);
			}

			continue;
		}

		if(IsServer(client_p) && *nick == '=' && nick[1] == '#')
		{
			nick++;
			if((chptr = find_channel(nick)) != NULL)
			{
				if(!duplicate_ptr(chptr))
				{
					if(ntargets >= ConfigFileEntry.max_targets)
					{
						sendto_one(source_p, form_str(ERR_TOOMANYTARGETS),
							   me.name, source_p->name, nick);
						return (1);
					}
					targets[ntargets].ptr = (void *) chptr;
					targets[ntargets++].type = ENTITY_CHANNEL_OPMOD;
				}
			}

			/* non existant channel */
			else if(msgtype != MESSAGE_TYPE_NOTICE)
				sendto_one_numeric(source_p, ERR_NOSUCHNICK,
						   form_str(ERR_NOSUCHNICK), nick);

			continue;
		}

		if(strchr(nick, '@') || (IsOper(source_p) && (*nick == '$')))
		{
			handle_special(msgtype, client_p, source_p, nick, text);
			continue;
		}

		/* no matching anything found - error if not NOTICE */
		if(msgtype != MESSAGE_TYPE_NOTICE)
		{
			/* dont give this numeric when source is local,
			 * because its misleading --anfl
			 */
			if(!MyClient(source_p) && IsDigit(*nick))
				sendto_one(source_p, ":%s %d %s * :Target left IRC. "
					   "Failed to deliver: [%.20s]",
					   get_id(&me, source_p), ERR_NOSUCHNICK,
					   get_id(source_p, source_p), text);
			else
				sendto_one_numeric(source_p, ERR_NOSUCHNICK,
						   form_str(ERR_NOSUCHNICK), nick);
		}

	}
	return (1);
}

/*
 * duplicate_ptr
 *
 * inputs	- pointer to check
 *		- pointer to table of entities
 *		- number of valid entities so far
 * output	- YES if duplicate pointer in table, NO if not.
 *		  note, this does the canonize using pointers
 * side effects	- NONE
 */
static int
duplicate_ptr(void *ptr)
{
	int i;
	for(i = 0; i < ntargets; i++)
		if(targets[i].ptr == ptr)
			return YES;
	return NO;
}

/*
 * msg_channel
 *
 * inputs	- flag privmsg or notice
 *		- pointer to client_p
 *		- pointer to source_p
 *		- pointer to channel
 * output	- NONE
 * side effects	- message given channel
 *
 * XXX - We need to rework this a bit, it's a tad ugly. --nenolod
 */
static void
msg_channel(enum message_type msgtype,
	    struct Client *client_p, struct Client *source_p, struct Channel *chptr,
	    const char *text)
{
	int result;
	hook_data_privmsg_channel hdata;

	if(MyClient(source_p))
	{
		/* idle time shouldnt be reset by notices --fl */
		if(msgtype != MESSAGE_TYPE_NOTICE)
			source_p->localClient->last = rb_current_time();
	}

	hdata.msgtype = msgtype;
	hdata.source_p = source_p;
	hdata.chptr = chptr;
	hdata.text = text;
	hdata.approved = 0;

	call_hook(h_privmsg_channel, &hdata);

	/* memory buffer address may have changed, update pointer */
	text = hdata.text;

	if (hdata.approved != 0)
		return;

	/* hook may have reduced the string to nothing. */
	if (EmptyString(text))
	{
		/* could be empty after colour stripping and
		 * that would cause problems later */
		if(msgtype != MESSAGE_TYPE_NOTICE)
			sendto_one(source_p, form_str(ERR_NOTEXTTOSEND), me.name, source_p->name);
		return;
	}

	/* chanops and voiced can flood their own channel with impunity */
	if((result = can_send(chptr, source_p, NULL)))
	{
		if(result != CAN_SEND_OPV && MyClient(source_p) &&
		   !IsOper(source_p) &&
		   !add_channel_target(source_p, chptr))
		{
			sendto_one(source_p, form_str(ERR_TARGCHANGE),
				   me.name, source_p->name, chptr->chname);
			return;
		}
		if(result == CAN_SEND_OPV ||
		   !flood_attack_channel(msgtype, source_p, chptr, chptr->chname))
		{
			sendto_channel_flags(client_p, ALL_MEMBERS, source_p, chptr,
					     "%s %s :%s", cmdname[msgtype], chptr->chname, text);
		}
	}
	else if(chptr->mode.mode & MODE_OPMODERATE &&
			(!(chptr->mode.mode & MODE_NOPRIVMSGS) ||
			 IsMember(source_p, chptr)))
	{
		if(MyClient(source_p) && !IsOper(source_p) &&
		   !add_channel_target(source_p, chptr))
		{
			sendto_one(source_p, form_str(ERR_TARGCHANGE),
				   me.name, source_p->name, chptr->chname);
			return;
		}
		if(!flood_attack_channel(msgtype, source_p, chptr, chptr->chname))
		{
			sendto_channel_opmod(client_p, source_p, chptr,
					     cmdname[msgtype], text);
		}
	}
	else
	{
		if(msgtype != MESSAGE_TYPE_NOTICE)
			sendto_one_numeric(source_p, ERR_CANNOTSENDTOCHAN,
					   form_str(ERR_CANNOTSENDTOCHAN), chptr->chname);
	}
}
/*
 * msg_channel_opmod
 *
 * inputs	- flag privmsg or notice
 *		- pointer to client_p
 *		- pointer to source_p
 *		- pointer to channel
 * output	- NONE
 * side effects	- message given channel ops
 *
 * XXX - We need to rework this a bit, it's a tad ugly. --nenolod
 */
static void
msg_channel_opmod(enum message_type msgtype,
		  struct Client *client_p, struct Client *source_p,
		  struct Channel *chptr, const char *text)
{
	hook_data_privmsg_channel hdata;

	hdata.msgtype = msgtype;
	hdata.source_p = source_p;
	hdata.chptr = chptr;
	hdata.text = text;
	hdata.approved = 0;

	call_hook(h_privmsg_channel, &hdata);

	/* memory buffer address may have changed, update pointer */
	text = hdata.text;

	if (hdata.approved != 0)
		return;

	/* hook may have reduced the string to nothing. */
	if (EmptyString(text))
	{
		/* could be empty after colour stripping and
		 * that would cause problems later */
		if(msgtype != MESSAGE_TYPE_NOTICE)
			sendto_one(source_p, form_str(ERR_NOTEXTTOSEND), me.name, source_p->name);
		return;
	}

	if(chptr->mode.mode & MODE_OPMODERATE &&
			(!(chptr->mode.mode & MODE_NOPRIVMSGS) ||
			 IsMember(source_p, chptr)))
	{
		if(!flood_attack_channel(msgtype, source_p, chptr, chptr->chname))
		{
			sendto_channel_opmod(client_p, source_p, chptr,
					     cmdname[msgtype], text);
		}
	}
	else
	{
		if(msgtype != MESSAGE_TYPE_NOTICE)
			sendto_one_numeric(source_p, ERR_CANNOTSENDTOCHAN,
					   form_str(ERR_CANNOTSENDTOCHAN), chptr->chname);
	}
}

/*
 * msg_channel_flags
 *
 * inputs	- flag 0 if PRIVMSG 1 if NOTICE. RFC
 *		  say NOTICE must not auto reply
 *		- pointer to client_p
 *		- pointer to source_p
 *		- pointer to channel
 *		- flags
 *		- pointer to text to send
 * output	- NONE
 * side effects	- message given channel either chanop or voice
 */
static void
msg_channel_flags(enum message_type msgtype, struct Client *client_p,
		  struct Client *source_p, struct Channel *chptr, int flags, const char *text)
{
	int type;
	char c;
	hook_data_privmsg_channel hdata;

	if(flags & CHFL_VOICE)
	{
		type = ONLY_CHANOPSVOICED;
		c = '+';
	}
	else
	{
		type = ONLY_CHANOPS;
		c = '@';
	}

	if(MyClient(source_p))
	{
		/* idletime shouldnt be reset by notice --fl */
		if(msgtype != MESSAGE_TYPE_NOTICE)
			source_p->localClient->last = rb_current_time();
	}

	hdata.msgtype = msgtype;
	hdata.source_p = source_p;
	hdata.chptr = chptr;
	hdata.text = text;
	hdata.approved = 0;

	call_hook(h_privmsg_channel, &hdata);

	/* memory buffer address may have changed, update pointer */
	text = hdata.text;

	if (hdata.approved != 0)
		return;

	if (EmptyString(text))
	{
		/* could be empty after colour stripping and
		 * that would cause problems later */
		if(msgtype != MESSAGE_TYPE_NOTICE)
			sendto_one(source_p, form_str(ERR_NOTEXTTOSEND), me.name, source_p->name);
		return;
	}

	sendto_channel_flags(client_p, type, source_p, chptr, "%s %c%s :%s",
			     cmdname[msgtype], c, chptr->chname, text);
}

static void
expire_tgchange(void *unused)
{
	tgchange *target;
	rb_dlink_node *ptr, *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, tgchange_list.head)
	{
		target = ptr->data;

		if(target->expiry < rb_current_time())
		{
			rb_dlinkDelete(ptr, &tgchange_list);
			rb_patricia_remove(tgchange_tree, target->pnode);
			rb_free(target->ip);
			rb_free(target);
		}
	}
}

/*
 * msg_client
 *
 * inputs	- flag 0 if PRIVMSG 1 if NOTICE. RFC
 *		  say NOTICE must not auto reply
 * 		- pointer to source_p source (struct Client *)
 *		- pointer to target_p target (struct Client *)
 *		- pointer to text
 * output	- NONE
 * side effects	- message given channel either chanop or voice
 */
static void
msg_client(enum message_type msgtype,
	   struct Client *source_p, struct Client *target_p, const char *text)
{
	int do_floodcount = 0;
	hook_data_privmsg_user hdata;

	if(MyClient(source_p))
	{
		/*
		 * XXX: Controversial? Allow target users to send replies
		 * through a +g.  Rationale is that people can presently use +g
		 * as a way to taunt users, e.g. harass them and hide behind +g
		 * as a way of griefing.  --nenolod
		 */
		if(msgtype != MESSAGE_TYPE_NOTICE &&
				(IsSetCallerId(source_p) ||
				 (IsSetRegOnlyMsg(source_p) && !target_p->user->suser[0])) &&
				!accept_message(target_p, source_p) &&
				!IsOper(target_p))
		{
			if(rb_dlink_list_length(&source_p->localClient->allow_list) <
					(unsigned long)ConfigFileEntry.max_accept)
			{
				rb_dlinkAddAlloc(target_p, &source_p->localClient->allow_list);
				rb_dlinkAddAlloc(source_p, &target_p->on_allow_list);
			}
			else
			{
				sendto_one_numeric(source_p, ERR_OWNMODE,
						form_str(ERR_OWNMODE),
						target_p->name, "+g");
				return;
			}
		}

		/* reset idle time for message only if its not to self
		 * and its not a notice */
		if(msgtype != MESSAGE_TYPE_NOTICE)
			source_p->localClient->last = rb_current_time();

		/* auto cprivmsg/cnotice */
		do_floodcount = !IsOper(source_p) &&
			!find_allowing_channel(source_p, target_p);

		/* target change stuff, dont limit ctcp replies as that
		 * would allow people to start filling up random users
		 * targets just by ctcping them
		 */
		if((msgtype != MESSAGE_TYPE_NOTICE || *text != '\001') &&
		   ConfigFileEntry.target_change && do_floodcount)
		{
			if(!add_target(source_p, target_p))
			{
				sendto_one(source_p, form_str(ERR_TARGCHANGE),
					   me.name, source_p->name, target_p->name);
				return;
			}
		}

		if (do_floodcount && msgtype == MESSAGE_TYPE_NOTICE && *text == '\001' &&
				target_p->large_ctcp_sent + LARGE_CTCP_TIME >= rb_current_time())
			do_floodcount = 0;

		if (do_floodcount &&
				flood_attack_client(msgtype, source_p, target_p))
			return;
	}
	else if(source_p->from == target_p->from)
	{
		sendto_realops_snomask(SNO_DEBUG, L_ALL,
				     "Send message to %s[%s] dropped from %s(Fake Dir)",
				     target_p->name, target_p->from->name, source_p->name);
		return;
	}

	if(MyConnect(source_p) && (msgtype != MESSAGE_TYPE_NOTICE) && target_p->user && target_p->user->away)
		sendto_one_numeric(source_p, RPL_AWAY, form_str(RPL_AWAY),
				   target_p->name, target_p->user->away);

	if(MyClient(target_p))
	{
		hdata.msgtype = msgtype;
		hdata.source_p = source_p;
		hdata.target_p = target_p;
		hdata.text = text;
		hdata.approved = 0;

		call_hook(h_privmsg_user, &hdata);

		/* buffer location may have changed. */
		text = hdata.text;

		if (hdata.approved != 0)
			return;

		if (EmptyString(text))
		{
			/* could be empty after colour stripping and
			 * that would cause problems later */
			if(msgtype != MESSAGE_TYPE_NOTICE)
				sendto_one(source_p, form_str(ERR_NOTEXTTOSEND), me.name, source_p->name);
			return;
		}

		/* XXX Controversial? allow opers always to send through a +g */
		if(!IsServer(source_p) && (IsSetCallerId(target_p) ||
					(IsSetRegOnlyMsg(target_p) && !source_p->user->suser[0])))
		{
			/* Here is the anti-flood bot/spambot code -db */
			if(accept_message(source_p, target_p) || IsOper(source_p))
			{
				add_reply_target(target_p, source_p);
				sendto_one(target_p, ":%s!%s@%s %s %s :%s",
					   source_p->name,
					   source_p->username,
					   source_p->host, cmdname[msgtype], target_p->name, text);
			}
			else if (IsSetRegOnlyMsg(target_p) && !source_p->user->suser[0])
			{
				if (msgtype != MESSAGE_TYPE_NOTICE)
					sendto_one_numeric(source_p, ERR_NONONREG,
							form_str(ERR_NONONREG),
							target_p->name);
			}
			else
			{
				/* check for accept, flag recipient incoming message */
				if(msgtype != MESSAGE_TYPE_NOTICE)
				{
					sendto_one_numeric(source_p, ERR_TARGUMODEG,
							   form_str(ERR_TARGUMODEG),
							   target_p->name);
				}

				if((target_p->localClient->last_caller_id_time +
				    ConfigFileEntry.caller_id_wait) < rb_current_time())
				{
					if(msgtype != MESSAGE_TYPE_NOTICE)
						sendto_one_numeric(source_p, RPL_TARGNOTIFY,
								   form_str(RPL_TARGNOTIFY),
								   target_p->name);

					add_reply_target(target_p, source_p);
					sendto_one(target_p, form_str(RPL_UMODEGMSG),
						   me.name, target_p->name, source_p->name,
						   source_p->username, source_p->host);

					target_p->localClient->last_caller_id_time = rb_current_time();
				}
			}
		}
		else
		{
			add_reply_target(target_p, source_p);
			sendto_anywhere(target_p, source_p, cmdname[msgtype], ":%s", text);
		}
	}
	else
		sendto_anywhere(target_p, source_p, cmdname[msgtype], ":%s", text);

	return;
}

/*
 * flood_attack_client
 * inputs       - flag 0 if PRIVMSG 1 if NOTICE. RFC
 *                say NOTICE must not auto reply
 *              - pointer to source Client
 *		- pointer to target Client
 * output	- 1 if target is under flood attack
 * side effects	- check for flood attack on target target_p
 */
static int
flood_attack_client(enum message_type msgtype, struct Client *source_p, struct Client *target_p)
{
	int delta;

	/* Services could get many messages legitimately and
	 * can be messaged without rate limiting via aliases
	 * and msg user@server.
	 * -- jilles
	 */
	if(GlobalSetOptions.floodcount && IsClient(source_p) && source_p != target_p && !IsService(target_p))
	{
		if((target_p->first_received_message_time + 1) < rb_current_time())
		{
			delta = rb_current_time() - target_p->first_received_message_time;
			target_p->received_number_of_privmsgs -= delta;
			target_p->first_received_message_time = rb_current_time();
			if(target_p->received_number_of_privmsgs <= 0)
			{
				target_p->received_number_of_privmsgs = 0;
				target_p->flood_noticed = 0;
			}
		}

		if((target_p->received_number_of_privmsgs >=
		    GlobalSetOptions.floodcount) || target_p->flood_noticed)
		{
			if(target_p->flood_noticed == 0)
			{
				sendto_realops_snomask(SNO_BOTS, L_NETWIDE,
						     "Possible Flooder %s[%s@%s] on %s target: %s",
						     source_p->name, source_p->username,
						     source_p->orighost,
						     source_p->servptr->name, target_p->name);
				target_p->flood_noticed = 1;
				/* add a bit of penalty */
				target_p->received_number_of_privmsgs += 2;
			}
			if(MyClient(source_p) && (msgtype != MESSAGE_TYPE_NOTICE))
				sendto_one(source_p,
					   ":%s NOTICE %s :*** Message to %s throttled due to flooding",
					   me.name, source_p->name, target_p->name);
			return 1;
		}
		else
			target_p->received_number_of_privmsgs++;
	}

	return 0;
}

/*
 * handle_special
 *
 * inputs	- server pointer
 *		- client pointer
 *		- nick stuff to grok for opers
 *		- text to send if grok
 * output	- none
 * side effects	- all the traditional oper type messages are parsed here.
 *		  i.e. "/msg #some.host."
 *		  However, syntax has been changed.
 *		  previous syntax "/msg #some.host.mask"
 *		  now becomes     "/msg $#some.host.mask"
 *		  previous syntax of: "/msg $some.server.mask" remains
 *		  This disambiguates the syntax.
 */
static void
handle_special(enum message_type msgtype, struct Client *client_p,
	       struct Client *source_p, const char *nick, const char *text)
{
	struct Client *target_p;
	char *server;
	char *s;

	/* user[%host]@server addressed?
	 * NOTE: users can send to user@server, but not user%host@server
	 * or opers@server
	 */
	if((server = strchr(nick, '@')) != NULL)
	{
		if((target_p = find_server(source_p, server + 1)) == NULL)
		{
			sendto_one_numeric(source_p, ERR_NOSUCHSERVER,
					   form_str(ERR_NOSUCHSERVER), server + 1);
			return;
		}

		if(!IsOper(source_p))
		{
			if(strchr(nick, '%') || (strncmp(nick, "opers", 5) == 0))
			{
				sendto_one_numeric(source_p, ERR_NOSUCHNICK,
						   form_str(ERR_NOSUCHNICK), nick);
				return;
			}
		}

		/* somewhere else.. */
		if(!IsMe(target_p))
		{
			sendto_one(target_p, ":%s %s %s :%s",
				   get_id(source_p, target_p), cmdname[msgtype], nick, text);
			return;
		}

		/* Check if someones msg'ing opers@our.server */
		if(strncmp(nick, "opers@", 6) == 0)
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL, "To opers: From: %s: %s",
					     source_p->name, text);
			return;
		}

		/* This was not very useful except for bypassing certain
		 * restrictions. Note that we still allow sending to
		 * remote servers this way, for messaging pseudoservers
		 * securely whether they have a service{} block or not.
		 * -- jilles
		 */
		sendto_one_numeric(source_p, ERR_NOSUCHNICK,
				   form_str(ERR_NOSUCHNICK), nick);
		return;
	}

	/*
	 * the following two cases allow masks in NOTICEs
	 * (for OPERs only)
	 *
	 * Armin, 8Jun90 (gruner@informatik.tu-muenchen.de)
	 */
	if(IsOper(source_p) && *nick == '$')
	{
		if((*(nick + 1) == '$' || *(nick + 1) == '#'))
			nick++;
		else if(MyOper(source_p))
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :The command %s %s is no longer supported, please use $%s",
				   me.name, source_p->name, cmdname[msgtype], nick, nick);
			return;
		}

		if(MyClient(source_p) && !IsOperMassNotice(source_p))
		{
			sendto_one(source_p, form_str(ERR_NOPRIVS),
				   me.name, source_p->name, "mass_notice");
			return;
		}

		if((s = strrchr(nick, '.')) == NULL)
		{
			sendto_one_numeric(source_p, ERR_NOTOPLEVEL,
					   form_str(ERR_NOTOPLEVEL), nick);
			return;
		}
		while(*++s)
			if(*s == '.' || *s == '*' || *s == '?')
				break;
		if(*s == '*' || *s == '?')
		{
			sendto_one_numeric(source_p, ERR_WILDTOPLEVEL,
					   form_str(ERR_WILDTOPLEVEL), nick);
			return;
		}

		sendto_match_butone(IsServer(client_p) ? client_p : NULL, source_p,
				    nick + 1,
				    (*nick == '#') ? MATCH_HOST : MATCH_SERVER,
				    "%s $%s :%s", cmdname[msgtype], nick, text);
		if (msgtype != MESSAGE_TYPE_NOTICE && *text == '\001')
			source_p->large_ctcp_sent = rb_current_time();
		return;
	}
}
