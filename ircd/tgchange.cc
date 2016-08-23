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

namespace ircd {

static int add_hashed_target(client::client *source_p, uint32_t hashv);

chan::chan *
find_allowing_channel(client::client *source, client::client *target)
{
	for(const auto &pit : chans(user(*source)))
	{
		auto &chan(*pit.first);
		auto &member(*pit.second);

		if ((is_chanop(member) || is_voiced(member)) && is_member(chan, *target))
			return &chan;
	}

	return nullptr;
}

int
add_target(client::client *source_p, client::client *target_p)
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
add_channel_target(client::client *source_p, chan::chan *chptr)
{
	uint32_t hashv;

	if(!ConfigChannel.channel_target_change)
		return 1;

	hashv = fnv_hash_upper((const unsigned char *)chptr->name.c_str(), 32);
	return add_hashed_target(source_p, hashv);
}

static int
add_hashed_target(client::client *source_p, uint32_t hashv)
{
	int i, j;
	uint32_t *targets;

	targets = source_p->localClient->targets;

	/* check for existing target, and move it to the head */
	for(i = 0; i < int(client::tgchange::NUM) + int(client::tgchange::REPLY); i++)
	{
		if(targets[i] == hashv)
		{
			for(j = i; j > 0; j--)
				targets[j] = targets[j - 1];
			targets[0] = hashv;
			return 1;
		}
	}

	if(source_p->localClient->targets_free < int(client::tgchange::NUM))
	{
		/* first message after connect, we may only start clearing
		 * slots after this message --anfl
		 */
		if(!is_tg_change(*source_p))
		{
			set_tg_change(*source_p);
			source_p->localClient->target_last = rb_current_time();
		}
		/* clear as many targets as we can */
		else if((i = (rb_current_time() - source_p->localClient->target_last) / 60))
		{
			if(i + source_p->localClient->targets_free > int(client::tgchange::NUM))
				source_p->localClient->targets_free = int(client::tgchange::NUM);
			else
				source_p->localClient->targets_free += i;

			source_p->localClient->target_last = rb_current_time();
		}
		/* cant clear any, full target list */
		else if(source_p->localClient->targets_free == 0)
		{
			ServerStats.is_tgch++;
			add_tgchange(source_p->sockhost);

			if (!is_tg_excessive(*source_p))
			{
				set_tg_excessive(*source_p);
				/* This is sent to L_ALL because it's regenerated on all servers
				 * that have the TGINFO module loaded.
				 */
				sendto_realops_snomask(SNO_BOTS, L_ALL,
					"Excessive target change from %s (%s@%s)",
					source_p->name, source_p->username,
					source_p->orighost);
			}

			sendto_match_servs(source_p, "*", CAP_ENCAP, NOCAPS,
				"ENCAP * TGINFO 0");

			return 0;
		}
	}
	/* no targets in use, reset their target_last so that they cant
	 * abuse a long idle to get targets back more quickly
	 */
	else
	{
		source_p->localClient->target_last = rb_current_time();
		set_tg_change(*source_p);
	}

	for(i = int(client::tgchange::NUM) + int(client::tgchange::REPLY) - 1; i > 0; i--)
		targets[i] = targets[i - 1];
	targets[0] = hashv;
	source_p->localClient->targets_free--;
	return 1;
}

void
add_reply_target(client::client *source_p, client::client *target_p)
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
	for(i = 0; i < int(client::tgchange::NUM) + int(client::tgchange::REPLY); i++)
	{
		if(targets[i] == hashv)
		{
			if(i > int(client::tgchange::NUM))
			{
				for(j = i; j > int(client::tgchange::NUM); j--)
					targets[j] = targets[j - 1];
				targets[int(client::tgchange::NUM)] = hashv;
			}
			return;
		}
	}
	for(i = int(client::tgchange::NUM) + int(client::tgchange::REPLY) - 1; i > int(client::tgchange::NUM); i--)
		targets[i] = targets[i - 1];
	targets[int(client::tgchange::NUM)] = hashv;
}

}
