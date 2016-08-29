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
 */

using namespace ircd;

static const char message_desc[] =
	"Provides the PRIVMSG and NOTICE commands to send messages to users and channels";

static void m_message(enum message_type, struct MsgBuf *, client::client &, client::client &, int, const char **);
static void m_privmsg(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void m_notice(struct MsgBuf *, client::client &, client::client &, int, const char **);

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
	"PRIVMSG", 0, 0, 0, 0,
	{mg_unreg, {m_privmsg, 0}, {m_privmsg, 0}, mg_ignore, mg_ignore, {m_privmsg, 0}}
};
struct Message notice_msgtab = {
	"NOTICE", 0, 0, 0, 0,
	{mg_unreg, {m_notice, 0}, {m_notice, 0}, {m_notice, 0}, mg_ignore, {m_notice, 0}}
};

mapi_clist_av1 message_clist[] = { &privmsg_msgtab, &notice_msgtab, NULL };

DECLARE_MODULE_AV2(message, modinit, moddeinit, message_clist, NULL, NULL, NULL, NULL, message_desc);

struct entity
{
	void *ptr;
	int type;
	int flags;
};

static int build_target_list(enum message_type msgtype,
			     client::client &client,
			     client::client &source, const char *nicks_channels, const char *text);

static bool flood_attack_client(enum message_type msgtype, client::client &source, client::client *target_p);

/* Fifteen seconds should be plenty for a client to reply a ctcp */
#define LARGE_CTCP_TIME 15

#define ENTITY_NONE    0
#define ENTITY_CHANNEL 1
#define ENTITY_CHANNEL_OPMOD 2
#define ENTITY_CHANOPS_ON_CHANNEL 3
#define ENTITY_CLIENT  4

static struct entity targets[512];
static int ntargets = 0;

static bool duplicate_ptr(void *);

static void msg_channel(enum message_type msgtype,
			client::client &client,
			client::client &source, chan::chan *chptr, const char *text);

static void msg_channel_opmod(enum message_type msgtype,
			      client::client &client,
			      client::client &source, chan::chan *chptr,
			      const char *text);

static void msg_channel_flags(enum message_type msgtype,
			      client::client &client,
			      client::client &source,
			      chan::chan *chptr, int flags, const char *text);

static void msg_client(enum message_type msgtype,
		       client::client &source, client::client *target_p, const char *text);

