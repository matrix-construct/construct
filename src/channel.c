/*
 *  ircd-ratbox: A slightly useful ircd.
 *  channel.c: Controls channels.
 *
 * Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center 
 * Copyright (C) 1996-2002 Hybrid Development Team 
 * Copyright (C) 2002-2005 ircd-ratbox development team 
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
 *  $Id: channel.c 3580 2007-11-07 23:45:14Z jilles $
 */

#include "stdinc.h"
#include "channel.h"
#include "chmode.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "hook.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"		/* captab */
#include "s_user.h"
#include "send.h"
#include "whowas.h"
#include "s_conf.h"		/* ConfigFileEntry, ConfigChannel */
#include "s_newconf.h"
#include "logger.h"

struct config_channel_entry ConfigChannel;
rb_dlink_list global_channel_list;
static rb_bh *channel_heap;
static rb_bh *ban_heap;
static rb_bh *topic_heap;
static rb_bh *member_heap;

static int channel_capabs[] = { CAP_EX, CAP_IE,
	CAP_SERVICE,
	CAP_TS6
};

#define NCHCAPS         (sizeof(channel_capabs)/sizeof(int))
#define NCHCAP_COMBOS   (1 << NCHCAPS)

static struct ChCapCombo chcap_combos[NCHCAP_COMBOS];

static void free_topic(struct Channel *chptr);

static int h_can_join;

/* init_channels()
 *
 * input	-
 * output	-
 * side effects - initialises the various blockheaps
 */
void
init_channels(void)
{
	channel_heap = rb_bh_create(sizeof(struct Channel), CHANNEL_HEAP_SIZE, "channel_heap");
	ban_heap = rb_bh_create(sizeof(struct Ban), BAN_HEAP_SIZE, "ban_heap");
	topic_heap = rb_bh_create(TOPICLEN + 1 + USERHOST_REPLYLEN, TOPIC_HEAP_SIZE, "topic_heap");
	member_heap = rb_bh_create(sizeof(struct membership), MEMBER_HEAP_SIZE, "member_heap");

	h_can_join = register_hook("can_join");
}

/*
 * allocate_channel - Allocates a channel
 */
struct Channel *
allocate_channel(const char *chname)
{
	struct Channel *chptr;
	chptr = rb_bh_alloc(channel_heap);
	chptr->chname = rb_strdup(chname);
	return (chptr);
}

void
free_channel(struct Channel *chptr)
{
	rb_free(chptr->chname);
	rb_free(chptr->mode_lock);
	rb_bh_free(channel_heap, chptr);
}

struct Ban *
allocate_ban(const char *banstr, const char *who)
{
	struct Ban *bptr;
	bptr = rb_bh_alloc(ban_heap);
	bptr->banstr = rb_strdup(banstr);
	bptr->who = rb_strdup(who);

	return (bptr);
}

void
free_ban(struct Ban *bptr)
{
	rb_free(bptr->banstr);
	rb_free(bptr->who);
	rb_bh_free(ban_heap, bptr);
}


/* find_channel_membership()
 *
 * input	- channel to find them in, client to find
 * output	- membership of client in channel, else NULL
 * side effects	-
 */
struct membership *
find_channel_membership(struct Channel *chptr, struct Client *client_p)
{
	struct membership *msptr;
	rb_dlink_node *ptr;

	if(!IsClient(client_p))
		return NULL;

	/* Pick the most efficient list to use to be nice to things like
	 * CHANSERV which could be in a large number of channels
	 */
	if(rb_dlink_list_length(&chptr->members) < rb_dlink_list_length(&client_p->user->channel))
	{
		RB_DLINK_FOREACH(ptr, chptr->members.head)
		{
			msptr = ptr->data;

			if(msptr->client_p == client_p)
				return msptr;
		}
	}
	else
	{
		RB_DLINK_FOREACH(ptr, client_p->user->channel.head)
		{
			msptr = ptr->data;

			if(msptr->chptr == chptr)
				return msptr;
		}
	}

	return NULL;
}

/* find_channel_status()
 *
 * input	- membership to get status for, whether we can combine flags
 * output	- flags of user on channel
 * side effects -
 */
const char *
find_channel_status(struct membership *msptr, int combine)
{
	static char buffer[3];
	char *p;

	p = buffer;

	if(is_chanop(msptr))
	{
		if(!combine)
			return "@";
		*p++ = '@';
	}

	if(is_voiced(msptr))
		*p++ = '+';

	*p = '\0';
	return buffer;
}

/* add_user_to_channel()
 *
 * input	- channel to add client to, client to add, channel flags
 * output	- 
 * side effects - user is added to channel
 */
void
add_user_to_channel(struct Channel *chptr, struct Client *client_p, int flags)
{
	struct membership *msptr;

	s_assert(client_p->user != NULL);
	if(client_p->user == NULL)
		return;

	msptr = rb_bh_alloc(member_heap);

	msptr->chptr = chptr;
	msptr->client_p = client_p;
	msptr->flags = flags;

	rb_dlinkAdd(msptr, &msptr->usernode, &client_p->user->channel);
	rb_dlinkAdd(msptr, &msptr->channode, &chptr->members);

	if(MyClient(client_p))
		rb_dlinkAdd(msptr, &msptr->locchannode, &chptr->locmembers);
}

