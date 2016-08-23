/*
 * charybdis: an advanced Internet Relay Chat Daemon(ircd).
 * tgchange.c - code for restricting private messages
 *
 * Copyright (C) 2004-2005 Lee Hardy <lee@leeh.co.uk>
 * Copyright (C) 2005-2010 Jilles Tjoelker <jilles@stack.nl>
 * Copyright (C) 2004-2005 ircd-ratbox development team
 * Copyright (C) 2016 Charybdis Development Team
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

namespace ircd     {
namespace tgchange {

static bool add_target(client &source, const uint32_t &hashv);

} // namespace tgchange
} // namespace ircd;

namespace tgchange = ircd::tgchange;
using ircd::client::client;
using ircd::chan::chan;

chan *
tgchange::find_allowing_channel(client &source,
                                const client &target)
{
	for(const auto &pit : chans(user(source)))
	{
		auto &chan(*pit.first);
		const auto &member(*pit.second);
		if((is_chanop(member) || is_voiced(member)) && is_member(chan, target))
			return &chan;
	}

	return nullptr;
}

const chan *
tgchange::find_allowing_channel(const client &source,
                                const client &target)
{
	for(const auto &pit : chans(user(source)))
	{
		const auto &chan(*pit.first);
		const auto &member(*pit.second);
		if((is_chanop(member) || is_voiced(member)) && is_member(chan, target))
			return &chan;
	}

	return nullptr;
}

bool
tgchange::add_target(client &source,
                     const client &target)
{
	/* can msg themselves or services without using any target slots */
	if(source == target || is(target, umode::SERVICE))
		return true;

	/* special condition for those who have had PRIVMSG crippled to allow them
	 * to talk to IRCops still.
	 *
	 * XXX: is this controversial?
	 */
	if(source.localClient->target_last > rb_current_time() && is(target, umode::OPER))
		return true;

	const auto hashv(fnv_hash_upper(use_id(target), 32));
	return add_target(source, hashv);
}

bool
tgchange::add_target(client &source,
                     const chan &target)
{
	if(!ConfigChannel.channel_target_change)
		return true;

	const auto hashv(fnv_hash_upper(name(target).c_str(), 32));
	return add_target(source, hashv);
}

void
tgchange::add_reply_target(client &source,
                           const client &target)
{
	/* can msg themselves or services without using any target slots */
	if(source == target || is(target, umode::SERVICE))
		return;

	auto &targets(source.localClient->targets);
	static_assert(NUM + REPLY == sizeof(targets) / sizeof(uint32_t), "");

	/* check for existing target, and move it to the first reply slot
	 * if it is in a reply slot
	 */
	const auto hashv(fnv_hash_upper(use_id(target), 32));
	for(int i(0); i < NUM + REPLY; i++)
	{
		if(targets[i] == hashv)
		{
			if(i > NUM)
			{
				for(int j(i); j > NUM; j--)
					targets[j] = targets[j - 1];
				targets[NUM] = hashv;
			}
			return;
		}
	}

	for(int i(NUM + REPLY - 1); i > NUM; i--)
		targets[i] = targets[i - 1];
	targets[NUM] = hashv;
}


bool
tgchange::add_target(client &source,
                     const uint32_t &hashv)
{
	int i, j;
	uint32_t *targets;

	targets = source.localClient->targets;

	/* check for existing target, and move it to the head */
	for(i = 0; i < NUM + REPLY; i++)
	{
		if(targets[i] == hashv)
		{
			for(j = i; j > 0; j--)
				targets[j] = targets[j - 1];
			targets[0] = hashv;
			return 1;
		}
	}

	if(source.localClient->targets_free < NUM)
	{
		/* first message after connect, we may only start clearing
		 * slots after this message --anfl
		 */
		if(!is_tg_change(source))
		{
			set_tg_change(source);
			source.localClient->target_last = rb_current_time();
		}
		/* clear as many targets as we can */
		else if((i = (rb_current_time() - source.localClient->target_last) / 60))
		{
			if(i + source.localClient->targets_free > NUM)
				source.localClient->targets_free = NUM;
			else
				source.localClient->targets_free += i;

			source.localClient->target_last = rb_current_time();
		}
		/* cant clear any, full target list */
		else if(source.localClient->targets_free == 0)
		{
			ServerStats.is_tgch++;
			add_tgchange(source.sockhost);

			if (!is_tg_excessive(source))
			{
				set_tg_excessive(source);
				/* This is sent to L_ALL because it's regenerated on all servers
				 * that have the TGINFO module loaded.
				 */
				sendto_realops_snomask(SNO_BOTS, L_ALL,
					"Excessive target change from %s (%s@%s)",
					source.name, source.username,
					source.orighost);
			}

			sendto_match_servs(&source, "*", CAP_ENCAP, NOCAPS, "ENCAP * TGINFO 0");
			return false;
		}
	}
	/* no targets in use, reset their target_last so that they cant
	 * abuse a long idle to get targets back more quickly
	 */
	else
	{
		source.localClient->target_last = rb_current_time();
		set_tg_change(source);
	}

	for(i = NUM + REPLY - 1; i > 0; i--)
		targets[i] = targets[i - 1];
	targets[0] = hashv;
	source.localClient->targets_free--;
	return true;
}
