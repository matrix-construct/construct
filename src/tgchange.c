/*
 * charybdis: an advanced Internet Relay Chat Daemon(ircd).
 * tgchange.c - code for restricting private messages
 *
 * Copyright (C) 2004-2005 Lee Hardy <lee@leeh.co.uk>
 * Copyright (C) 2005-2010 Jilles Tjoelker <jilles@stack.nl>
 * Copyright (C) 2004-2005 ircd-ratbox development team
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
#include "tgchange.h"
#include "channel.h"
#include "client.h"
#include "s_stats.h"
#include "hash.h"
#include "s_newconf.h"

static int add_hashed_target(struct Client *source_p, uint32_t hashv);

struct Channel *
find_allowing_channel(struct Client *source_p, struct Client *target_p)
{
	rb_dlink_node *ptr;
	struct membership *msptr;

	RB_DLINK_FOREACH(ptr, source_p->user->channel.head)
	{
		msptr = ptr->data;
		if (is_chanop_voiced(msptr) && IsMember(target_p, msptr->chptr))
			return msptr->chptr;
	}
	return NULL;
}

int
add_target(struct Client *source_p, struct Client *target_p)
{
	uint32_t hashv;

	/* can msg themselves or services without using any target slots */
	if(source_p == target_p || IsService(target_p))
		return 1;

	/* special condition for those who have had PRIVMSG crippled to allow them
	 * to talk to IRCops still.
	 *
	 * XXX: is this controversial?
	 */
	if(source_p->localClient->target_last > rb_current_time() && IsOper(target_p))
		return 1;

	hashv = fnv_hash_upper((const unsigned char *)use_id(target_p), 32);
	return add_hashed_target(source_p, hashv);
}

int
add_channel_target(struct Client *source_p, struct Channel *chptr)
{
	uint32_t hashv;

	hashv = fnv_hash_upper((const unsigned char *)chptr->chname, 32);
	return add_hashed_target(source_p, hashv);
}

static int
add_hashed_target(struct Client *source_p, uint32_t hashv)
{
	int i, j;
	uint32_t *targets;

	targets = source_p->localClient->targets;

	/* check for existing target, and move it to the head */
	for(i = 0; i < TGCHANGE_NUM + TGCHANGE_REPLY; i++)
	{
		if(targets[i] == hashv)
		{
			for(j = i; j > 0; j--)
				targets[j] = targets[j - 1];
			targets[0] = hashv;
			return 1;
		}
	}

	if(source_p->localClient->targets_free < TGCHANGE_NUM)
	{
		/* first message after connect, we may only start clearing
		 * slots after this message --anfl
		 */
		if(!IsTGChange(source_p))
		{
			SetTGChange(source_p);
			source_p->localClient->target_last = rb_current_time();
		}
		/* clear as many targets as we can */
		else if((i = (rb_current_time() - source_p->localClient->target_last) / 60))
		{
			if(i + source_p->localClient->targets_free > TGCHANGE_NUM)
				source_p->localClient->targets_free = TGCHANGE_NUM;
			else
				source_p->localClient->targets_free += i;

			source_p->localClient->target_last = rb_current_time();
		}
		/* cant clear any, full target list */
		else if(source_p->localClient->targets_free == 0)
		{
			ServerStats.is_tgch++;
			add_tgchange(source_p->sockhost);
			return 0;
		}
	}
	/* no targets in use, reset their target_last so that they cant
	 * abuse a long idle to get targets back more quickly
	 */
	else
	{
		source_p->localClient->target_last = rb_current_time();
		SetTGChange(source_p);
	}

	for(i = TGCHANGE_NUM + TGCHANGE_REPLY - 1; i > 0; i--)
		targets[i] = targets[i - 1];
	targets[0] = hashv;
	source_p->localClient->targets_free--;
	return 1;
}

void
add_reply_target(struct Client *source_p, struct Client *target_p)
{
	int i, j;
	uint32_t hashv;
	uint32_t *targets;

	/* can msg themselves or services without using any target slots */
	if(source_p == target_p || IsService(target_p))
		return;

	hashv = fnv_hash_upper((const unsigned char *)use_id(target_p), 32);
	targets = source_p->localClient->targets;

	/* check for existing target, and move it to the first reply slot
	 * if it is in a reply slot
	 */
	for(i = 0; i < TGCHANGE_NUM + TGCHANGE_REPLY; i++)
	{
		if(targets[i] == hashv)
		{
			if(i > TGCHANGE_NUM)
			{
				for(j = i; j > TGCHANGE_NUM; j--)
					targets[j] = targets[j - 1];
				targets[TGCHANGE_NUM] = hashv;
			}
			return;
		}
	}
	for(i = TGCHANGE_NUM + TGCHANGE_REPLY - 1; i > TGCHANGE_NUM; i--)
		targets[i] = targets[i - 1];
	targets[TGCHANGE_NUM] = hashv;
}
