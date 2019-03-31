// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Matrix m.room.aliases"
};

const m::room::id::buf
alias_room_id
{
	"alias", ircd::my_host()
};

const m::room
alias_room
{
	alias_room_id
};

//
// m::room::aliases
//

bool
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
ircd::m::room::aliases::cache::del(const alias &alias)
{
	char swapbuf[m::id::room_alias::buf::SIZE];
	const string_view &key
	{
		alias.swap(swapbuf)
	};

	const auto &event_idx
	{
		alias_room.get("ircd.room.alias", key)
	};

	if(!event_idx)
		return false;

	const auto event_id
	{
		m::event_id(event_idx)
	};

	if(!event_id)
		return false;

	const auto ret
	{
		redact(alias_room, m::me.user_id, event_id, "deleted")
	};

	return true;
}

bool
IRCD_MODULE_EXPORT
ircd::m::room::aliases::cache::set(const alias &alias,
                                   const id &id)
{
	char swapbuf[m::id::room_alias::buf::SIZE];
	const string_view &key
	{
		alias.swap(swapbuf)
	};

	const auto ret
	{
		send(alias_room, m::me.user_id, "ircd.room.alias", key,
		{
			{ "room_id", id }
		})
	};

	return true;
}

bool
IRCD_MODULE_EXPORT
ircd::m::room::aliases::cache::get(std::nothrow_t,
                                   const alias &alias,
                                   const id::closure &closure)
{
	char swapbuf[m::id::room_alias::buf::SIZE];
	const string_view &key
	{
		alias.swap(swapbuf)
	};

	const auto &event_idx
	{
		alias_room.get("ircd.room.alias", key)
	};

	if(!event_idx)
		return false;

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
IRCD_MODULE_EXPORT
ircd::m::room::aliases::cache::has(const alias &alias)
{
	char swapbuf[m::id::room_alias::buf::SIZE];
	const string_view &key
	{
		alias.swap(swapbuf)
	};

	return alias_room.has("ircd.room.alias", key);
}

bool
IRCD_MODULE_EXPORT
ircd::m::room::aliases::cache::for_each(const string_view &server,
                                        const closure_bool &closure)
{
	const m::room::state state
	{
		alias_room
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

//
// hook handlers
//

void
_changed_aliases(const m::event &event,
                 m::vm::eval &)
{
	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const json::array &aliases
	{
		at<"content"_>(event).get("aliases")
	};

	for(const json::string &alias_ : aliases)
	{
		const m::room::alias &alias{alias_};
		const auto event_id
		{
			send(alias_room, m::me.user_id, "ircd.alias", alias, json::strung{event})
		};

		log::info
		{
			m::log, "Updated aliases of %s by %s in %s [%s] => %s",
			string_view{room_id},
			json::get<"sender"_>(event),
			json::get<"event_id"_>(event),
			string_view{alias},
			string_view{event_id}
		};
	}
}

const m::hookfn<m::vm::eval &>
_changed_aliases_hookfn
{
	_changed_aliases,
	{
		{ "_site",    "vm.effect"       },
		{ "type",     "m.room.aliases"  },
	}
};

void
_can_change_aliases(const m::event &event,
                    m::vm::eval &eval)
{
	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const json::array &aliases
	{
		at<"content"_>(event).get("aliases")
	};

	for(const json::string &alias_ : aliases)
	{
		const m::room::alias &alias{alias_};
		if(at<"origin"_>(event) != alias.host())
			throw m::ACCESS_DENIED
			{
				"Cannot set alias for host '%s' from origin '%s'",
				alias.host(),
				at<"origin"_>(event)
			};
	}
}

const m::hookfn<m::vm::eval &>
_can_change_aliases_hookfn
{
	_can_change_aliases,
	{
		{ "_site",    "vm.eval"         },
		{ "type",     "m.room.aliases"  },
	}
};