static void handle_special(enum message_type msgtype,
			   client::client &client, client::client &source, const char *nick,
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
const char *cmdname[MESSAGE_TYPE_COUNT]
{
	"NOTICE",
	"PRIVMSG",
	nullptr,
};

static void
m_privmsg(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	m_message(MESSAGE_TYPE_PRIVMSG, msgbuf_p, client, source, parc, parv);
}

static void
m_notice(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	m_message(MESSAGE_TYPE_NOTICE, msgbuf_p, client, source, parc, parv);
}

/*
 * inputs	- flag privmsg or notice
 *		- pointer to &client
 *		- pointer to &source
 *		- pointer to channel
 */
static void
m_message(enum message_type msgtype, struct MsgBuf *msgbuf_p,
	  client::client &client, client::client &source, int parc, const char *parv[])
{
	int i;

	if(parc < 2 || EmptyString(parv[1]))
	{
		if(msgtype != MESSAGE_TYPE_NOTICE)
			sendto_one(&source, form_str(ERR_NORECIPIENT), me.name,
				   source.name, cmdname[msgtype]);
		return;
	}

	if(parc < 3 || EmptyString(parv[2]))
	{
		if(msgtype != MESSAGE_TYPE_NOTICE)
			sendto_one(&source, form_str(ERR_NOTEXTTOSEND), me.name, source.name);
		return;
	}

	/* Finish the flood grace period if theyre not messaging themselves
	 * as some clients (ircN) do this as a "lag check"
	 */
	if(my(source) && !is_flood_done(source) && irccmp(source.name, parv[1]))
		flood_endgrace(&source);

	if(build_target_list(msgtype, client, source, parv[1], parv[2]) < 0)
	{
		return;
	}

	for(i = 0; i < ntargets; i++)
	{
		switch (targets[i].type)
		{
		case ENTITY_CHANNEL:
			msg_channel(msgtype, client, source,
				    (chan::chan *) targets[i].ptr, parv[2]);
			break;

		case ENTITY_CHANNEL_OPMOD:
			msg_channel_opmod(msgtype, client, source,
				   (chan::chan *) targets[i].ptr, parv[2]);
			break;

		case ENTITY_CHANOPS_ON_CHANNEL:
			msg_channel_flags(msgtype, client, source,
					  (chan::chan *) targets[i].ptr,
					  targets[i].flags, parv[2]);
			break;

		case ENTITY_CLIENT:
			msg_client(msgtype, source,
				   (client::client *) targets[i].ptr, parv[2]);
			break;
		}
	}
}

/*
 * build_target_list
 *
 * inputs	- pointer to given &client (server)
 *		- pointer to given source (oper/client etc.)
 *		- pointer to list of nicks/channels
 *		- pointer to table to place results
 *		- pointer to text (only used if &source is an oper)
 * output	- number of valid entities
 * side effects	- target_table is modified to contain a list of
 *		  pointers to channels or clients
 *		  if source client is an oper
 *		  all the classic old bizzare oper privmsg tricks
 *		  are parsed and sent as is, if prefixed with $
 *		  to disambiguate.
 */

static int
build_target_list(enum message_type msgtype, client::client &client,
		  client::client &source, const char *nicks_channels, const char *text)
{
	int type;
	char *p, *nick, *target_list;
	chan::chan *chptr = NULL;
	client::client *target_p;

	target_list = LOCAL_COPY(nicks_channels);	/* skip strcpy for non-lazyleafs */

	ntargets = 0;

	for(nick = rb_strtok_r(target_list, ",", &p); nick; nick = rb_strtok_r(NULL, ",", &p))
	{
		char *with_prefix;
		/*
		 * channels are privmsg'd a lot more than other clients, moved up
		 * here plain old channel msg?
		 */

		if(rfc1459::is_chan_prefix(*nick))
		{
			/* ignore send of local channel to a server (should not happen) */
			if(is_server(client) && *nick == '&')
				continue;

			if((chptr = chan::get(nick, std::nothrow)) != NULL)
			{
				if(!duplicate_ptr(chptr))
				{
					if(ntargets >= ConfigFileEntry.max_targets)
					{
						sendto_one(&source, form_str(ERR_TOOMANYTARGETS),
							   me.name, source.name, nick);
						return (1);
					}
					targets[ntargets].ptr = (void *) chptr;
					targets[ntargets++].type = ENTITY_CHANNEL;
				}
			}

			/* non existant channel */
			else if(msgtype != MESSAGE_TYPE_NOTICE)
				sendto_one_numeric(&source, ERR_NOSUCHNICK,
						   form_str(ERR_NOSUCHNICK), nick);

			continue;
		}

		if(my(source))
			target_p = client::find_named_person(nick);
		else
			target_p = client::find_person(nick);

		/* look for a privmsg to another client */
		if(target_p)
		{
			if(!duplicate_ptr(target_p))
			{
				if(ntargets >= ConfigFileEntry.max_targets)
				{
					sendto_one(&source, form_str(ERR_TOOMANYTARGETS),
						   me.name, source.name, nick);
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
				type |= chan::CHANOP;
			else if(*nick == '+')
				type |= chan::CHANOP | chan::VOICE;
			else
				break;
			nick++;
		}

		if(type != 0)
		{
			/* no recipient.. */
			if(EmptyString(nick))
			{
				sendto_one(&source, form_str(ERR_NORECIPIENT),
					   me.name, source.name, cmdname[msgtype]);
				continue;
			}

			/* At this point, nick+1 should be a channel name i.e. #foo or &foo
			 * if the channel is found, fine, if not report an error
			 */

			if((chptr = chan::get(nick, std::nothrow)) != NULL)
			{
				chan::membership *msptr;

				msptr = get(chptr->members, source, std::nothrow);

				if(!is_server(source) && !is(source, umode::SERVICE) && !is_chanop(msptr) && !is_voiced(msptr))
				{
					sendto_one(&source, form_str(ERR_CHANOPRIVSNEEDED),
						   get_id(&me, &source),
						   get_id(&source, &source),
						   with_prefix);
					continue;
				}

				if(!duplicate_ptr(chptr))
				{
					if(ntargets >= ConfigFileEntry.max_targets)
					{
						sendto_one(&source, form_str(ERR_TOOMANYTARGETS),
							   me.name, source.name, nick);
						return (1);
					}
					targets[ntargets].ptr = (void *) chptr;
					targets[ntargets].type = ENTITY_CHANOPS_ON_CHANNEL;
					targets[ntargets++].flags = type;
				}
			}
			else if(msgtype != MESSAGE_TYPE_NOTICE)
			{
				sendto_one_numeric(&source, ERR_NOSUCHNICK,
						   form_str(ERR_NOSUCHNICK), nick);
			}

			continue;
		}

		if(is_server(client) && *nick == '=' && nick[1] == '#')
		{
			nick++;
			if((chptr = chan::get(nick, std::nothrow)) != NULL)
			{
				if(!duplicate_ptr(chptr))
				{
					if(ntargets >= ConfigFileEntry.max_targets)
					{
						sendto_one(&source, form_str(ERR_TOOMANYTARGETS),
							   me.name, source.name, nick);
						return (1);
					}
					targets[ntargets].ptr = (void *) chptr;
					targets[ntargets++].type = ENTITY_CHANNEL_OPMOD;
				}
			}

			/* non existant channel */
			else if(msgtype != MESSAGE_TYPE_NOTICE)
				sendto_one_numeric(&source, ERR_NOSUCHNICK,
						   form_str(ERR_NOSUCHNICK), nick);

			continue;
		}

		if(strchr(nick, '@') || (is(source, umode::OPER) && (*nick == '$')))
		{
			handle_special(msgtype, client, source, nick, text);
			continue;
		}

		/* no matching anything found - error if not NOTICE */
		if(msgtype != MESSAGE_TYPE_NOTICE)
		{
			/* dont give this numeric when source is local,
			 * because its misleading --anfl
			 */
			if(!my(source) && rfc1459::is_digit(*nick))
				sendto_one(&source, ":%s %d %s * :Target left IRC. "
					   "Failed to deliver: [%.20s]",
					   get_id(&me, &source), ERR_NOSUCHNICK,
					   get_id(&source, &source), text);
			else
				sendto_one_numeric(&source, ERR_NOSUCHNICK,
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
 * output	- true if duplicate pointer in table, false if not.
 *		  note, this does the canonize using pointers
 * side effects	- NONE
 */
static bool
duplicate_ptr(void *ptr)
{
	int i;
	for(i = 0; i < ntargets; i++)
		if(targets[i].ptr == ptr)
			return true;
	return false;
}

/*
 * msg_channel
 *
 * inputs	- flag privmsg or notice
 *		- pointer to &client
 *		- pointer to &source
 *		- pointer to channel
 * output	- NONE
 * side effects	- message given channel
 *
 * XXX - We need to rework this a bit, it's a tad ugly. --nenolod
 */
static void
msg_channel(enum message_type msgtype,
	    client::client &client, client::client &source, chan::chan *chptr,
	    const char *text)
{
	int result;
	hook_data_privmsg_channel hdata;

	if(my(source))
	{
		/* idle time shouldnt be reset by notices --fl */
		if(msgtype != MESSAGE_TYPE_NOTICE)
			source.localClient->last = rb_current_time();
	}

	hdata.msgtype = msgtype;
	hdata.source_p = &source;
	hdata.chptr = chptr;
	hdata.text = text;
	hdata.approved = 0;
	hdata.reason = NULL;

	call_hook(h_privmsg_channel, &hdata);

	/* memory buffer address may have changed, update pointer */
	text = hdata.text;

	if (hdata.approved != 0)
	{
		if (msgtype == MESSAGE_TYPE_PRIVMSG)
		{
			if (!EmptyString(hdata.reason))
				sendto_one_numeric(&source, ERR_CANNOTSENDTOCHAN,
				                   form_str(ERR_CANNOTSENDTOCHAN)" (%s).",
				                   chptr->name.c_str(),
				                   hdata.reason);
			else
				sendto_one_numeric(&source, ERR_CANNOTSENDTOCHAN,
				                   form_str(ERR_CANNOTSENDTOCHAN),
				                   chptr->name.c_str());
		}

		return;
	}

	/* hook may have reduced the string to nothing. */
	if (EmptyString(text))
	{
		/* could be empty after colour stripping and
		 * that would cause problems later */
		if(msgtype != MESSAGE_TYPE_NOTICE)
			sendto_one(&source, form_str(ERR_NOTEXTTOSEND), me.name, source.name);
		return;
	}

	/* chanops and voiced can flood their own channel with impunity */
	if((result = can_send(chptr, &source, NULL)))
	{
		if(result != chan::CAN_SEND_OPV && my(source) &&
		   !is(source, umode::OPER) &&
		   !tgchange::add_target(source, *chptr))
		{
			sendto_one(&source, form_str(ERR_TARGCHANGE),
			           me.name,
			           source.name,
			           chptr->name.c_str());
			return;
		}
		if(result == chan::CAN_SEND_OPV ||
		   !flood_attack_channel(msgtype, &source, chptr))
		{
			sendto_channel_flags(&client, chan::ALL_MEMBERS, &source, chptr,
			                     "%s %s :%s",
			                     cmdname[msgtype],
			                     chptr->name.c_str(),
			                     text);
		}
	}
	else if(chptr->mode.mode & chan::mode::OPMODERATE &&
	        (!(chptr->mode.mode & chan::mode::NOPRIVMSGS) || is_member(chptr, &source)))
	{
		if(my(source) && !is(source, umode::OPER) && !tgchange::add_target(source, *chptr))
		{
			sendto_one(&source, form_str(ERR_TARGCHANGE),
			           me.name,
			           source.name,
			           chptr->name.c_str());
			return;
		}

		if(!flood_attack_channel(msgtype, &source, chptr))
			sendto_channel_opmod(&client, &source, chptr, cmdname[msgtype], text);
	}
	else
	{
		if(msgtype != MESSAGE_TYPE_NOTICE)
			sendto_one_numeric(&source, ERR_CANNOTSENDTOCHAN,
					   form_str(ERR_CANNOTSENDTOCHAN), chptr->name.c_str());
	}
}
/*
 * msg_channel_opmod
 *
 * inputs	- flag privmsg or notice
 *		- pointer to &client
 *		- pointer to &source
 *		- pointer to channel
 * output	- NONE
 * side effects	- message given channel ops
 *
 * XXX - We need to rework this a bit, it's a tad ugly. --nenolod
 */
static void
msg_channel_opmod(enum message_type msgtype,
		  client::client &client, client::client &source,
		  chan::chan *chptr, const char *text)
{
	hook_data_privmsg_channel hdata;

	hdata.msgtype = msgtype;
	hdata.source_p = &source;
	hdata.chptr = chptr;
	hdata.text = text;
	hdata.approved = 0;
	hdata.reason = NULL;

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
		if (msgtype != MESSAGE_TYPE_NOTICE)
			sendto_one(&source, form_str(ERR_NOTEXTTOSEND),
			           me.name,
			           source.name);
		return;
	}

	if (chptr->mode.mode & chan::mode::OPMODERATE &&
	   (!(chptr->mode.mode & chan::mode::NOPRIVMSGS) || is_member(chptr, &source)))
	{
		if (!flood_attack_channel(msgtype, &source, chptr))
			sendto_channel_opmod(&client, &source, chptr, cmdname[msgtype], text);
	}
	else
	{
		if (msgtype != MESSAGE_TYPE_NOTICE)
			sendto_one_numeric(&source, ERR_CANNOTSENDTOCHAN, form_str(ERR_CANNOTSENDTOCHAN),
			                   chptr->name.c_str());
	}
}

/*
 * msg_channel_flags
 *
 * inputs	- flag 0 if PRIVMSG 1 if NOTICE. RFC
 *		  say NOTICE must not auto reply
 *		- pointer to &client
 *		- pointer to &source
 *		- pointer to channel
 *		- flags
 *		- pointer to text to send
 * output	- NONE
 * side effects	- message given channel either chanop or voice
 */
static void
msg_channel_flags(enum message_type msgtype, client::client &client,
		  client::client &source, chan::chan *chptr, int flags, const char *text)
{
	int type;
	char c;
	hook_data_privmsg_channel hdata;

	if(flags & chan::VOICE)
	{
		type = chan::ONLY_CHANOPSVOICED;
		c = '+';
	}
	else
	{
		type = chan::ONLY_CHANOPS;
		c = '@';
	}

	if(my(source))
	{
		/* idletime shouldnt be reset by notice --fl */
		if(msgtype != MESSAGE_TYPE_NOTICE)
			source.localClient->last = rb_current_time();
	}

	hdata.msgtype = msgtype;
	hdata.source_p = &source;
	hdata.chptr = chptr;
	hdata.text = text;
	hdata.approved = 0;
	hdata.reason = NULL;

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
			sendto_one(&source, form_str(ERR_NOTEXTTOSEND), me.name, source.name);
		return;
	}

	sendto_channel_flags(&client, type, &source, chptr, "%s %c%s :%s",
			     cmdname[msgtype], c, chptr->name.c_str(), text);
}

static void
expire_tgchange(void *unused)
{
	tgchange::tgchange *target;
	rb_dlink_node *ptr, *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, tgchange_list.head)
	{
		target = (tgchange::tgchange *)ptr->data;

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
 * 		- pointer to &source source (client::client *)
 *		- pointer to target_p target (client::client *)
 *		- pointer to text
 * output	- NONE
 * side effects	- message given channel either chanop or voice
 */
static void
msg_client(enum message_type msgtype,
	   client::client &source, client::client *target_p, const char *text)
{
	int do_floodcount = 0;
	hook_data_privmsg_user hdata;

	if(my(source))
	{
		/*
		 * XXX: Controversial? Allow target users to send replies
		 * through a +g.  Rationale is that people can presently use +g
		 * as a way to taunt users, e.g. harass them and hide behind +g
		 * as a way of griefing.  --nenolod
		 */
		if(msgtype != MESSAGE_TYPE_NOTICE &&
				(is(source, umode::CALLERID) ||
				 (is(source, umode::REGONLYMSG) && suser(user(*target_p)).empty())) &&
				!accept_message(target_p, &source) &&
				!is(*target_p, umode::OPER))
		{
			if(rb_dlink_list_length(&source.localClient->allow_list) <
					(unsigned long)ConfigFileEntry.max_accept)
			{
				rb_dlinkAddAlloc(target_p, &source.localClient->allow_list);
				rb_dlinkAddAlloc(&source, &target_p->on_allow_list);
			}
			else
			{
				sendto_one_numeric(&source, ERR_OWNMODE,
						form_str(ERR_OWNMODE),
						target_p->name, "+g");
				return;
			}
		}

		/* reset idle time for message only if its not to self
		 * and its not a notice */
		if(msgtype != MESSAGE_TYPE_NOTICE)
			source.localClient->last = rb_current_time();

		/* auto cprivmsg/cnotice */
		do_floodcount = !is(source, umode::OPER) &&
			!tgchange::find_allowing_channel(source, *target_p);

		/* target change stuff, dont limit ctcp replies as that
		 * would allow people to start filling up random users
		 * targets just by ctcping them
		 */
		if((msgtype != MESSAGE_TYPE_NOTICE || *text != '\001') &&
		   ConfigFileEntry.target_change && do_floodcount)
		{
			if(!tgchange::add_target(source, *target_p))
			{
				sendto_one(&source, form_str(ERR_TARGCHANGE),
					   me.name, source.name, target_p->name);
				return;
			}
		}

		if (do_floodcount && msgtype == MESSAGE_TYPE_NOTICE && *text == '\001' &&
				target_p->large_ctcp_sent + LARGE_CTCP_TIME >= rb_current_time())
			do_floodcount = 0;

		if (do_floodcount &&
				flood_attack_client(msgtype, source, target_p))
			return;
	}
	else if(source.from == target_p->from)
	{
		sendto_realops_snomask(sno::DEBUG, L_ALL,
				     "Send message to %s[%s] dropped from %s(Fake Dir)",
				     target_p->name, target_p->from->name, source.name);
		return;
	}

	if(my_connect(source) && (msgtype != MESSAGE_TYPE_NOTICE) && target_p->user && away(user(*target_p)).size())
		sendto_one_numeric(&source, RPL_AWAY, form_str(RPL_AWAY),
				   target_p->name, away(user(*target_p)).c_str());

	if(my(*target_p))
	{
		hdata.msgtype = msgtype;
		hdata.source_p = &source;
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
				sendto_one(&source, form_str(ERR_NOTEXTTOSEND), me.name, source.name);
			return;
		}

		/* XXX Controversial? allow opers always to send through a +g */
		if(!is_server(source) && (is(*target_p, umode::CALLERID) ||
					(is(*target_p, umode::REGONLYMSG) && suser(user(source)).empty())))
		{
			/* Here is the anti-flood bot/spambot code -db */
			if(accept_message(&source, target_p) || is(source, umode::OPER))
			{
				tgchange::add_reply_target(*target_p, source);
				sendto_one(target_p, ":%s!%s@%s %s %s :%s",
					   source.name,
					   source.username,
					   source.host, cmdname[msgtype], target_p->name, text);
			}
			else if (is(*target_p, umode::REGONLYMSG) && suser(user(source)).empty())
			{
				if (msgtype != MESSAGE_TYPE_NOTICE)
					sendto_one_numeric(&source, ERR_NONONREG,
							form_str(ERR_NONONREG),
							target_p->name);
			}
			else
			{
				/* check for accept, flag recipient incoming message */
				if(msgtype != MESSAGE_TYPE_NOTICE)
				{
					sendto_one_numeric(&source, ERR_TARGUMODEG,
							   form_str(ERR_TARGUMODEG),
							   target_p->name);
				}

				if((target_p->localClient->last_caller_id_time +
				    ConfigFileEntry.caller_id_wait) < rb_current_time())
				{
					if(msgtype != MESSAGE_TYPE_NOTICE)
						sendto_one_numeric(&source, RPL_TARGNOTIFY,
								   form_str(RPL_TARGNOTIFY),
								   target_p->name);

					tgchange::add_reply_target(*target_p, source);
					sendto_one(target_p, form_str(RPL_UMODEGMSG),
						   me.name, target_p->name, source.name,
						   source.username, source.host);

					target_p->localClient->last_caller_id_time = rb_current_time();
				}
			}
		}
		else
		{
			tgchange::add_reply_target(*target_p, source);
			sendto_anywhere(target_p, &source, cmdname[msgtype], ":%s", text);
		}
	}
	else
		sendto_anywhere(target_p, &source, cmdname[msgtype], ":%s", text);

	return;
}

/*
 * flood_attack_client
 * inputs       - flag 0 if PRIVMSG 1 if NOTICE. RFC
 *                says NOTICE must not auto reply
 *              - pointer to source Client
 *		- pointer to target Client
 * output	- true if target is under flood attack
 * side effects	- check for flood attack on target target_p
 */
static bool
flood_attack_client(enum message_type msgtype, client::client &source, client::client *target_p)
{
	int delta;

	/* Services could get many messages legitimately and
	 * can be messaged without rate limiting via aliases
	 * and msg user@server.
	 * -- jilles
	 */
	if(GlobalSetOptions.floodcount && is_client(source) && &source != target_p && !is(*target_p, umode::SERVICE))
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
				sendto_realops_snomask(sno::BOTS, L_NETWIDE,
						     "Possible Flooder %s[%s@%s] on %s target: %s",
						     source.name, source.username,
						     source.orighost,
						     source.servptr->name, target_p->name);
				target_p->flood_noticed = 1;
				/* add a bit of penalty */
				target_p->received_number_of_privmsgs += 2;
			}
			if(my(source) && (msgtype != MESSAGE_TYPE_NOTICE))
				sendto_one(&source,
					   ":%s NOTICE %s :*** Message to %s throttled due to flooding",
					   me.name, source.name, target_p->name);
			return true;
		}
		else
			target_p->received_number_of_privmsgs++;
	}

	return false;
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
handle_special(enum message_type msgtype, client::client &client,
	       client::client &source, const char *nick, const char *text)
{
	client::client *target_p;
	char *server;
	char *s;

	/* user[%host]@server addressed?
	 * NOTE: users can send to user@server, but not user%host@server
	 * or opers@server
	 */
	if((server = (char *)strchr(nick, '@')) != NULL)
	{
		if((target_p = find_server(&source, server + 1)) == NULL)
		{
			sendto_one_numeric(&source, ERR_NOSUCHSERVER,
					   form_str(ERR_NOSUCHSERVER), server + 1);
			return;
		}

		if(!is(source, umode::OPER))
		{
			if(strchr(nick, '%') || (strncmp(nick, "opers", 5) == 0))
			{
				sendto_one_numeric(&source, ERR_NOSUCHNICK,
						   form_str(ERR_NOSUCHNICK), nick);
				return;
			}
		}

		/* somewhere else.. */
		if(!is_me(*target_p))
		{
			sendto_one(target_p, ":%s %s %s :%s",
				   get_id(&source, target_p), cmdname[msgtype], nick, text);
			return;
		}

		/* Check if someones msg'ing opers@our.server */
		if(strncmp(nick, "opers@", 6) == 0)
		{
			sendto_realops_snomask(sno::GENERAL, L_ALL, "To opers: From: %s: %s",
					     source.name, text);
			return;
		}

		/* This was not very useful except for bypassing certain
		 * restrictions. Note that we still allow sending to
		 * remote servers this way, for messaging pseudoservers
		 * securely whether they have a service{} block or not.
		 * -- jilles
		 */
		sendto_one_numeric(&source, ERR_NOSUCHNICK,
				   form_str(ERR_NOSUCHNICK), nick);
		return;
	}

	/*
	 * the following two cases allow masks in NOTICEs
	 * (for OPERs only)
	 *
	 * Armin, 8Jun90 (gruner@informatik.tu-muenchen.de)
	 */
	if(is(source, umode::OPER) && *nick == '$')
	{
		if((*(nick + 1) == '$' || *(nick + 1) == '#'))
			nick++;
		else if(my_oper(source))
		{
			sendto_one(&source,
				   ":%s NOTICE %s :The command %s %s is no longer supported, please use $%s",
				   me.name, source.name, cmdname[msgtype], nick, nick);
			return;
		}

		if(my(source) && !IsOperMassNotice(&source))
		{
			sendto_one(&source, form_str(ERR_NOPRIVS),
				   me.name, source.name, "mass_notice");
			return;
		}

		if((s = (char *)strrchr(nick, '.')) == NULL)
		{
			sendto_one_numeric(&source, ERR_NOTOPLEVEL,
					   form_str(ERR_NOTOPLEVEL), nick);
			return;
		}
		while(*++s)
			if(*s == '.' || *s == '*' || *s == '?')
				break;
		if(*s == '*' || *s == '?')
		{
			sendto_one_numeric(&source, ERR_WILDTOPLEVEL,
					   form_str(ERR_WILDTOPLEVEL), nick);
			return;
		}

		sendto_match_butone(is_server(client) ? &client : NULL, &source,
				    nick + 1,
				    (*nick == '#') ? MATCH_HOST : MATCH_SERVER,
				    "%s $%s :%s", cmdname[msgtype], nick, text);
		if (msgtype != MESSAGE_TYPE_NOTICE && *text == '\001')
			source.large_ctcp_sent = rb_current_time();
		return;
	}
}