/* remove_user_from_channel()
 *
 * input	- membership pointer to remove from channel
 * output	-
 * side effects - membership (thus user) is removed from channel
 */
void
remove_user_from_channel(struct membership *msptr)
{
	struct Client *client_p;
	struct Channel *chptr;
	s_assert(msptr != NULL);
	if(msptr == NULL)
		return;

	client_p = msptr->client_p;
	chptr = msptr->chptr;

	rb_dlinkDelete(&msptr->usernode, &client_p->user->channel);
	rb_dlinkDelete(&msptr->channode, &chptr->members);

	if(client_p->servptr == &me)
		rb_dlinkDelete(&msptr->locchannode, &chptr->locmembers);

	if(!(chptr->mode.mode & MODE_PERMANENT) && rb_dlink_list_length(&chptr->members) <= 0)
		destroy_channel(chptr);

	rb_bh_free(member_heap, msptr);

	return;
}

/* remove_user_from_channels()
 *
 * input        - user to remove from all channels
 * output       -
 * side effects - user is removed from all channels
 */
void
remove_user_from_channels(struct Client *client_p)
{
	struct Channel *chptr;
	struct membership *msptr;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;

	if(client_p == NULL)
		return;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, client_p->user->channel.head)
	{
		msptr = ptr->data;
		chptr = msptr->chptr;

		rb_dlinkDelete(&msptr->channode, &chptr->members);

		if(client_p->servptr == &me)
			rb_dlinkDelete(&msptr->locchannode, &chptr->locmembers);

		if(!(chptr->mode.mode & MODE_PERMANENT) && rb_dlink_list_length(&chptr->members) <= 0)
			destroy_channel(chptr);

		rb_bh_free(member_heap, msptr);
	}

	client_p->user->channel.head = client_p->user->channel.tail = NULL;
	client_p->user->channel.length = 0;
}

/* invalidate_bancache_user()
 *
 * input	- user to invalidate ban cache for
 * output	-
 * side effects - ban cache is invalidated for all memberships of that user
 *                to be used after a nick change
 */
void
invalidate_bancache_user(struct Client *client_p)
{
	struct membership *msptr;
	rb_dlink_node *ptr;

	if(client_p == NULL)
		return;

	RB_DLINK_FOREACH(ptr, client_p->user->channel.head)
	{
		msptr = ptr->data;
		msptr->bants = 0;
		msptr->flags &= ~CHFL_BANNED;
	}
}

/* check_channel_name()
 *
 * input	- channel name
 * output	- 1 if valid channel name, else 0
 * side effects -
 */
int
check_channel_name(const char *name)
{
	s_assert(name != NULL);
	if(name == NULL)
		return 0;

	for (; *name; ++name)
	{
		if(!IsChanChar(*name))
			return 0;
	}

	return 1;
}

/* free_channel_list()
 *
 * input	- rb_dlink list to free
 * output	-
 * side effects - list of b/e/I modes is cleared
 */
void
free_channel_list(rb_dlink_list * list)
{
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	struct Ban *actualBan;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, list->head)
	{
		actualBan = ptr->data;
		free_ban(actualBan);
	}

	list->head = list->tail = NULL;
	list->length = 0;
}

/* destroy_channel()
 *
 * input	- channel to destroy
 * output	-
 * side effects - channel is obliterated
 */
void
destroy_channel(struct Channel *chptr)
{
	rb_dlink_node *ptr, *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, chptr->invites.head)
	{
		del_invite(chptr, ptr->data);
	}

	/* free all bans/exceptions/denies */
	free_channel_list(&chptr->banlist);
	free_channel_list(&chptr->exceptlist);
	free_channel_list(&chptr->invexlist);
	free_channel_list(&chptr->quietlist);

	/* Free the topic */
	free_topic(chptr);

	rb_dlinkDelete(&chptr->node, &global_channel_list);
	del_from_channel_hash(chptr->chname, chptr);
	free_channel(chptr);
}

/* channel_pub_or_secret()
 *
 * input	- channel
 * output	- "=" if public, "@" if secret, else "*"
 * side effects	-
 */
static const char *
channel_pub_or_secret(struct Channel *chptr)
{
	if(PubChannel(chptr))
		return ("=");
	else if(SecretChannel(chptr))
		return ("@");
	return ("*");
}

/* channel_member_names()
 *
 * input	- channel to list, client to list to, show endofnames
 * output	-
 * side effects - client is given list of users on channel
 */
