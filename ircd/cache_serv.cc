/*
 *  charybdis
 *  scache.c: Server names cache.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *  Copyright (C) 2005-2016 Charybdis Development Team
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
/*
 * ircd used to store full servernames in anUser as well as in the
 * whowas info.  there can be some 40k such structures alive at any
 * given time, while the number of unique server names a server sees
 * in its lifetime is at most a few hundred.  by tokenizing server
 * names internally, the server can easily save 2 or 3 megs of RAM.
 * -orabidoo
 *
 * reworked to serve for flattening/delaying /links also
 * -- jilles
 *
 * reworked to make use of rb_radixtree.
 * -- kaniini
 *
 * reworked to use standard template container.
 * -- jzk
 */

using ircd::client::client;
using ircd::cache::serv::entry;
using namespace ircd;

struct cache::serv::entry
{
	std::string name;
	std::string info;
	time_t known_since;
	time_t last_connect;
	time_t last_split;
	enum flag flag;

	entry(const std::string &name, const std::string &info, const enum flag &flag);
};

cache::serv::entry::entry(const std::string &name,
                          const std::string &info,
                          const enum flag &flag)
:name{name}
,info{info}
,known_since{rb_current_time()}
,last_connect{0}
,last_split{0}
,flag{flag | ONLINE}
{
}

std::map<const std::string *, std::shared_ptr<entry>, rfc1459::less> ents;

std::shared_ptr<entry>
cache::serv::connect(const std::string &name,
                     const std::string &info,
                     const bool &hidden)
{
	return connect(name, info, flag(hidden));
}

std::shared_ptr<entry>
cache::serv::connect(const std::string &name,
                     const std::string &info,
                     const flag &flag)
{
	auto it(ents.lower_bound(&name));
	if(it == end(ents) || *it->first != name)
	{
		auto entry(std::make_shared<struct entry>(name, info, flag));
		const auto *const name_p(&entry->name);
		it = ents.emplace_hint(it, name_p, std::move(entry));
		return it->second;
	}

	auto &entry(it->second);
	entry->last_connect = rb_current_time();
	return entry;
}

void
cache::serv::clear()
{
	ents.clear();
}

/*
 * count_scache
 * inputs   - pointer to where to leave number of servers cached
 *          - pointer to where to leave total memory usage
 * side effects	-
 */
size_t
cache::serv::count_bytes()
{
	return ents.size() * sizeof(entry);
}

size_t
cache::serv::count_servers()
{
	return ents.size();
}

/* scache_send_flattened_links()
 *
 * inputs	- client to send to
 * outputs	- the cached links, us, and RPL_ENDOFLINKS
 * side effects	-
 */
void
cache::serv::send_flattened_links(client &source)
{
	for(const auto &pair : ents)
	{
		const auto &entry(*pair.second);

		if(irccmp(name(entry), me.name) == 0)
			continue;

		if(flags(entry) & HIDDEN && !ConfigServerHide.disable_hidden)
			continue;

		if(flags(entry) & ONLINE && entry.known_since >= rb_current_time() - ConfigServerHide.links_delay)
			continue;

		if(~flags(entry) & ONLINE && entry.last_split <= rb_current_time() - ConfigServerHide.links_delay)
			continue;

		if(~flags(entry) & ONLINE && entry.last_split - entry.known_since <= ConfigServerHide.links_delay)
			continue;

		sendto_one_numeric(&source, RPL_LINKS, form_str(RPL_LINKS),
		                   name(entry).c_str(),
		                   me.name,
		                   1,
		                   entry.info.c_str());
	}

	sendto_one_numeric(&source, RPL_LINKS, form_str(RPL_LINKS),
	                   me.name,
	                   me.name,
	                   0,
	                   me.info);

	sendto_one_numeric(&source, RPL_ENDOFLINKS, form_str(RPL_ENDOFLINKS),
	                   "*");
}

/* scache_send_missing()
 *
 * inputs	- client to send to
 * outputs	- recently split servers
 * side effects	-
 */
void
cache::serv::send_missing(client &source)
{
	static const auto MISSING_TIMEOUT = 60 * 60 * 24;

	for(const auto &pair : ents)
	{
		const auto &entry(*pair.second);

		if(flags(entry) & ONLINE)
			continue;

		if(entry.last_split <= rb_current_time() - MISSING_TIMEOUT)
			continue;

		sendto_one_numeric(&source, RPL_MAP, "** %s (recently split)",
		                   name(entry).c_str());
	}
}

void
cache::serv::split(std::shared_ptr<entry> &entry)
{
	if(entry)
		split(*entry);
}

void
cache::serv::split(entry &entry)
{
	entry.flag &= ~ONLINE;
	entry.last_split = rb_current_time();
}

const cache::serv::flag &
cache::serv::flags(const entry &entry)
{
	return entry.flag;
}

const std::string &
cache::serv::name(const entry &entry)
{
	return entry.name;
}
