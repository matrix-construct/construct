// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	extern conf::item<seconds> alias_fetch_timeout;
	extern conf::item<seconds> alias_cache_ttl;
}

decltype(ircd::m::alias_cache_ttl)
ircd::m::alias_cache_ttl
{
	{ "name",    "ircd.m.room.aliases.cache.ttl" },
	{ "default", 604800L                         },
};

decltype(ircd::m::alias_fetch_timeout)
ircd::m::alias_fetch_timeout
{
	{ "name",    "ircd.m.room.aliases.fetch.timeout" },
	{ "default", 10L                                 },
};

//
// m::room::aliases
//

size_t
ircd::m::room::aliases::count()
const
{
	return count(string_view{});
}

size_t
ircd::m::room::aliases::count(const string_view &server)
const
{
	size_t ret(0);
	for_each(server, [&ret](const auto &a)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::room::aliases::has(const alias &alias)
const
{
	return !for_each(alias.host(), [&alias]
	(const id::room_alias &a)
	{
		assert(a.host() == alias.host());
		return a == alias? false : true; // false to break on found
	});
}

bool
ircd::m::room::aliases::for_each(const closure_bool &closure)
const
{
	const room::state state
	{
		room
	};

	return state.for_each("m.room.aliases", [this, &closure]
	(const string_view &type, const string_view &state_key, const event::idx &)
	{
		return for_each(state_key, closure);
	});
}

bool
ircd::m::room::aliases::for_each(const string_view &server,
                                 const closure_bool &closure)
const
{
	if(!server)
		return for_each(closure);

	return for_each(room, server, closure);
}

bool
ircd::m::room::aliases::for_each(const m::room &room,
                                 const string_view &server,
                                 const closure_bool &closure)
{
	const room::state state
	{
		room
	};

	assert(server);
	const event::idx &event_idx
	{
		state.get(std::nothrow, "m.room.aliases", server)
	};

	if(!event_idx)
		return true;

	bool ret{true};
	m::get(std::nothrow, event_idx, "content", [&closure, &ret]
	(const json::object &content)
	{
		const json::array &aliases
		{
			content["aliases"]
		};

		for(auto it(begin(aliases)); it != end(aliases) && ret; ++it)
		{
			const json::string &alias(*it);
			if(!valid(m::id::ROOM_ALIAS, alias))
				continue;

			if(!closure(alias))
				ret = false;
		}
	});

	return ret;
}

//
// m::room::aliases::cache
//

bool
ircd::m::room::aliases::cache::del(const alias &alias)
{
	char buf[m::id::room_alias::buf::SIZE];
	const string_view &key
	{
		make_key(buf, alias)
	};

	const m::room::id::buf alias_room_id
	{
		"alias", origin(my())
	};

	const m::room alias_room
	{
		alias_room_id
	};

	const auto &event_idx
	{
		alias_room.get(std::nothrow, "ircd.room.alias", key)
	};

	if(!event_idx)
		return false;

	const auto event_id
	{
		m::event_id(std::nothrow, event_idx)
	};

	if(!event_id)
		return false;

	const auto ret
	{
		redact(alias_room, me(), event_id, "deleted")
	};

	return true;
}

bool
ircd::m::room::aliases::cache::set(const alias &alias,
                                   const id &id)
{
	char buf[m::id::room_alias::buf::SIZE];
	const string_view &key
	{
		make_key(buf, alias)
	};

	const m::room::id::buf alias_room_id
	{
		"alias", origin(my())
	};

	const m::room alias_room
	{
		alias_room_id
	};

	const auto ret
	{
		send(alias_room, me(), "ircd.room.alias", key,
		{
			{ "room_id", id }
		})
	};

	return true;
}

bool
ircd::m::room::aliases::cache::get(std::nothrow_t,
                                   const alias &alias,
                                   const id::closure &closure)
{
	m::event::idx event_idx
	{
		getidx(alias)
	};

	const bool expired
	{
		!my_host(alias.host()) && (!event_idx || cache::expired(event_idx))
	};

	if(!event_idx || expired)
	{
		if(my_host(alias.host()))
			return false;

		if(expired)
			log::dwarning
			{
				log, "Cached alias %s expired age:%ld ttl:%ld",
				string_view{alias},
				cache::age(event_idx).count(),
				milliseconds(seconds(alias_cache_ttl)).count(),
			};

		if(!fetch(std::nothrow, alias, alias.host()))
			return false;

		if(!(event_idx = getidx(alias)))
			return false;
	}

	bool ret{false};
	m::get(std::nothrow, event_idx, "content", [&closure, &ret]
	(const json::object &content)
	{
		const json::string &room_id
		{
			content.get("room_id")
		};

		if(!empty(room_id))
		{
			ret = true;
			closure(room_id);
		}
	});

	return ret;
}

bool
ircd::m::room::aliases::cache::fetch(std::nothrow_t,
                                     const alias &a,
                                     const string_view &remote)
try
{
	fetch(a, remote);
	return true;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to fetch room_id for %s from %s :%s",
		string_view{a},
		remote,
		e.what(),
	};

	return false;
}

ircd::m::room::id::buf
ircd::m::room::aliases::cache::get(const alias &a)
{
	id::buf ret;
	get(a, [&ret]
	(const id &room_id)
	{
		ret = room_id;
	});

	return ret;
}

ircd::m::room::id::buf
ircd::m::room::aliases::cache::get(std::nothrow_t,
                                   const alias &a)
{
	id::buf ret;
	get(std::nothrow, a, [&ret]
	(const id &room_id)
	{
		ret = room_id;
	});

	return ret;
}

void
ircd::m::room::aliases::cache::get(const alias &a,
                                   const id::closure &c)
{
	if(!get(std::nothrow, a, c))
		throw m::NOT_FOUND
		{
			"Cannot find room_id for %s",
			string_view{a}
		};
}

bool
ircd::m::room::aliases::cache::for_each(const closure_bool &c)
{
	return for_each(string_view{}, c);
}

void
ircd::m::room::aliases::cache::fetch(const alias &alias,
                                     const string_view &remote)
try
{
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	m::fed::query::opts opts;
	opts.remote = remote;
	opts.dynamic = false;

	m::fed::query::directory request
	{
		alias, buf, std::move(opts)
    };

	const http::code &code
	{
		request.get(seconds(alias_fetch_timeout))
	};

	const json::object response
	{
		request
	};

	const json::string &room_id
	{
		response["room_id"]
	};

	if(!valid(m::id::ROOM, room_id))
		throw m::NOT_FOUND
		{
			"Server '%s' does not know room_id for %s",
			remote,
			string_view{alias},
		};

	set(alias, m::id::room(room_id));
}
catch(const ctx::timeout &e)
{
	throw m::error
	{
		http::GATEWAY_TIMEOUT, "M_ROOM_ALIAS_TIMEOUT",
		"Server '%s' did not respond with a room_id for %s in time",
		remote,
		string_view{alias},
	};
}
catch(const server::unavailable &e)
{
	throw m::error
	{
		http::BAD_GATEWAY, "M_ROOM_ALIAS_UNAVAILABLE",
		"Server '%s' is not available to query a room_id for %s",
		remote,
		string_view{alias},
	};
}

bool
ircd::m::room::aliases::cache::for_each(const string_view &server,
                                        const closure_bool &closure)
{
	const m::room::id::buf alias_room_id
	{
		"alias", origin(my())
	};

	const m::room::state state
	{
		alias_room_id
	};

	bool ret{true};
	const m::room::state::closure_bool reclosure{[&server, &closure, &ret]
	(const string_view &type, const string_view &state_key, const m::event::idx &event_idx)
	{
		thread_local char swapbuf[m::id::room_alias::buf::SIZE];
		const alias &alias
		{
			m::id::unswap(state_key, swapbuf)
		};

		if(server && alias.host() != server)
			return false;

		if(expired(event_idx))
			return true;

		m::get(std::nothrow, event_idx, "content", [&closure, &ret, &alias]
		(const json::object &content)
		{
			const json::string &room_id
			{
				content.get("room_id")
			};

			if(!empty(room_id))
				ret = closure(alias, room_id);
		});

		return ret;
	}};

	state.for_each("ircd.room.alias", server, reclosure);
	return ret;
}

bool
ircd::m::room::aliases::cache::has(const alias &alias)
{
	const auto &event_idx
	{
		getidx(alias)
	};

	if(!event_idx)
		return false;

	if(expired(event_idx))
		return false;

	bool ret{false};
	m::get(std::nothrow, event_idx, "content", [&ret]
	(const json::object &content)
	{
		const json::string &room_id
		{
			content.get("room_id")
		};

		ret = !empty(room_id);
	});

	return ret;
}

ircd::system_point
ircd::m::room::aliases::cache::expires(const alias &alias)
{
	const auto event_idx
	{
		getidx(alias)
	};

	if(!event_idx)
		return system_point::min();

	const milliseconds age
	{
		cache::age(event_idx)
	};

	const seconds ttl
	{
		alias_cache_ttl
	};

	return now<system_point>() + (ttl - age);
}

bool
ircd::m::room::aliases::cache::expired(const event::idx &event_idx)
{
	const milliseconds age
	{
		cache::age(event_idx)
	};

	const seconds ttl
	{
		alias_cache_ttl
	};

	return age > ttl;
}

ircd::milliseconds
ircd::m::room::aliases::cache::age(const event::idx &event_idx)
{
	time_t ts;
	if(!m::get(event_idx, "origin_server_ts", ts))
		return milliseconds::max();

	const time_t now
	{
		ircd::time<milliseconds>()
	};

	return milliseconds
	{
		now - ts
	};
}

ircd::m::event::idx
ircd::m::room::aliases::cache::getidx(const alias &alias)
{
	thread_local char swapbuf alignas(16) [m::id::room_alias::buf::SIZE];
	const string_view &swapped
	{
		alias.swap(swapbuf)
	};

	char buf[m::id::room_alias::buf::SIZE];
	const string_view &key
	{
		tolower(buf, swapped)
	};

	const m::room::id::buf alias_room_id
	{
		"alias", origin(my())
	};

	const m::room alias_room
	{
		alias_room_id
	};

	const auto &event_idx
	{
		alias_room.get(std::nothrow, "ircd.room.alias", key)
	};

	return event_idx;
}

ircd::string_view
ircd::m::room::aliases::cache::make_key(const mutable_buffer &out,
                                        const alias &alias)
{

	thread_local char swapbuf alignas(16) [m::id::room_alias::buf::SIZE];
	const string_view &swapped
	{
		alias.swap(swapbuf)
	};

	const string_view &key
	{
		tolower(out, swapped)
	};

	return key;
}