void
channel_member_names(struct Channel *chptr, struct Client *client_p, int show_eon)
{
	struct membership *msptr;
	struct Client *target_p;
	rb_dlink_node *ptr;
	char lbuf[BUFSIZE];
	char *t;
	int mlen;
	int tlen;
	int cur_len;
	int is_member;
	int stack = IsCapable(client_p, CLICAP_MULTI_PREFIX);

	if(ShowChannel(client_p, chptr))
	{
		is_member = IsMember(client_p, chptr);

		cur_len = mlen = rb_sprintf(lbuf, form_str(RPL_NAMREPLY),
					    me.name, client_p->name,
					    channel_pub_or_secret(chptr), chptr->chname);

		t = lbuf + cur_len;

		RB_DLINK_FOREACH(ptr, chptr->members.head)
		{
			msptr = ptr->data;
			target_p = msptr->client_p;

			if(IsInvisible(target_p) && !is_member)
				continue;

			/* space, possible "@+" prefix */
			if(cur_len + strlen(target_p->name) + 3 >= BUFSIZE - 3)
			{
				*(t - 1) = '\0';
				sendto_one(client_p, "%s", lbuf);
				cur_len = mlen;
				t = lbuf + mlen;
			}

			tlen = rb_sprintf(t, "%s%s ", find_channel_status(msptr, stack),
					  target_p->name);

			cur_len += tlen;
			t += tlen;
		}

		/* The old behaviour here was to always output our buffer,
		 * even if there are no clients we can show.  This happens
		 * when a client does "NAMES" with no parameters, and all
		 * the clients on a -sp channel are +i.  I dont see a good
		 * reason for keeping that behaviour, as it just wastes
		 * bandwidth.  --anfl
		 */
		if(cur_len != mlen)
		{
			*(t - 1) = '\0';
			sendto_one(client_p, "%s", lbuf);
		}
	}

	if(show_eon)
		sendto_one(client_p, form_str(RPL_ENDOFNAMES),
			   me.name, client_p->name, chptr->chname);
}

/* del_invite()
 *
 * input	- channel to remove invite from, client to remove
 * output	-
 * side effects - user is removed from invite list, if exists
 */
void
del_invite(struct Channel *chptr, struct Client *who)
{
	rb_dlinkFindDestroy(who, &chptr->invites);
	rb_dlinkFindDestroy(chptr, &who->user->invited);
}

/* is_banned()
 *
 * input	- channel to check bans for, user to check bans against
 *                optional prebuilt buffers
 * output	- 1 if banned, else 0
 * side effects -
 */
