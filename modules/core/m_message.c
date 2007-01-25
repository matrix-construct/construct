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
 *  $Id: m_message.c 1761 2006-07-27 19:27:49Z jilles $
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
#include "irc_string.h"
#include "hash.h"
#include "class.h"
#include "msg.h"
#include "packet.h"
#include "send.h"
#include "event.h"
#include "patricia.h"
#include "s_newconf.h"

static int m_message(int, const char *, struct Client *, struct Client *, int, const char **);
static int m_privmsg(struct Client *, struct Client *, int, const char **);
static int m_notice(struct Client *, struct Client *, int, const char **);

static void expire_tgchange(void *unused);

static int
modinit(void)
{
	eventAddIsh("expire_tgchange", expire_tgchange, NULL, 300);
	expire_tgchange(NULL);
	return 0;
}

static void
moddeinit(void)
{
	eventDelete(expire_tgchange, NULL);
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

DECLARE_MODULE_AV1(message, modinit, moddeinit, message_clist, NULL, NULL, "$Revision: 1761 $");

struct entity
{
	void *ptr;
	int type;
	int flags;
};

static int build_target_list(int p_or_n, const char *command,
			     struct Client *client_p,
			     struct Client *source_p, const char *nicks_channels, const char *text);

static int flood_attack_client(int p_or_n, struct Client *source_p, struct Client *target_p);
static int flood_attack_channel(int p_or_n, struct Client *source_p,
				struct Channel *chptr, char *chname);
static struct Client *find_userhost(const char *, const char *, int *);

#define ENTITY_NONE    0
#define ENTITY_CHANNEL 1
#define ENTITY_CHANOPS_ON_CHANNEL 2
#define ENTITY_CLIENT  3

static struct entity targets[512];
static int ntargets = 0;

static int duplicate_ptr(void *);

static void msg_channel(int p_or_n, const char *command,
			struct Client *client_p,
			struct Client *source_p, struct Channel *chptr, const char *text);

static void msg_channel_flags(int p_or_n, const char *command,
			      struct Client *client_p,
			      struct Client *source_p,
			      struct Channel *chptr, int flags, const char *text);

static void msg_client(int p_or_n, const char *command,
		       struct Client *source_p, struct Client *target_p, const char *text);

static void handle_special(int p_or_n, const char *command,
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

#define PRIVMSG 0
#define NOTICE  1

static int
m_privmsg(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	return m_message(PRIVMSG, "PRIVMSG", client_p, source_p, parc, parv);
}

static int
m_notice(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	return m_message(NOTICE, "NOTICE", client_p, source_p, parc, parv);
}

/*
 * inputs	- flag privmsg or notice
 * 		- pointer to command "PRIVMSG" or "NOTICE"
 *		- pointer to client_p
 *		- pointer to source_p
 *		- pointer to channel
 */
static int
m_message(int p_or_n,
	  const char *command,
	  struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	int i;

	if(parc < 2 || EmptyString(parv[1]))
	{
		if(p_or_n != NOTICE)
			sendto_one(source_p, form_str(ERR_NORECIPIENT), me.name,
				   source_p->name, command);
		return 0;
	}

	if(parc < 3 || EmptyString(parv[2]))
	{
		if(p_or_n != NOTICE)
			sendto_one(source_p, form_str(ERR_NOTEXTTOSEND), me.name, source_p->name);
		return 0;
	}

	/* Finish the flood grace period if theyre not messaging themselves
	 * as some clients (ircN) do this as a "lag check"
	 */
	if(MyClient(source_p) && !IsFloodDone(source_p) && irccmp(source_p->name, parv[1]))
		flood_endgrace(source_p);

	if(build_target_list(p_or_n, command, client_p, source_p, parv[1], parv[2]) < 0)
	{
		return 0;
	}

	for(i = 0; i < ntargets; i++)
	{
		switch (targets[i].type)
		{
		case ENTITY_CHANNEL:
			msg_channel(p_or_n, command, client_p, source_p,
				    (struct Channel *) targets[i].ptr, parv[2]);
			break;

		case ENTITY_CHANOPS_ON_CHANNEL:
			msg_channel_flags(p_or_n, command, client_p, source_p,
					  (struct Channel *) targets[i].ptr,
					  targets[i].flags, parv[2]);
			break;

		case ENTITY_CLIENT:
			msg_client(p_or_n, command, source_p,
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
build_target_list(int p_or_n, const char *command, struct Client *client_p,
		  struct Client *source_p, const char *nicks_channels, const char *text)
{
	int type;
	char *p, *nick, *target_list;
	struct Channel *chptr = NULL;
	struct Client *target_p;

	target_list = LOCAL_COPY(nicks_channels);	/* skip strcpy for non-lazyleafs */

	ntargets = 0;

	for(nick = strtoken(&p, target_list, ","); nick; nick = strtoken(&p, NULL, ","))
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
			else if(p_or_n != NOTICE)
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
					   me.name, source_p->name, command);
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
						   me.name, source_p->name, with_prefix);
					return (-1);
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
			else if(p_or_n != NOTICE)
			{
				sendto_one_numeric(source_p, ERR_NOSUCHNICK,
						   form_str(ERR_NOSUCHNICK), nick);
			}

			continue;
		}

		if(strchr(nick, '@') || (IsOper(source_p) && (*nick == '$')))
		{
			handle_special(p_or_n, command, client_p, source_p, nick, text);
			continue;
		}

		/* no matching anything found - error if not NOTICE */
		if(p_or_n != NOTICE)
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
 * 		- pointer to command "PRIVMSG" or "NOTICE"
 *		- pointer to client_p
 *		- pointer to source_p
 *		- pointer to channel
 * output	- NONE
 * side effects	- message given channel
 *
 * XXX - We need to rework this a bit, it's a tad ugly. --nenolod
 */
static void
msg_channel(int p_or_n, const char *command,
	    struct Client *client_p, struct Client *source_p, struct Channel *chptr,
	    const char *text)
{
	int result;
	char text2[BUFSIZE];

	if(MyClient(source_p))
	{
		/* idle time shouldnt be reset by notices --fl */
		if(p_or_n != NOTICE)
			source_p->localClient->last = CurrentTime;
	}

	if(chptr->mode.mode & MODE_NOCOLOR)
	{
		strlcpy(text2, text, BUFSIZE);
		strip_colour(text2);
		text = text2;
		if (EmptyString(text))
		{
			/* could be empty after colour stripping and
			 * that would cause problems later */
			if(p_or_n != NOTICE)
				sendto_one(source_p, form_str(ERR_NOTEXTTOSEND), me.name, source_p->name);
			return;
		}
	}

	/* chanops and voiced can flood their own channel with impunity */
	if((result = can_send(chptr, source_p, NULL)))
	{
		if(result == CAN_SEND_OPV ||
		   !flood_attack_channel(p_or_n, source_p, chptr, chptr->chname))
		{
			sendto_channel_flags(client_p, ALL_MEMBERS, source_p, chptr,
					     "%s %s :%s", command, chptr->chname, text);
		}
	}
	else if(chptr->mode.mode & MODE_OPMODERATE &&
			chptr->mode.mode & MODE_MODERATED &&
			IsMember(source_p, chptr))
	{
		/* only do +z for +m channels for now, as bans/quiets
		 * aren't tested for remote clients -- jilles */
		if(!flood_attack_channel(p_or_n, source_p, chptr, chptr->chname))
		{
			sendto_channel_flags(client_p, ONLY_CHANOPS, source_p, chptr,
					     "%s %s :%s", command, chptr->chname, text);
		}
	}
	else
	{
		if(p_or_n != NOTICE)
			sendto_one_numeric(source_p, ERR_CANNOTSENDTOCHAN,
					   form_str(ERR_CANNOTSENDTOCHAN), chptr->chname);
	}
}

/*
 * msg_channel_flags
 *
 * inputs	- flag 0 if PRIVMSG 1 if NOTICE. RFC 
 *		  say NOTICE must not auto reply
 *		- pointer to command, "PRIVMSG" or "NOTICE"
 *		- pointer to client_p
 *		- pointer to source_p
 *		- pointer to channel
 *		- flags
 *		- pointer to text to send
 * output	- NONE
 * side effects	- message given channel either chanop or voice
 */
static void
msg_channel_flags(int p_or_n, const char *command, struct Client *client_p,
		  struct Client *source_p, struct Channel *chptr, int flags, const char *text)
{
	int type;
	char c;

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
		if(p_or_n != NOTICE)
			source_p->localClient->last = CurrentTime;
	}

	sendto_channel_flags(client_p, type, source_p, chptr, "%s %c%s :%s",
			     command, c, chptr->chname, text);
}

#define PREV_FREE_TARGET(x) ((FREE_TARGET(x) == 0) ? 9 : FREE_TARGET(x) - 1)
#define PREV_TARGET(i) ((i == 0) ? i = 9 : --i)
#define NEXT_TARGET(i) ((i == 9) ? i = 0 : ++i)

static void
expire_tgchange(void *unused)
{
	tgchange *target;
	dlink_node *ptr, *next_ptr;

	DLINK_FOREACH_SAFE(ptr, next_ptr, tgchange_list.head)
	{
		target = ptr->data;

		if(target->expiry < CurrentTime)
		{
			dlinkDelete(ptr, &tgchange_list);
			patricia_remove(tgchange_tree, target->pnode);
			MyFree(target->ip);
			MyFree(target);
		}
	}
}

static int
add_target(struct Client *source_p, struct Client *target_p)
{
	int i, j;

	/* can msg themselves or services without using any target slots */
	if(source_p == target_p || IsService(target_p))
		return 1;

	/* special condition for those who have had PRIVMSG crippled to allow them
	 * to talk to IRCops still.
	 *
	 * XXX: is this controversial?
	 */
	if(source_p->localClient->target_last > CurrentTime && IsOper(target_p))
		return 1;

	if(USED_TARGETS(source_p))
	{
		/* hunt for an existing target */
		for(i = PREV_FREE_TARGET(source_p), j = USED_TARGETS(source_p);
		    j; --j, PREV_TARGET(i))
		{
			if(source_p->localClient->targets[i] == target_p)
				return 1;
		}

		/* first message after connect, we may only start clearing
		 * slots after this message --anfl
		 */
		if(!IsTGChange(source_p))
		{
			SetTGChange(source_p);
			source_p->localClient->target_last = CurrentTime;
		}
		/* clear as many targets as we can */
		else if((i = (CurrentTime - source_p->localClient->target_last) / 60))
		{
			if(i > USED_TARGETS(source_p))
				USED_TARGETS(source_p) = 0;
			else
				USED_TARGETS(source_p) -= i;

			source_p->localClient->target_last = CurrentTime;
		}
		/* cant clear any, full target list */
		else if(USED_TARGETS(source_p) == 10)
		{
			add_tgchange(source_p->sockhost);
			return 0;
		}
	}
	/* no targets in use, reset their target_last so that they cant
	 * abuse a long idle to get targets back more quickly
	 */
	else
	{
		source_p->localClient->target_last = CurrentTime;
		SetTGChange(source_p);
	}

	source_p->localClient->targets[FREE_TARGET(source_p)] = target_p;
	NEXT_TARGET(FREE_TARGET(source_p));
	++USED_TARGETS(source_p);
	return 1;
}

/*
 * msg_client
 *
 * inputs	- flag 0 if PRIVMSG 1 if NOTICE. RFC 
 *		  say NOTICE must not auto reply
 *		- pointer to command, "PRIVMSG" or "NOTICE"
 * 		- pointer to source_p source (struct Client *)
 *		- pointer to target_p target (struct Client *)
 *		- pointer to text
 * output	- NONE
 * side effects	- message given channel either chanop or voice
 */
static void
msg_client(int p_or_n, const char *command,
	   struct Client *source_p, struct Client *target_p, const char *text)
{
	if(MyClient(source_p))
	{
		/* reset idle time for message only if its not to self 
		 * and its not a notice */
		if(p_or_n != NOTICE)
			source_p->localClient->last = CurrentTime;

		/* target change stuff, dont limit ctcp replies as that
		 * would allow people to start filling up random users
		 * targets just by ctcping them
		 */
		if((p_or_n != NOTICE || *text != '\001') &&
		   ConfigFileEntry.target_change && !IsOper(source_p))
		{
			if(!add_target(source_p, target_p))
			{
				sendto_one(source_p, form_str(ERR_TARGCHANGE),
					   me.name, source_p->name, target_p->name);
				return;
			}
		}
	}
	else if(source_p->from == target_p->from)
	{
		sendto_realops_snomask(SNO_DEBUG, L_ALL,
				     "Send message to %s[%s] dropped from %s(Fake Dir)",
				     target_p->name, target_p->from->name, source_p->name);
		return;
	}

	if(MyConnect(source_p) && (p_or_n != NOTICE) && target_p->user && target_p->user->away)
		sendto_one_numeric(source_p, RPL_AWAY, form_str(RPL_AWAY),
				   target_p->name, target_p->user->away);

	if(MyClient(target_p))
	{
		/* XXX Controversial? allow opers always to send through a +g */
		if(!IsServer(source_p) && (IsSetCallerId(target_p) ||
					(IsSetRegOnlyMsg(target_p) && !source_p->user->suser[0])))
		{
			/* Here is the anti-flood bot/spambot code -db */
			if(accept_message(source_p, target_p) || IsOper(source_p))
			{
				sendto_one(target_p, ":%s!%s@%s %s %s :%s",
					   source_p->name,
					   source_p->username,
					   source_p->host, command, target_p->name, text);
			}
			else if (IsSetRegOnlyMsg(target_p) && !source_p->user->suser[0])
			{
				if (p_or_n != NOTICE)
					sendto_one_numeric(source_p, ERR_NONONREG,
							form_str(ERR_NONONREG),
							target_p->name);
				/* Only so opers can watch for floods */
				(void) flood_attack_client(p_or_n, source_p, target_p);
			}
			else
			{
				/* check for accept, flag recipient incoming message */
				if(p_or_n != NOTICE)
				{
					sendto_one_numeric(source_p, ERR_TARGUMODEG,
							   form_str(ERR_TARGUMODEG),
							   target_p->name);
				}

				if((target_p->localClient->last_caller_id_time +
				    ConfigFileEntry.caller_id_wait) < CurrentTime)
				{
					if(p_or_n != NOTICE)
						sendto_one_numeric(source_p, RPL_TARGNOTIFY,
								   form_str(RPL_TARGNOTIFY),
								   target_p->name);

					sendto_one(target_p, form_str(RPL_UMODEGMSG),
						   me.name, target_p->name, source_p->name,
						   source_p->username, source_p->host);

					target_p->localClient->last_caller_id_time = CurrentTime;
				}
				/* Only so opers can watch for floods */
				(void) flood_attack_client(p_or_n, source_p, target_p);
			}
		}
		else
		{
			/* If the client is remote, we dont perform a special check for
			 * flooding.. as we wouldnt block their message anyway.. this means
			 * we dont give warnings.. we then check if theyre opered 
			 * (to avoid flood warnings), lastly if theyre our client
			 * and flooding    -- fl */
			if(!MyClient(source_p) || IsOper(source_p) ||
			   !flood_attack_client(p_or_n, source_p, target_p))
				sendto_anywhere(target_p, source_p, command, ":%s", text);
		}
	}
	else if(!MyClient(source_p) || IsOper(source_p) ||
		!flood_attack_client(p_or_n, source_p, target_p))
		sendto_anywhere(target_p, source_p, command, ":%s", text);

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
flood_attack_client(int p_or_n, struct Client *source_p, struct Client *target_p)
{
	int delta;

	if(GlobalSetOptions.floodcount && MyConnect(target_p) && IsClient(source_p))
	{
		if((target_p->localClient->first_received_message_time + 1) < CurrentTime)
		{
			delta = CurrentTime - target_p->localClient->first_received_message_time;
			target_p->localClient->received_number_of_privmsgs -= delta;
			target_p->localClient->first_received_message_time = CurrentTime;
			if(target_p->localClient->received_number_of_privmsgs <= 0)
			{
				target_p->localClient->received_number_of_privmsgs = 0;
				target_p->localClient->flood_noticed = 0;
			}
		}

		if((target_p->localClient->received_number_of_privmsgs >=
		    GlobalSetOptions.floodcount) || target_p->localClient->flood_noticed)
		{
			if(target_p->localClient->flood_noticed == 0)
			{
				sendto_realops_snomask(SNO_BOTS, L_ALL,
						     "Possible Flooder %s[%s@%s] on %s target: %s",
						     source_p->name, source_p->username,
						     source_p->host,
						     source_p->user->server, target_p->name);
				target_p->localClient->flood_noticed = 1;
				/* add a bit of penalty */
				target_p->localClient->received_number_of_privmsgs += 2;
			}
			if(MyClient(source_p) && (p_or_n != NOTICE))
				sendto_one(source_p,
					   ":%s NOTICE %s :*** Message to %s throttled due to flooding",
					   me.name, source_p->name, target_p->name);
			return 1;
		}
		else
			target_p->localClient->received_number_of_privmsgs++;
	}

	return 0;
}

/*
 * flood_attack_channel
 * inputs       - flag 0 if PRIVMSG 1 if NOTICE. RFC
 *                says NOTICE must not auto reply
 *              - pointer to source Client 
 *		- pointer to target channel
 * output	- 1 if target is under flood attack
 * side effects	- check for flood attack on target chptr
 */
static int
flood_attack_channel(int p_or_n, struct Client *source_p, struct Channel *chptr, char *chname)
{
	int delta;

	if(GlobalSetOptions.floodcount && MyClient(source_p))
	{
		if((chptr->first_received_message_time + 1) < CurrentTime)
		{
			delta = CurrentTime - chptr->first_received_message_time;
			chptr->received_number_of_privmsgs -= delta;
			chptr->first_received_message_time = CurrentTime;
			if(chptr->received_number_of_privmsgs <= 0)
			{
				chptr->received_number_of_privmsgs = 0;
				chptr->flood_noticed = 0;
			}
		}

		if((chptr->received_number_of_privmsgs >= GlobalSetOptions.floodcount)
		   || chptr->flood_noticed)
		{
			if(chptr->flood_noticed == 0)
			{
				sendto_realops_snomask(SNO_BOTS, L_ALL,
						     "Possible Flooder %s[%s@%s] on %s target: %s",
						     source_p->name, source_p->username,
						     source_p->host,
						     source_p->user->server, chptr->chname);
				chptr->flood_noticed = 1;

				/* Add a bit of penalty */
				chptr->received_number_of_privmsgs += 2;
			}
			if(MyClient(source_p) && (p_or_n != NOTICE))
				sendto_one(source_p,
					   ":%s NOTICE %s :*** Message to %s throttled due to flooding",
					   me.name, source_p->name, chptr->chname);
			return 1;
		}
		else
			chptr->received_number_of_privmsgs++;
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
handle_special(int p_or_n, const char *command, struct Client *client_p,
	       struct Client *source_p, const char *nick, const char *text)
{
	struct Client *target_p;
	char *host;
	char *server;
	char *s;
	int count;

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

		count = 0;

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
				   get_id(source_p, target_p), command, nick, text);
			return;
		}

		*server = '\0';

		if((host = strchr(nick, '%')) != NULL)
			*host++ = '\0';

		/* Check if someones msg'ing opers@our.server */
		if(strcmp(nick, "opers") == 0)
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL, "To opers: From: %s: %s",
					     source_p->name, text);
			return;
		}

		/*
		 * Look for users which match the destination host
		 * (no host == wildcard) and if one and one only is
		 * found connected to me, deliver message!
		 */
		target_p = find_userhost(nick, host, &count);

		if(target_p != NULL)
		{
			if(server != NULL)
				*server = '@';
			if(host != NULL)
				*--host = '%';

			if(count == 1)
				sendto_anywhere(target_p, source_p, command, ":%s", text);
			else
				sendto_one(source_p, form_str(ERR_TOOMANYTARGETS),
					   get_id(&me, source_p), get_id(source_p, source_p), nick);
		}
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
				   me.name, source_p->name, command, nick, nick);
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
				    "%s $%s :%s", command, nick, text);
		return;
	}
}

/*
 * find_userhost - find a user@host (server or user).
 * inputs       - user name to look for
 *              - host name to look for
 *		- pointer to count of number of matches found
 * outputs	- pointer to client if found
 *		- count is updated
 * side effects	- none
 *
 */
static struct Client *
find_userhost(const char *user, const char *host, int *count)
{
	struct Client *c2ptr;
	struct Client *res = NULL;
	char *u = LOCAL_COPY(user);
	dlink_node *ptr;
	*count = 0;
	if(collapse(u) != NULL)
	{
		DLINK_FOREACH(ptr, global_client_list.head)
		{
			c2ptr = ptr->data;
			if(!MyClient(c2ptr))	/* implies mine and an user */
				continue;
			if((!host || match(host, c2ptr->host)) && irccmp(u, c2ptr->username) == 0)
			{
				(*count)++;
				res = c2ptr;
			}
		}
	}
	return (res);
}
