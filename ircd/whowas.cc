/*
 *  ircd-ratbox: A slightly useful ircd.
 *  whowas.c: WHOWAS user cache.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2012 ircd-ratbox development team
 *  Copyright (C) 2016 Charybdis Development Team
 *  Copyright (C) 2016 William Pitcock <nenolod@dereferenced.org>
 *  Copyright (C) 2016 Jason Volk <jason@zemos.net>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

namespace ircd   {
namespace whowas {

id_t id_ctr = 1;
size_t list_max;
std::multimap<id_t, std::shared_ptr<struct whowas>> ids;
std::multimap<std::string, std::shared_ptr<struct whowas>, rfc1459::less> nicks;

void trim();

} // namespace whowas
} // namespace ircd

using namespace ircd;

void
whowas::init()
{
	if(!list_max)
		list_max = NICKNAMEHISTORYLENGTH;
}

void
whowas::set_size(const size_t &max)
{
	list_max = max;
	trim();
}

void
whowas::memory_usage(size_t *const &count,
                     size_t *const &memused)
{
	*count = ids.size();
	*memused = ids.size() * sizeof(struct whowas);
}

void
whowas::off(client &client)
{
	const auto &id(client.wwid);
	const auto ppit(ids.equal_range(id));
	std::for_each(ppit.first, ppit.second, []
	(const auto &pit)
	{
		auto &whowas(pit.second);
		whowas->online = nullptr;
	});
}

void
whowas::add(client &client)
{
	// trim some of the entries if we're getting well over our history length
	trim();

	// Client's wwid will be 0 if never seen before
	auto &id(client.wwid);
	if(!id)
		id = id_ctr++;

	// This is an unconditional add to both maps.
	auto it(ids.lower_bound(id));
	it = ids.emplace_hint(it, id, std::make_shared<struct whowas>(client));
	nicks.emplace(client.name, it->second);
}

std::vector<std::shared_ptr<whowas::whowas>>
whowas::history(const std::string &nick,
                const time_t &limit,
                const bool &online)
{
	const auto ppit(nicks.equal_range(nick));
	const auto num(std::distance(ppit.first, ppit.second));
	std::vector<std::shared_ptr<struct whowas>> ret(num);
	std::transform(ppit.first, ppit.second, begin(ret), []
	(const auto &pit)
	{
		return pit.second;
	});

	// C++11 says the multimap has stronger ordering and preserves
	// the insert order, which should already be the logoff time, so stuff below
	// this comment can be optimized at a later pass.
	std::sort(begin(ret), end(ret), []
	(const auto &a, const auto &b)
	{
		return a->logoff < b->logoff;
	});

	const auto e(std::remove_if(begin(ret), end(ret), [&limit, &online]
	(const auto &whowas)
	{
		if(online && !whowas->online)
			return true;

		if(limit && whowas->logoff + limit < rb_current_time())
			return true;

		return false;
	}));

	ret.erase(e, end(ret));
	return ret;
}

std::vector<std::shared_ptr<whowas::whowas>>
whowas::history(const client &client)
{
	return history(client.wwid);
}

std::vector<std::shared_ptr<whowas::whowas>>
whowas::history(const id_t &wwid)
{
	const auto ppit(ids.equal_range(wwid));
	const auto num(std::distance(ppit.first, ppit.second));
	std::vector<std::shared_ptr<struct whowas>> ret(num);
	std::transform(ppit.first, ppit.second, begin(ret), []
	(const auto &pit)
	{
		return pit.second;
	});

	return ret;
}

void
whowas::trim()
{
	// Trims by oldest ID until satisfied.

	auto it(begin(ids));
	while(it != end(ids) && nicks.size() > list_max)
	{
		const auto &id(it->first);
		const auto &whowas(it->second);
		const auto nick_ppit(nicks.equal_range(whowas->name));
		for(auto pit(nick_ppit.first); pit != nick_ppit.second; )
		{
			const auto &nick_whowas(pit->second);
			if(nick_whowas->wwid == whowas->wwid)
				nicks.erase(pit++);
			else
				++pit;
		}

		ids.erase(it++);
	}
}

whowas::whowas::whowas(client &client)
:wwid{client.wwid}
,online{&client}
,logoff{rb_current_time()}
,scache
{
	client.serv? nameinfo(serv(client)) : nullptr
}
,flag
{
	is_ip_spoof(client)?   IP_SPOOFING   : (enum flag)0 |
	is_dyn_spoof(client)?  DYNSPOOF      : (enum flag)0
}
{
	rb_strlcpy(name, client.name, sizeof(name));
	rb_strlcpy(username, client.username, sizeof(username));
	rb_strlcpy(hostname, client.host, sizeof(hostname));
	rb_strlcpy(realname, client.info, sizeof(realname));
	rb_strlcpy(sockhost, client.sockhost, sizeof(sockhost));
	assert(wwid);
}