int
is_banned(struct Channel *chptr, struct Client *who, struct membership *msptr,
	  const char *s, const char *s2)
{
	char src_host[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_iphost[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_althost[NICKLEN + USERLEN + HOSTLEN + 6];
	char *s3 = NULL;
	rb_dlink_node *ptr;
	struct Ban *actualBan = NULL;
	struct Ban *actualExcept = NULL;

	if(!MyClient(who))
		return 0;

	/* if the buffers havent been built, do it here */
	if(s == NULL)
	{
		rb_sprintf(src_host, "%s!%s@%s", who->name, who->username, who->host);
		rb_sprintf(src_iphost, "%s!%s@%s", who->name, who->username, who->sockhost);

		s = src_host;
		s2 = src_iphost;
	}
	if(who->localClient->mangledhost != NULL)
	{
		/* if host mangling mode enabled, also check their real host */
		if(!strcmp(who->host, who->localClient->mangledhost))
		{
			rb_sprintf(src_althost, "%s!%s@%s", who->name, who->username, who->orighost);
			s3 = src_althost;
		}
		/* if host mangling mode not enabled and no other spoof,
		 * also check the mangled form of their host */
		else if (!IsDynSpoof(who))
		{
			rb_sprintf(src_althost, "%s!%s@%s", who->name, who->username, who->localClient->mangledhost);
			s3 = src_althost;
		}
	}

	RB_DLINK_FOREACH(ptr, chptr->banlist.head)
	{
		actualBan = ptr->data;
		if(match(actualBan->banstr, s) ||
		   match(actualBan->banstr, s2) ||
		   match_cidr(actualBan->banstr, s2) ||
		   match_extban(actualBan->banstr, who, chptr, CHFL_BAN) ||
		   (s3 != NULL && match(actualBan->banstr, s3)))
			break;
		else
			actualBan = NULL;
	}

	if((actualBan != NULL) && ConfigChannel.use_except)
	{
		RB_DLINK_FOREACH(ptr, chptr->exceptlist.head)
		{
			actualExcept = ptr->data;

			/* theyre exempted.. */
			if(match(actualExcept->banstr, s) ||
			   match(actualExcept->banstr, s2) ||
			   match_cidr(actualExcept->banstr, s2) ||
			   match_extban(actualExcept->banstr, who, chptr, CHFL_EXCEPTION) ||
			   (s3 != NULL && match(actualExcept->banstr, s3)))
			{
				/* cache the fact theyre not banned */
				if(msptr != NULL)
				{
					msptr->bants = chptr->bants;
					msptr->flags &= ~CHFL_BANNED;
				}

				return CHFL_EXCEPTION;
			}
		}
	}

	/* cache the banned/not banned status */
	if(msptr != NULL)
	{
		msptr->bants = chptr->bants;

		if(actualBan != NULL)
		{
			msptr->flags |= CHFL_BANNED;
			return CHFL_BAN;
		}
		else
		{
			msptr->flags &= ~CHFL_BANNED;
			return 0;
		}
	}

	return ((actualBan ? CHFL_BAN : 0));
}

/* is_quieted()
 *
 * input	- channel to check bans for, user to check bans against
 *                optional prebuilt buffers
 * output	- 1 if banned, else 0
 * side effects -
 */
int
is_quieted(struct Channel *chptr, struct Client *who, struct membership *msptr,
	   const char *s, const char *s2)
{
	char src_host[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_iphost[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_althost[NICKLEN + USERLEN + HOSTLEN + 6];
	char *s3 = NULL;
	rb_dlink_node *ptr;
	struct Ban *actualBan = NULL;
	struct Ban *actualExcept = NULL;

	if(!MyClient(who))
		return 0;

	/* if the buffers havent been built, do it here */
	if(s == NULL)
	{
		rb_sprintf(src_host, "%s!%s@%s", who->name, who->username, who->host);
		rb_sprintf(src_iphost, "%s!%s@%s", who->name, who->username, who->sockhost);

		s = src_host;
		s2 = src_iphost;
	}
	if(who->localClient->mangledhost != NULL)
	{
		/* if host mangling mode enabled, also check their real host */
		if(!strcmp(who->host, who->localClient->mangledhost))
		{
			rb_sprintf(src_althost, "%s!%s@%s", who->name, who->username, who->orighost);
			s3 = src_althost;
		}
		/* if host mangling mode not enabled and no other spoof,
		 * also check the mangled form of their host */
		else if (!IsDynSpoof(who))
		{
			rb_sprintf(src_althost, "%s!%s@%s", who->name, who->username, who->localClient->mangledhost);
			s3 = src_althost;
		}
	}

	RB_DLINK_FOREACH(ptr, chptr->quietlist.head)
	{
		actualBan = ptr->data;
		if(match(actualBan->banstr, s) ||
		   match(actualBan->banstr, s2) ||
		   match_cidr(actualBan->banstr, s2) ||
		   match_extban(actualBan->banstr, who, chptr, CHFL_QUIET) ||
		   (s3 != NULL && match(actualBan->banstr, s3)))
			break;
		else
			actualBan = NULL;
	}

	if((actualBan != NULL) && ConfigChannel.use_except)
	{
		RB_DLINK_FOREACH(ptr, chptr->exceptlist.head)
		{
			actualExcept = ptr->data;

			/* theyre exempted.. */
			if(match(actualExcept->banstr, s) ||
			   match(actualExcept->banstr, s2) ||
			   match_cidr(actualExcept->banstr, s2) ||
			   match_extban(actualExcept->banstr, who, chptr, CHFL_EXCEPTION) ||
			   (s3 != NULL && match(actualExcept->banstr, s3)))
			{
				/* cache the fact theyre not banned */
				if(msptr != NULL)
				{
					msptr->bants = chptr->bants;
					msptr->flags &= ~CHFL_BANNED;
				}

				return CHFL_EXCEPTION;
			}
		}
	}

	/* cache the banned/not banned status */
	if(msptr != NULL)
	{
		msptr->bants = chptr->bants;

		if(actualBan != NULL)
		{
			msptr->flags |= CHFL_BANNED;
			return CHFL_BAN;
		}
		else
		{
			msptr->flags &= ~CHFL_BANNED;
			return 0;
		}
	}

	return ((actualBan ? CHFL_BAN : 0));
}

/* can_join()
 *
 * input	- client to check, channel to check for, key
 * output	- reason for not being able to join, else 0
 * side effects -
 * caveats      - this function should only be called on a local user.
 */
int
can_join(struct Client *source_p, struct Channel *chptr, char *key)
{
	rb_dlink_node *invite = NULL;
	rb_dlink_node *ptr;
	struct Ban *invex = NULL;
	char src_host[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_iphost[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_althost[NICKLEN + USERLEN + HOSTLEN + 6];
	int use_althost = 0;
	int i = 0;
	hook_data_channel moduledata;

	s_assert(source_p->localClient != NULL);

	rb_sprintf(src_host, "%s!%s@%s", source_p->name, source_p->username, source_p->host);
	rb_sprintf(src_iphost, "%s!%s@%s", source_p->name, source_p->username, source_p->sockhost);
	if(source_p->localClient->mangledhost != NULL)
	{
		/* if host mangling mode enabled, also check their real host */
		if(!strcmp(source_p->host, source_p->localClient->mangledhost))
		{
			rb_sprintf(src_althost, "%s!%s@%s", source_p->name, source_p->username, source_p->orighost);
			use_althost = 1;
		}
		/* if host mangling mode not enabled and no other spoof,
		 * also check the mangled form of their host */
		else if (!IsDynSpoof(source_p))
		{
			rb_sprintf(src_althost, "%s!%s@%s", source_p->name, source_p->username, source_p->localClient->mangledhost);
			use_althost = 1;
		}
	}

	if((is_banned(chptr, source_p, NULL, src_host, src_iphost)) == CHFL_BAN)
		return (ERR_BANNEDFROMCHAN);

	if(chptr->mode.mode & MODE_INVITEONLY)
	{
		RB_DLINK_FOREACH(invite, source_p->user->invited.head)
		{
			if(invite->data == chptr)
				break;
		}
		if(invite == NULL)
		{
			if(!ConfigChannel.use_invex)
				return (ERR_INVITEONLYCHAN);
			RB_DLINK_FOREACH(ptr, chptr->invexlist.head)
			{
				invex = ptr->data;
				if(match(invex->banstr, src_host)
				   || match(invex->banstr, src_iphost)
				   || match_cidr(invex->banstr, src_iphost)
			   	   || match_extban(invex->banstr, source_p, chptr, CHFL_INVEX)
				   || (use_althost && match(invex->banstr, src_althost)))
					break;
			}
			if(ptr == NULL)
				return (ERR_INVITEONLYCHAN);
		}
	}

	if(*chptr->mode.key && (EmptyString(key) || irccmp(chptr->mode.key, key)))
		return (ERR_BADCHANNELKEY);

	if(chptr->mode.limit &&
	   rb_dlink_list_length(&chptr->members) >= (unsigned long) chptr->mode.limit)
		i = ERR_CHANNELISFULL;
	if(chptr->mode.mode & MODE_REGONLY && EmptyString(source_p->user->suser))
		i = ERR_NEEDREGGEDNICK;
	/* join throttling stuff --nenolod */
	else if(chptr->mode.join_num > 0 && chptr->mode.join_time > 0)
	{
		if ((rb_current_time() - chptr->join_delta <= 
			chptr->mode.join_time) && (chptr->join_count >=
			chptr->mode.join_num))
			i = ERR_THROTTLE;
	}

	/* allow /invite to override +l/+r/+j also -- jilles */
	if (i != 0 && invite == NULL)
	{
		RB_DLINK_FOREACH(invite, source_p->user->invited.head)
		{
			if(invite->data == chptr)
				break;
		}
		if (invite == NULL)
			return i;
	}

	moduledata.client = source_p;
	moduledata.chptr = chptr;
	moduledata.approved = 0;

	call_hook(h_can_join, &moduledata);

	return moduledata.approved;
}

/* can_send()
 *
 * input	- user to check in channel, membership pointer
 * output	- whether can explicitly send or not, else CAN_SEND_NONOP
 * side effects -
 */
int
can_send(struct Channel *chptr, struct Client *source_p, struct membership *msptr)
{
	if(IsServer(source_p) || IsService(source_p))
		return CAN_SEND_OPV;

	if(MyClient(source_p) && hash_find_resv(chptr->chname) &&
	   !IsOper(source_p) && !IsExemptResv(source_p))
		return CAN_SEND_NO;

	if(msptr == NULL)
	{
		msptr = find_channel_membership(chptr, source_p);

		if(msptr == NULL)
		{
			/* if its +m or +n and theyre not in the channel,
			 * they cant send.  we dont check bans here because
			 * theres no possibility of caching them --fl
			 */
			if(chptr->mode.mode & MODE_NOPRIVMSGS || chptr->mode.mode & MODE_MODERATED)
				return CAN_SEND_NO;
			else
				return CAN_SEND_NONOP;
		}
	}

	if(is_chanop_voiced(msptr))
		return CAN_SEND_OPV;

	if(chptr->mode.mode & MODE_MODERATED)
		return CAN_SEND_NO;

	if(MyClient(source_p))
	{
		/* cached can_send */
		if(msptr->bants == chptr->bants)
		{
			if(can_send_banned(msptr))
				return CAN_SEND_NO;
		}
		else if(is_banned(chptr, source_p, msptr, NULL, NULL) == CHFL_BAN
			|| is_quieted(chptr, source_p, msptr, NULL, NULL) == CHFL_BAN)
			return CAN_SEND_NO;
	}

	return CAN_SEND_NONOP;
}

/* find_bannickchange_channel()
 * Input: client to check
 * Output: channel preventing nick change
 */
struct Channel *
find_bannickchange_channel(struct Client *client_p)
{
	struct Channel *chptr;
	struct membership *msptr;
	rb_dlink_node *ptr;
	char src_host[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_iphost[NICKLEN + USERLEN + HOSTLEN + 6];

	if (!MyClient(client_p))
		return NULL;

	rb_sprintf(src_host, "%s!%s@%s", client_p->name, client_p->username, client_p->host);
	rb_sprintf(src_iphost, "%s!%s@%s", client_p->name, client_p->username, client_p->sockhost);

	RB_DLINK_FOREACH(ptr, client_p->user->channel.head)
	{
		msptr = ptr->data;
		chptr = msptr->chptr;
		if (is_chanop_voiced(msptr))
			continue;
		/* cached can_send */
		if (msptr->bants == chptr->bants)
		{
			if (can_send_banned(msptr))
				return chptr;
		}
		else if (is_banned(chptr, client_p, msptr, src_host, src_iphost) == CHFL_BAN
			|| is_quieted(chptr, client_p, msptr, src_host, src_iphost) == CHFL_BAN)
			return chptr;
	}
	return NULL;
}

/* void check_spambot_warning(struct Client *source_p)
 * Input: Client to check, channel name or NULL if this is a part.
 * Output: none
 * Side-effects: Updates the client's oper_warn_count_down, warns the
 *    IRC operators if necessary, and updates join_leave_countdown as
 *    needed.
 */
void
check_spambot_warning(struct Client *source_p, const char *name)
{
	int t_delta;
	int decrement_count;
	if((GlobalSetOptions.spam_num &&
	    (source_p->localClient->join_leave_count >= GlobalSetOptions.spam_num)))
	{
		if(source_p->localClient->oper_warn_count_down > 0)
			source_p->localClient->oper_warn_count_down--;
		else
			source_p->localClient->oper_warn_count_down = 0;
		if(source_p->localClient->oper_warn_count_down == 0 &&
				name != NULL)
		{
			/* Its already known as a possible spambot */
			sendto_realops_snomask(SNO_BOTS, L_NETWIDE,
					     "User %s (%s@%s) trying to join %s is a possible spambot",
					     source_p->name,
					     source_p->username, source_p->orighost, name);
			source_p->localClient->oper_warn_count_down = OPER_SPAM_COUNTDOWN;
		}
	}
	else
	{
		if((t_delta =
		    (rb_current_time() - source_p->localClient->last_leave_time)) >
		   JOIN_LEAVE_COUNT_EXPIRE_TIME)
		{
			decrement_count = (t_delta / JOIN_LEAVE_COUNT_EXPIRE_TIME);
			if(name != NULL)
				;
			else if(decrement_count > source_p->localClient->join_leave_count)
				source_p->localClient->join_leave_count = 0;
			else
				source_p->localClient->join_leave_count -= decrement_count;
		}
		else
		{
			if((rb_current_time() -
			    (source_p->localClient->last_join_time)) < GlobalSetOptions.spam_time)
			{
				/* oh, its a possible spambot */
				source_p->localClient->join_leave_count++;
			}
		}
		if(name != NULL)
			source_p->localClient->last_join_time = rb_current_time();
		else
			source_p->localClient->last_leave_time = rb_current_time();
	}
}

/* check_splitmode()
 *
 * input	-
 * output	-
 * side effects - compares usercount and servercount against their split
 *                values and adjusts splitmode accordingly
 */
void
check_splitmode(void *unused)
{
	if(splitchecking && (ConfigChannel.no_join_on_split || ConfigChannel.no_create_on_split))
	{
		/* not split, we're being asked to check now because someone
		 * has left
		 */
		if(!splitmode)
		{
			if(eob_count < split_servers || Count.total < split_users)
			{
				splitmode = 1;
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						     "Network split, activating splitmode");
				check_splitmode_ev = rb_event_addish("check_splitmode", check_splitmode, NULL, 2);
			}
		}
		/* in splitmode, check whether its finished */
		else if(eob_count >= split_servers && Count.total >= split_users)
		{
			splitmode = 0;

			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					     "Network rejoined, deactivating splitmode");

			rb_event_delete(check_splitmode_ev);
			check_splitmode_ev = NULL;
		}
	}
}


/* allocate_topic()
 *
 * input	- channel to allocate topic for
 * output	- 1 on success, else 0
 * side effects - channel gets a topic allocated
 */
static void
allocate_topic(struct Channel *chptr)
{
	void *ptr;

	if(chptr == NULL)
		return;

	ptr = rb_bh_alloc(topic_heap);

	/* Basically we allocate one large block for the topic and
	 * the topic info.  We then split it up into two and shove it
	 * in the chptr 
	 */
	chptr->topic = ptr;
	chptr->topic_info = (char *) ptr + TOPICLEN + 1;
	*chptr->topic = '\0';
	*chptr->topic_info = '\0';
}

/* free_topic()
 *
 * input	- channel which has topic to free
 * output	-
 * side effects - channels topic is free'd
 */
static void
free_topic(struct Channel *chptr)
{
	void *ptr;

	if(chptr == NULL || chptr->topic == NULL)
		return;

	/* This is safe for now - If you change allocate_topic you
	 * MUST change this as well
	 */
	ptr = chptr->topic;
	rb_bh_free(topic_heap, ptr);
	chptr->topic = NULL;
	chptr->topic_info = NULL;
}

/* set_channel_topic()
 *
 * input	- channel, topic to set, topic info and topic ts
 * output	-
 * side effects - channels topic, topic info and TS are set.
 */
void
set_channel_topic(struct Channel *chptr, const char *topic, const char *topic_info, time_t topicts)
{
	if(strlen(topic) > 0)
	{
		if(chptr->topic == NULL)
			allocate_topic(chptr);
		rb_strlcpy(chptr->topic, topic, TOPICLEN + 1);
		rb_strlcpy(chptr->topic_info, topic_info, USERHOST_REPLYLEN);
		chptr->topic_time = topicts;
	}
	else
	{
		if(chptr->topic != NULL)
			free_topic(chptr);
		chptr->topic_time = 0;
	}
}

/* channel_modes()
 *
 * inputs       - pointer to channel
 *              - pointer to client
 * output       - string with simple modes
 * side effects - result from previous calls overwritten
 *
 * Stolen from ShadowIRCd 4 --nenolod
 */
const char *
channel_modes(struct Channel *chptr, struct Client *client_p)
{
	int i;
	char buf1[BUFSIZE];
	char buf2[BUFSIZE];
	static char final[BUFSIZE];
	char *mbuf = buf1;
	char *pbuf = buf2;

	*mbuf++ = '+';
	*pbuf = '\0';

	for (i = 0; i < 256; i++)
		if(chptr->mode.mode & chmode_flags[i])
			*mbuf++ = i;

	if(chptr->mode.limit)
	{
		*mbuf++ = 'l';

		if(!IsClient(client_p) || IsMember(client_p, chptr))
			pbuf += rb_sprintf(pbuf, " %d", chptr->mode.limit);
	}

	if(*chptr->mode.key)
	{
		*mbuf++ = 'k';

		if(pbuf > buf2 || !IsClient(client_p) || IsMember(client_p, chptr))
			pbuf += rb_sprintf(pbuf, " %s", chptr->mode.key);
	}

	if(chptr->mode.join_num)
	{
		*mbuf++ = 'j';

		if(pbuf > buf2 || !IsClient(client_p) || IsMember(client_p, chptr))
			pbuf += rb_sprintf(pbuf, " %d:%d", chptr->mode.join_num,
					   chptr->mode.join_time);
	}

	if(*chptr->mode.forward && (ConfigChannel.use_forward || !IsClient(client_p)))
	{
		*mbuf++ = 'f';

		if(pbuf > buf2 || !IsClient(client_p) || IsMember(client_p, chptr))
			pbuf += rb_sprintf(pbuf, " %s", chptr->mode.forward);
	}

	*mbuf = '\0';

	rb_strlcpy(final, buf1, sizeof final);
	rb_strlcat(final, buf2, sizeof final);
	return final;
}

/* Now lets do some stuff to keep track of what combinations of
 * servers exist...
 * Note that the number of combinations doubles each time you add
 * something to this list. Each one is only quick if no servers use that
 * combination, but if the numbers get too high here MODE will get too
 * slow. I suggest if you get more than 7 here, you consider getting rid
 * of some and merging or something. If it wasn't for irc+cs we would
 * probably not even need to bother about most of these, but unfortunately
 * we do. -A1kmm
 */

/* void init_chcap_usage_counts(void)
 *
 * Inputs	- none
 * Output	- none
 * Side-effects	- Initialises the usage counts to zero. Fills in the
 *                chcap_yes and chcap_no combination tables.
 */
void
init_chcap_usage_counts(void)
{
	unsigned long m, c, y, n;

	memset(chcap_combos, 0, sizeof(chcap_combos));

	/* For every possible combination */
	for (m = 0; m < NCHCAP_COMBOS; m++)
	{
		/* Check each capab */
		for (c = y = n = 0; c < NCHCAPS; c++)
		{
			if((m & (1 << c)) == 0)
				n |= channel_capabs[c];
			else
				y |= channel_capabs[c];
		}
		chcap_combos[m].cap_yes = y;
		chcap_combos[m].cap_no = n;
	}
}

/* void set_chcap_usage_counts(struct Client *serv_p)
 * Input: serv_p; The client whose capabs to register.
 * Output: none
 * Side-effects: Increments the usage counts for the correct capab
 *               combination.
 */
void
set_chcap_usage_counts(struct Client *serv_p)
{
	int n;

	for (n = 0; n < NCHCAP_COMBOS; n++)
	{
		if(IsCapable(serv_p, chcap_combos[n].cap_yes) &&
		   NotCapable(serv_p, chcap_combos[n].cap_no))
		{
			chcap_combos[n].count++;
			return;
		}
	}

	/* This should be impossible -A1kmm. */
	s_assert(0);
}

/* void set_chcap_usage_counts(struct Client *serv_p)
 *
 * Inputs	- serv_p; The client whose capabs to register.
 * Output	- none
 * Side-effects	- Decrements the usage counts for the correct capab
 *                combination.
 */
void
unset_chcap_usage_counts(struct Client *serv_p)
{
	int n;

	for (n = 0; n < NCHCAP_COMBOS; n++)
	{
		if(IsCapable(serv_p, chcap_combos[n].cap_yes) &&
		   NotCapable(serv_p, chcap_combos[n].cap_no))
		{
			/* Hopefully capabs can't change dynamically or anything... */
			s_assert(chcap_combos[n].count > 0);

			if(chcap_combos[n].count > 0)
				chcap_combos[n].count--;
			return;
		}
	}

	/* This should be impossible -A1kmm. */
	s_assert(0);
}

/* void send_cap_mode_changes(struct Client *client_p,
 *                        struct Client *source_p,
 *                        struct Channel *chptr, int cap, int nocap)
 * Input: The client sending(client_p), the source client(source_p),
 *        the channel to send mode changes for(chptr)
 * Output: None.
 * Side-effects: Sends the appropriate mode changes to capable servers.
 *
 * Reverted back to my original design, except that we now keep a count
 * of the number of servers which each combination as an optimisation, so
 * the capabs combinations which are not needed are not worked out. -A1kmm
 */
void
send_cap_mode_changes(struct Client *client_p, struct Client *source_p,
		      struct Channel *chptr, struct ChModeChange mode_changes[], int mode_count)
{
	static char modebuf[BUFSIZE];
	static char parabuf[BUFSIZE];
	int i, mbl, pbl, nc, mc, preflen, len;
	char *pbuf;
	const char *arg;
	int dir;
	int j;
	int cap;
	int nocap;
	int arglen;

	/* Now send to servers... */
	for (j = 0; j < NCHCAP_COMBOS; j++)
	{
		if(chcap_combos[j].count == 0)
			continue;

		mc = 0;
		nc = 0;
		pbl = 0;
		parabuf[0] = 0;
		pbuf = parabuf;
		dir = MODE_QUERY;

		cap = chcap_combos[j].cap_yes;
		nocap = chcap_combos[j].cap_no;

		mbl = preflen = rb_sprintf(modebuf, ":%s TMODE %ld %s ",
					   use_id(source_p), (long) chptr->channelts,
					   chptr->chname);

		/* loop the list of - modes we have */
		for (i = 0; i < mode_count; i++)
		{
			/* if they dont support the cap we need, or they do support a cap they
			 * cant have, then dont add it to the modebuf.. that way they wont see
			 * the mode
			 */
			if((mode_changes[i].letter == 0) ||
			   ((cap & mode_changes[i].caps) != mode_changes[i].caps)
			   || ((nocap & mode_changes[i].nocaps) != mode_changes[i].nocaps))
				continue;

			if(!EmptyString(mode_changes[i].id))
				arg = mode_changes[i].id;
			else
				arg = mode_changes[i].arg;

			if(arg)
			{
				arglen = strlen(arg);

				/* dont even think about it! --fl */
				if(arglen > MODEBUFLEN - 5)
					continue;
			}

			/* if we're creeping past the buf size, we need to send it and make
			 * another line for the other modes
			 * XXX - this could give away server topology with uids being
			 * different lengths, but not much we can do, except possibly break
			 * them as if they were the longest of the nick or uid at all times,
			 * which even then won't work as we don't always know the uid -A1kmm.
			 */
			if(arg && ((mc == MAXMODEPARAMSSERV) ||
				   ((mbl + pbl + arglen + 4) > (BUFSIZE - 3))))
			{
				if(nc != 0)
					sendto_server(client_p, chptr, cap, nocap,
						      "%s %s", modebuf, parabuf);
				nc = 0;
				mc = 0;

				mbl = preflen;
				pbl = 0;
				pbuf = parabuf;
				parabuf[0] = 0;
				dir = MODE_QUERY;
			}

			if(dir != mode_changes[i].dir)
			{
				modebuf[mbl++] = (mode_changes[i].dir == MODE_ADD) ? '+' : '-';
				dir = mode_changes[i].dir;
			}

			modebuf[mbl++] = mode_changes[i].letter;
			modebuf[mbl] = 0;
			nc++;

			if(arg != NULL)
			{
				len = rb_sprintf(pbuf, "%s ", arg);
				pbuf += len;
				pbl += len;
				mc++;
			}
		}

		if(pbl && parabuf[pbl - 1] == ' ')
			parabuf[pbl - 1] = 0;

		if(nc != 0)
			sendto_server(client_p, chptr, cap, nocap, "%s %s", modebuf, parabuf);
	}
}

void 
resv_chan_forcepart(const char *name, const char *reason, int temp_time)
{
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	struct Channel *chptr;
	struct membership *msptr;
	struct Client *target_p;

	if(!ConfigChannel.resv_forcepart)
		return;

	/* for each user on our server in the channel list
	 * send them a PART, and notify opers.
	 */
	chptr = find_channel(name);
	if(chptr != NULL)
	{
		RB_DLINK_FOREACH_SAFE(ptr, next_ptr, chptr->locmembers.head)
		{
			msptr = ptr->data;
			target_p = msptr->client_p;

			if(IsExemptResv(target_p))
				continue;

			sendto_server(target_p, chptr, CAP_TS6, NOCAPS,
			              ":%s PART %s", target_p->id, chptr->chname);

			sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s PART %s :%s",
			                     target_p->name, target_p->username,
			                     target_p->host, chptr->chname, target_p->name);

			remove_user_from_channel(msptr);

			/* notify opers & user they were removed from the channel */
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
			                     "Forced PART for %s!%s@%s from %s (%s)",
			                     target_p->name, target_p->username, 
			                     target_p->host, name, reason);

			if(temp_time > 0)
				sendto_one_notice(target_p, ":*** Channel %s is temporarily unavailable on this server.",
				           name);
			else
				sendto_one_notice(target_p, ":*** Channel %s is no longer available on this server.",
				           name);
		}
	}
}
