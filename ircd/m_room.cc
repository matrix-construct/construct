// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

int64_t
ircd::m::depth(const id::room &room_id)
{
	return std::get<int64_t>(top(room_id));
}

int64_t
ircd::m::depth(std::nothrow_t,
               const id::room &room_id)
{
	const auto it
	{
		dbs::room_events.begin(room_id)
	};

	if(!it)
		return -1;

	const auto part
	{
		dbs::room_events_key(it->first)
	};

	return std::get<0>(part);
}

ircd::m::event::idx
ircd::m::head_idx(const id::room &room_id)
{
	return std::get<event::idx>(top(room_id));
}

ircd::m::event::idx
ircd::m::head_idx(std::nothrow_t,
                  const id::room &room_id)
{
	const auto it
	{
		dbs::room_events.begin(room_id)
	};

	if(!it)
		return 0;

	const auto part
	{
		dbs::room_events_key(it->first)
	};

	return std::get<1>(part);
}

ircd::m::event::id::buf
ircd::m::head(const id::room &room_id)
{
	return std::get<event::id::buf>(top(room_id));
}

ircd::m::event::id::buf
ircd::m::head(std::nothrow_t,
              const id::room &room_id)
{
	return std::get<event::id::buf>(top(std::nothrow, room_id));
}

std::tuple<ircd::m::event::id::buf, int64_t, ircd::m::event::idx>
ircd::m::top(const id::room &room_id)
{
	const auto ret
	{
		top(std::nothrow, room_id)
	};

	if(std::get<int64_t>(ret) == -1)
		throw m::NOT_FOUND
		{
			"No head for room %s", string_view{room_id}
		};

	return ret;
}

std::tuple<ircd::m::event::id::buf, int64_t, ircd::m::event::idx>
ircd::m::top(std::nothrow_t,
             const id::room &room_id)
{
	const auto it
	{
		dbs::room_events.begin(room_id)
	};

	if(!it)
		return
		{
			event::id::buf{}, -1, 0
		};

	const auto part
	{
		dbs::room_events_key(it->first)
	};

	const int64_t &depth
	{
		int64_t(std::get<0>(part))
	};

	const event::idx &event_idx
	{
		std::get<1>(part)
	};

	std::tuple<event::id::buf, int64_t, event::idx> ret
	{
		event::id::buf{}, depth, event_idx
	};

	event::fetch::event_id(event_idx, std::nothrow, [&ret]
	(const event::id &event_id)
	{
		std::get<event::id::buf>(ret) = event_id;
	});

	return ret;
}

ircd::string_view
ircd::m::version(const mutable_buffer &buf,
                 const room &room)
{
	const auto ret
	{
		version(buf, room, std::nothrow)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"Failed to find room %s to query its version",
			string_view{room.room_id}
		};

	return ret;
}

ircd::string_view
ircd::m::version(const mutable_buffer &buf,
                 const room &room,
                 std::nothrow_t)
{
	string_view ret;
	const auto event_idx
	{
		room.get(std::nothrow, "m.room.create", "")
	};

	if(!event_idx)
		return ret;

	m::get(std::nothrow, event_idx, "content", [&ret]
	(const json::object &content)
	{
		ret = unquote(content.get("version", "1"));
	});

	return ret;
}

bool
ircd::m::creator(const id::room &room_id,
                 const id::user &user_id)
{
	const auto creator_user_id
	{
		creator(room_id)
	};

	return creator_user_id == user_id;
}

ircd::m::id::user::buf
ircd::m::creator(const id::room &room_id)
{
	// Query the sender field of the event to get the creator. This is for
	// future compatibility if the content.creator field gets eliminated.
	static const event::fetch::opts fopts
	{
		event::keys::include
		{
			"sender",
		}
	};

	const room::state state
	{
		room_id, &fopts
	};

	id::user::buf ret;
	state.get("m.room.create", "", [&ret]
	(const m::event &event)
	{
		ret = user::id
		{
			json::get<"sender"_>(event)
		};
	});

	return ret;
}

bool
ircd::m::federate(const id::room &room_id)
{
	static const m::event::fetch::opts fopts
	{
		event::keys::include
		{
			"content",
		}
	};

	const m::room::state state
	{
		room_id, &fopts
	};

	bool ret;
	state.get("m.room.create", "", [&ret]
	(const m::event &event)
	{
		ret = json::get<"content"_>(event).get("m.federate", true);
	});

	return ret;
}

bool
ircd::m::exists(const id::room &room_id)
{
	const auto it
	{
		dbs::room_events.begin(room_id)
	};

	return bool(it);
}

bool
ircd::m::exists(const room &room)
{
	return exists(room.room_id);
}

bool
ircd::m::operator==(const room &a, const room &b)
{
	return !(a != b);
}

bool
ircd::m::operator!=(const room &a, const room &b)
{
	return a.room_id != b.room_id;
}

bool
ircd::m::operator!(const room &a)
{
	return !a.room_id;
}

bool
ircd::m::my(const room &room)
{
	return my(room.room_id);
}

//
// room
//

/// A room index is just the event::idx of its create event.
ircd::m::event::idx
ircd::m::room::index(const room::id &room_id)
{
	const auto ret
	{
		index(room_id, std::nothrow)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"No index for room %s", string_view{room_id}
		};

	return ret;
}

ircd::m::event::idx
ircd::m::room::index(const room::id &room_id,
                     std::nothrow_t)
{
	uint64_t depth{0};
	room::messages it
	{
		room_id, depth
	};

	return it? it.event_idx() : 0;
}

//
// room::room
//

ircd::m::id::user::buf
ircd::m::room::any_user(const string_view &host,
                        const string_view &membership)
const
{
	user::id::buf ret;
	const members members{*this};
	members.for_each(membership, members::closure_bool{[&host, &ret]
	(const id::user &user_id)
	{
		if(host && user_id.host() != host)
			return true;

		ret = user_id;
		return false;
	}});

	return ret;
}

/// Test of the join_rule of the room is the argument.
bool
ircd::m::room::join_rule(const string_view &rule)
const
{
	char buf[32];
	return join_rule(mutable_buffer{buf}) == rule;
}

/// Receive the join_rule of the room into buffer of sufficient size.
/// The protocol does not specify a join_rule string longer than 7
/// characters but do be considerate of the future. This function
/// properly defaults the string as per the protocol spec.
ircd::string_view
ircd::m::room::join_rule(const mutable_buffer &out)
const
{
	static const string_view default_join_rule
	{
		"invite"
	};

	string_view ret
	{
		default_join_rule
	};

	const event::keys::include keys
	{
		"content"
	};

	const m::event::fetch::opts fopts
	{
		keys, this->fopts? this->fopts->gopts : db::gopts{}
	};

	const room::state state
	{
		*this, &fopts
	};

	state.get(std::nothrow, "m.room.join_rules", "", [&ret, &out]
	(const m::event &event)
	{
		const auto &content
		{
			json::get<"content"_>(event)
		};

		const string_view &rule
		{
			content.get("join_rule", default_join_rule)
		};

		ret = string_view
		{
			data(out), copy(out, unquote(rule))
		};
	});

	return ret;
}

/// The only joined members are from our origin (local only). This indicates
/// we won't have any other federation servers to query for room data, nor do
/// we need to broadcast events to the federation. This is not an authority
/// about a room's type or ability to federate. Returned value changes to false
/// when another origin joins.
bool
ircd::m::room::lonly()
const
{
	const origins origins(*this);
	return origins.only(my_host());
}

bool
ircd::m::room::visible(const string_view &mxid,
                       const event *const &event)
const
{
	if(event)
		return m::visible(*event, mxid);

	const m::event event_
	{
		{ "event_id",  event_id  },
		{ "room_id",   room_id   },
	};

	return m::visible(event_, mxid);
}

bool
ircd::m::room::membership(const m::id::user &user_id,
                          const string_view &membership)
const
{
	char buf[64];
	return this->membership(buf, user_id) == membership;
}

ircd::string_view
ircd::m::room::membership(const mutable_buffer &out,
                          const m::id::user &user_id)
const
{
	string_view ret;
	const room::state state{*this};
	state.get(std::nothrow, "m.room.member", user_id, [&out, &ret]
	(const event::idx &event_idx)
	{
		m::get(std::nothrow, event_idx, "content", [&out, &ret]
		(const json::object &content)
		{
			ret =
			{
				data(out), copy(out, unquote(content.get("membership")))
			};
		});
	});

	return ret;
}

bool
ircd::m::room::has(const string_view &type)
const
{
	return get(std::nothrow, type, event::closure{});
}

void
ircd::m::room::get(const string_view &type,
                   const event::closure &closure)
const
{
	if(!get(std::nothrow, type, closure))
		throw m::NOT_FOUND
		{
			"No events of type '%s' found in '%s'",
			type,
			room_id
		};
}

bool
ircd::m::room::get(std::nothrow_t,
                   const string_view &type,
                   const event::closure &closure)
const
{
	bool ret{false};
	for_each(type, event::closure_bool{[&ret, &closure]
	(const event &event)
	{
		if(closure)
			closure(event);

		ret = true;
		return false;
	}});

	return ret;
}

ircd::m::event::idx
ircd::m::room::get(const string_view &type,
                   const string_view &state_key)
const
{
	return state(*this).get(type, state_key);
}

ircd::m::event::idx
ircd::m::room::get(std::nothrow_t,
                   const string_view &type,
                   const string_view &state_key)
const
{
	return state(*this).get(std::nothrow, type, state_key);
}

void
ircd::m::room::get(const string_view &type,
                   const string_view &state_key,
                   const event::closure &closure)
const
{
	const state state{*this};
	state.get(type, state_key, closure);
}

bool
ircd::m::room::get(std::nothrow_t,
                   const string_view &type,
                   const string_view &state_key,
                   const event::closure &closure)
const
{
	const state state{*this};
	return state.get(std::nothrow, type, state_key, closure);
}

bool
ircd::m::room::has(const string_view &type,
                   const string_view &state_key)
const
{
	const state state{*this};
	return state.has(type, state_key);
}

void
ircd::m::room::for_each(const event::closure &closure)
const
{
	for_each(string_view{}, closure);
}

bool
ircd::m::room::for_each(const event::closure_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

void
ircd::m::room::for_each(const event::id::closure &closure)
const
{
	for_each(string_view{}, closure);
}

bool
ircd::m::room::for_each(const event::id::closure_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

void
ircd::m::room::for_each(const event::closure_idx &closure)
const
{
	for_each(string_view{}, closure);
}

bool
ircd::m::room::for_each(const event::closure_idx_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

void
ircd::m::room::for_each(const string_view &type,
                        const event::closure &closure)
const
{
	for_each(type, event::closure_bool{[&closure]
	(const event &event)
	{
		closure(event);
		return true;
	}});
}

bool
ircd::m::room::for_each(const string_view &type,
                        const event::closure_bool &closure)
const
{
	event::fetch event
	{
		fopts? *fopts : event::fetch::default_opts
	};

	return for_each(type, event::closure_idx_bool{[&closure, &event]
	(const event::idx &event_idx)
	{
		if(!seek(event, event_idx, std::nothrow))
			return true;

		return closure(event);
	}});
}

void
ircd::m::room::for_each(const string_view &type,
                        const event::id::closure &closure)
const
{
	for_each(type, event::id::closure_bool{[&closure]
	(const event::id &event_id)
	{
		closure(event_id);
		return true;
	}});
}

bool
ircd::m::room::for_each(const string_view &type,
                        const event::id::closure_bool &closure)
const
{
	return for_each(type, event::closure_idx_bool{[&closure]
	(const event::idx &idx)
	{
		bool ret{true};
		event::fetch::event_id(idx, std::nothrow, [&ret, &closure]
		(const event::id &event_id)
		{
			ret = closure(event_id);
		});

		return ret;
	}});
}

void
ircd::m::room::for_each(const string_view &type,
                        const event::closure_idx &closure)
const
{
	for_each(type, event::closure_idx_bool{[&closure]
	(const event::idx &idx)
	{
		closure(idx);
		return true;
	}});
}

bool
ircd::m::room::for_each(const string_view &type,
                        const event::closure_idx_bool &closure)
const
{
	static constexpr auto idx
	{
		json::indexof<event, "type"_>()
	};

	auto &column
	{
		dbs::event_column.at(idx)
	};

	messages it{*this};
	for(; it; --it)
	{
		const auto &event_idx
		{
			it.event_idx()
		};

		bool match
		{
			empty(type) // allow empty type to always match and bypass query
		};

		if(!match)
			column(byte_view<string_view>(event_idx), std::nothrow, [&match, &type]
			(const string_view &value)
			{
				match = value == type;
			});

		if(match)
			if(!closure(event_idx))
				return false;
	}

	return true;
}

//
// room::messages
//

ircd::m::room::messages::messages(const m::room &room,
                                  const event::fetch::opts *const &fopts)
:room{room}
,_event
{
	fopts?
		*fopts:
	room.fopts?
		*room.fopts:
		event::fetch::default_opts
}
{
	if(room.event_id)
		seek(room.event_id);
	else
		seek();
}

ircd::m::room::messages::messages(const m::room &room,
                                  const event::id &event_id,
                                  const event::fetch::opts *const &fopts)
:room{room}
,_event
{
	fopts?
		*fopts:
	room.fopts?
		*room.fopts:
		event::fetch::default_opts
}
{
	seek(event_id);
}

ircd::m::room::messages::messages(const m::room &room,
                                  const uint64_t &depth,
                                  const event::fetch::opts *const &fopts)
:room{room}
,_event
{
	fopts?
		*fopts:
	room.fopts?
		*room.fopts:
		event::fetch::default_opts
}
{
	// As a special convenience for the ctor only, if the depth=0 and
	// nothing is found another attempt is made for depth=1 for synapse
	// rooms which start at depth=1.
	if(!seek(depth) && depth == 0)
		seek(1);
}

const ircd::m::event &
ircd::m::room::messages::operator*()
{
	return fetch(std::nothrow);
};

bool
ircd::m::room::messages::seek(const event::id &event_id)
{
	const event::idx &event_idx
	{
		m::index(event_id, std::nothrow)
	};

	return event_idx?
		seek_idx(event_idx):
		false;
}

bool
ircd::m::room::messages::seek(const uint64_t &depth)
{
	char buf[dbs::ROOM_EVENTS_KEY_MAX_SIZE];
	const string_view seek_key
	{
		depth != uint64_t(-1)?
			dbs::room_events_key(buf, room.room_id, depth):
			room.room_id
	};

	this->it = dbs::room_events.begin(seek_key);
	return bool(*this);
}

bool
ircd::m::room::messages::seek_idx(const event::idx &event_idx)
try
{
	uint64_t depth(0);
	if(event_idx)
		m::get(event_idx, "depth", mutable_buffer
		{
			reinterpret_cast<char *>(&depth), sizeof(depth)
		});

	char buf[dbs::ROOM_EVENTS_KEY_MAX_SIZE];
	const auto &seek_key
	{
		dbs::room_events_key(buf, room.room_id, depth, event_idx)
	};

	this->it = dbs::room_events.begin(seek_key);
	if(!bool(*this))
		return false;

	// Check if this event_idx is actually in this room
	if(event_idx && event_idx != this->event_idx())
		return false;

	return true;
}
catch(const db::not_found &e)
{
	return false;
}

ircd::m::event::id::buf
ircd::m::room::messages::event_id()
const
{
	event::id::buf ret;
	event::fetch::event_id(this->event_idx(), [&ret]
	(const event::id &event_id)
	{
		ret = event_id;
	});

	return ret;
}

ircd::string_view
ircd::m::room::messages::state_root()
const
{
	assert(bool(*this));
	return it->second;
}

uint64_t
ircd::m::room::messages::depth()
const
{
	assert(bool(*this));
	const auto part
	{
		dbs::room_events_key(it->first)
	};

	return std::get<0>(part);
}

ircd::m::event::idx
ircd::m::room::messages::event_idx()
const
{
	assert(bool(*this));
	const auto part
	{
		dbs::room_events_key(it->first)
	};

	return std::get<1>(part);
}

const ircd::m::event &
ircd::m::room::messages::fetch()
{
	m::seek(_event, event_idx());
	return _event;
}

const ircd::m::event &
ircd::m::room::messages::fetch(std::nothrow_t)
{
	m::seek(_event, event_idx(), std::nothrow);
	return _event;
}

//
// room::state
//

decltype(ircd::m::room::state::disable_history)
ircd::m::room::state::disable_history
{
	{ "name",     "ircd.m.room.state.disable_history" },
	{ "default",  true                                },
};

decltype(ircd::m::room::state::readahead_size)
ircd::m::room::state::readahead_size
{
	{ "name",     "ircd.m.room.state.readahead_size" },
	{ "default",  0L                                 },
};

//
// room::state::state
//

ircd::m::room::state::state(const m::room &room,
                            const event::fetch::opts *const &fopts)
:room_id
{
	room.room_id
}
,event_id
{
	room.event_id?
		event::id::buf{room.event_id}:
		event::id::buf{}
}
,root_id
{
	event_id && !bool(disable_history)?
		dbs::state_root(root_id_buf, room_id, event_id):
		m::state::id{}
}
,fopts
{
	fopts?
		fopts:
		room.fopts
}
{
}

size_t
ircd::m::room::state::prefetch(const event::idx &start,
                               const event::idx &stop)
const
{
	return prefetch(string_view{}, start, stop);
}

size_t
ircd::m::room::state::prefetch(const string_view &type,
                               const event::idx &start,
                               const event::idx &stop)
const
{
	return prefetch(*this, type, event::idx_range{start, stop});
}

ircd::m::event::idx
ircd::m::room::state::get(const string_view &type,
                          const string_view &state_key)
const
{
	event::idx ret;
	get(type, state_key, event::closure_idx{[&ret]
	(const event::idx &event_idx)
	{
		ret = event_idx;
	}});

	return ret;
}

ircd::m::event::idx
ircd::m::room::state::get(std::nothrow_t,
                          const string_view &type,
                          const string_view &state_key)
const
{
	event::idx ret{0};
	get(std::nothrow, type, state_key, event::closure_idx{[&ret]
	(const event::idx &event_idx)
	{
		ret = event_idx;
	}});

	return ret;
}

void
ircd::m::room::state::get(const string_view &type,
                          const string_view &state_key,
                          const event::closure &closure)
const
{
	get(type, state_key, event::closure_idx{[this, &closure]
	(const event::idx &event_idx)
	{
		const event::fetch event
		{
			event_idx, fopts? *fopts : event::fetch::default_opts
		};

		closure(event);
	}});
}

void
ircd::m::room::state::get(const string_view &type,
                          const string_view &state_key,
                          const event::id::closure &closure)
const try
{
	if(!present())
		return m::state::get(root_id, type, state_key, [&closure]
		(const string_view &event_id)
		{
			closure(unquote(event_id));
		});

	get(type, state_key, event::closure_idx{[&closure]
	(const event::idx &idx)
	{
		event::fetch::event_id(idx, closure);
	}});
}
catch(const db::not_found &e)
{
	throw m::NOT_FOUND
	{
		"(%s,%s) in %s :%s",
		type,
		state_key,
		string_view{room_id},
		e.what()
	};
}

void
ircd::m::room::state::get(const string_view &type,
                          const string_view &state_key,
                          const event::closure_idx &closure)
const try
{
	if(!present())
		m::state::get(root_id, type, state_key, [&closure]
		(const string_view &event_id)
		{
			closure(index(m::event::id(unquote(event_id))));
		});

	auto &column{dbs::room_state};
	char key[dbs::ROOM_STATE_KEY_MAX_SIZE];
	column(dbs::room_state_key(key, room_id, type, state_key), [&closure]
	(const string_view &value)
	{
		closure(byte_view<event::idx>(value));
	});
}
catch(const db::not_found &e)
{
	throw m::NOT_FOUND
	{
		"(%s,%s) in %s :%s",
		type,
		state_key,
		string_view{room_id},
		e.what()
	};
}

bool
ircd::m::room::state::get(std::nothrow_t,
                          const string_view &type,
                          const string_view &state_key,
                          const event::closure &closure)
const
{
	return get(std::nothrow, type, state_key, event::closure_idx{[this, &closure]
	(const event::idx &event_idx)
	{
		const event::fetch event
		{
			event_idx, std::nothrow, fopts? *fopts : event::fetch::default_opts
		};

		closure(event);
	}});
}

bool
ircd::m::room::state::get(std::nothrow_t,
                          const string_view &type,
                          const string_view &state_key,
                          const event::id::closure &closure)
const
{
	if(!present())
		return m::state::get(std::nothrow, root_id, type, state_key, [&closure]
		(const string_view &event_id)
		{
			closure(unquote(event_id));
		});

	return get(std::nothrow, type, state_key, event::closure_idx{[&closure]
	(const event::idx &idx)
	{
		event::fetch::event_id(idx, std::nothrow, closure);
	}});
}

bool
ircd::m::room::state::get(std::nothrow_t,
                          const string_view &type,
                          const string_view &state_key,
                          const event::closure_idx &closure)
const
{
	if(!present())
		return m::state::get(std::nothrow, root_id, type, state_key, [&closure]
		(const string_view &event_id)
		{
			return closure(index(m::event::id(unquote(event_id)), std::nothrow));
		});

	auto &column{dbs::room_state};
	char key[dbs::ROOM_STATE_KEY_MAX_SIZE];
	return column(dbs::room_state_key(key, room_id, type, state_key), std::nothrow, [&closure]
	(const string_view &value)
	{
		closure(byte_view<event::idx>(value));
	});
}

bool
ircd::m::room::state::has(const string_view &type)
const
{
	return for_each(type, event::id::closure_bool{[](const m::event::id &)
	{
		return true;
	}});
}

bool
ircd::m::room::state::has(const string_view &type,
                          const string_view &state_key)
const
{
	if(!present())
		return m::state::get(std::nothrow, root_id, type, state_key, []
		(const string_view &event_id)
		{});

	auto &column{dbs::room_state};
	char key[dbs::ROOM_STATE_KEY_MAX_SIZE];
	return db::has(column, dbs::room_state_key(key, room_id, type, state_key));
}

size_t
ircd::m::room::state::count()
const
{
	return count(string_view{});
}

size_t
ircd::m::room::state::count(const string_view &type)
const
{
	if(!present())
		return m::state::count(root_id, type);

	size_t ret{0};
	for_each(type, event::closure_idx{[&ret]
	(const event::idx &event_idx)
	{
		++ret;
	}});

	return ret;
}

void
ircd::m::room::state::for_each(const event::closure &closure)
const
{
	for_each(event::closure_bool{[&closure]
	(const m::event &event)
	{
		closure(event);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const event::closure_bool &closure)
const
{
	event::fetch event
	{
		fopts? *fopts : event::fetch::default_opts
	};

	return for_each(event::closure_idx_bool{[&event, &closure]
	(const event::idx &event_idx)
	{
		if(seek(event, event_idx, std::nothrow))
			if(!closure(event))
				return false;

		return true;
	}});
}

void
ircd::m::room::state::for_each(const event::id::closure &closure)
const
{
	for_each(event::id::closure_bool{[&closure]
	(const event::id &event_id)
	{
		closure(event_id);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const event::id::closure_bool &closure)
const
{
	if(!present())
		return m::state::for_each(root_id, m::state::iter_bool_closure{[&closure]
		(const json::array &key, const string_view &event_id)
		{
			return closure(unquote(event_id));
		}});

	return for_each(event::closure_idx_bool{[&closure]
	(const event::idx &idx)
	{
		bool ret{true};
		event::fetch::event_id(idx, std::nothrow, [&ret, &closure]
		(const event::id &id)
		{
			ret = closure(id);
		});

		return ret;
	}});
}

void
ircd::m::room::state::for_each(const event::closure_idx &closure)
const
{
	for_each(event::closure_idx_bool{[&closure]
	(const event::idx &event_idx)
	{
		closure(event_idx);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const event::closure_idx_bool &closure)
const
{
	return for_each(closure_bool{[&closure]
	(const string_view &type, const string_view &state_key, const event::idx &event_idx)
	{
		return closure(event_idx);
	}});
}

bool
ircd::m::room::state::for_each(const closure_bool &closure)
const
{
	if(!present())
		return m::state::for_each(root_id, m::state::iter_bool_closure{[&closure]
		(const json::array &key, const string_view &event_id)
		{
			const auto idx(index(m::event::id(unquote(event_id)), std::nothrow));
			return closure(key.at(0), key.at(1), idx);
		}});

	db::gopts opts
	{
		this->fopts? this->fopts->gopts : db::gopts{}
	};

	if(!opts.readahead)
		opts.readahead = size_t(readahead_size);

	auto &column{dbs::room_state};
	for(auto it{column.begin(room_id, opts)}; bool(it); ++it)
	{
		const byte_view<event::idx> idx(it->second);
		const auto key(dbs::room_state_key(it->first));
		if(!closure(std::get<0>(key), std::get<1>(key), idx))
			return false;
	}

	return true;
}

void
ircd::m::room::state::for_each(const string_view &type,
                               const event::closure &closure)
const
{
	for_each(type, event::closure_bool{[&closure]
	(const m::event &event)
	{
		closure(event);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const event::closure_bool &closure)
const
{
	return type?
		for_each(type, string_view{}, closure):
		for_each(closure);
}

void
ircd::m::room::state::for_each(const string_view &type,
                               const event::id::closure &closure)
const
{
	for_each(type, event::id::closure_bool{[&closure]
	(const event::id &event_id)
	{
		closure(event_id);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const event::id::closure_bool &closure)
const
{
	return type?
		for_each(type, string_view{}, closure):
		for_each(closure);
}

void
ircd::m::room::state::for_each(const string_view &type,
                               const event::closure_idx &closure)
const
{
	for_each(type, event::closure_idx_bool{[&closure]
	(const event::idx &event_idx)
	{
		closure(event_idx);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const event::closure_idx_bool &closure)
const
{
	return type?
		for_each(type, string_view{}, closure):
		for_each(closure);
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const closure_bool &closure)
const
{
	return type?
		for_each(type, string_view{}, closure):
		for_each(closure);
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const string_view &state_key_lb,
                               const event::closure_bool &closure)
const
{
	event::fetch event
	{
		fopts? *fopts : event::fetch::default_opts
	};

	return for_each(type, state_key_lb, event::closure_idx_bool{[&event, &closure]
	(const event::idx &event_idx)
	{
		if(seek(event, event_idx, std::nothrow))
			if(!closure(event))
				return false;

		return true;
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const string_view &state_key_lb,
                               const event::id::closure_bool &closure)
const
{
	if(!present())
		return m::state::for_each(root_id, type, state_key_lb, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			return closure(unquote(event_id));
		});

	return for_each(type, state_key_lb, event::closure_idx_bool{[&closure]
	(const event::idx &idx)
	{
		bool ret{true};
		event::fetch::event_id(idx, std::nothrow, [&ret, &closure]
		(const event::id &id)
		{
			ret = closure(id);
		});

		return ret;
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const string_view &state_key_lb,
                               const event::closure_idx_bool &closure)
const
{
	return for_each(type, state_key_lb, closure_bool{[&closure]
	(const string_view &type, const string_view &state_key, const event::idx &event_idx)
	{
		return closure(event_idx);
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const string_view &state_key_lb,
                               const closure_bool &closure)
const
{
	if(!present())
		return m::state::for_each(root_id, type, state_key_lb, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			const auto idx(index(m::event::id(unquote(event_id)), std::nothrow));
			return closure(key.at(0), key.at(1), idx);
		});

	char keybuf[dbs::ROOM_STATE_KEY_MAX_SIZE];
	const auto &key
	{
		dbs::room_state_key(keybuf, room_id, type, state_key_lb)
	};

	db::gopts opts
	{
		this->fopts? this->fopts->gopts : db::gopts{}
	};

	if(!opts.readahead)
		opts.readahead = size_t(readahead_size);

	auto &column{dbs::room_state};
	for(auto it{column.begin(key, opts)}; bool(it); ++it)
	{
		const auto key
		{
			dbs::room_state_key(it->first)
		};

		if(std::get<0>(key) != type)
			break;

		const byte_view<event::idx> idx(it->second);
		if(!closure(std::get<0>(key), std::get<1>(key), idx))
			return false;
	}

	return true;
}

void
ircd::m::room::state::for_each(const string_view &type,
                               const keys &closure)
const
{
	for_each(type, keys_bool{[&closure]
	(const string_view &key)
	{
		closure(key);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const keys_bool &closure)
const
{
	return for_each(type, string_view{}, closure);
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const string_view &state_key_lb,
                               const keys_bool &closure)
const
{
	if(!present())
		return m::state::for_each(root_id, type, state_key_lb, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			assert(size(key) >= 2);
			return closure(unquote(key.at(1)));
		});

	char keybuf[dbs::ROOM_STATE_KEY_MAX_SIZE];
	const auto &key
	{
		dbs::room_state_key(keybuf, room_id, type, state_key_lb)
	};

	db::gopts opts
	{
		this->fopts? this->fopts->gopts : db::gopts{}
	};

	if(!opts.readahead)
		opts.readahead = size_t(readahead_size);

	auto &column{dbs::room_state};
	for(auto it{column.begin(key, opts)}; bool(it); ++it)
	{
		const auto key
		{
			dbs::room_state_key(it->first)
		};

		if(std::get<0>(key) != type)
			break;

		if(!closure(std::get<1>(key)))
			return false;
	}

	return true;
}

bool
ircd::m::room::state::for_each(const type_prefix &prefix,
                               const types_bool &closure)
const
{
	bool ret(true), cont(true);
	for_each(types_bool([&prefix, &closure, &ret, &cont]
	(const string_view &type)
	{
		if(!startswith(type, string_view(prefix)))
			return cont;

		cont = false;
		ret = closure(type);
		return ret;
	}));

	return ret;
}

void
ircd::m::room::state::for_each(const types &closure)
const
{
	for_each(types_bool{[&closure]
	(const string_view &type)
	{
		closure(type);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const types_bool &closure)
const
{
	string_view last;
	char lastbuf[m::event::TYPE_MAX_SIZE];
	if(!present())
	{
		m::state::for_each(root_id, m::state::iter_bool_closure{[&closure, &last, &lastbuf]
		(const json::array &key, const string_view &)
		{
			assert(size(key) >= 2);
			const auto type
			{
				unquote(key.at(0))
			};

			if(type == last)
				return true;

			last = strlcpy(lastbuf, type);
			return closure(type);
		}});

		return true;
	}

	char keybuf[dbs::ROOM_STATE_KEY_MAX_SIZE];
	const auto &key
	{
		dbs::room_state_key(keybuf, room_id, string_view{})
	};

	db::gopts opts
	{
		this->fopts? this->fopts->gopts : db::gopts{}
	};

	auto &column{dbs::room_state};
	for(auto it{column.begin(key, opts)}; bool(it); ++it)
	{
		const auto part
		{
			dbs::room_state_key(it->first)
		};

		const auto &type
		{
			std::get<0>(part)
		};

		if(type == last)
			continue;

		last = strlcpy(lastbuf, type);
		if(!closure(type))
			return false;
	}

	return true;
}

/// Figure out if this instance of room::state is presenting the current
/// "present" state of the room or the state of the room at some previous
/// event. This is an important distinction because the present state of
/// the room should provide optimal performance for the functions of this
/// interface by using the present state table. Prior states will use the
/// state btree.
bool
ircd::m::room::state::present()
const
{
	// When no event_id is passed to the state constructor that immediately
	// indicates the present state of the room is sought.
	if(!event_id)
		return true;

	// When an event_id is passed but no state btree root_id was found
	// that means there is no state btree generated for this room or at
	// this event_id. Right now we fall back to using the present state
	// of the room, which may be confusing behavior. In production this
	// shouldn't happen; if it does, the ctor should probably throw rather
	// than this. We just fallback to considering present state here.
	if(!root_id)
		return true;

	// Check the cached value from a previous false result of this function
	// before doing any real work/IO below. If this function ever returned
	// false it will never return true after.
	if(_not_present)
		return false;

	const auto head_id
	{
		m::head(std::nothrow, room_id)
	};

	// If the event_id passed is exactly the latest event we can obviously
	// consider this the present state.
	if(!head_id || head_id == event_id)
		return true;

	m::state::id_buffer root_id_buf;
	const auto head_root
	{
		dbs::state_root(root_id_buf, room_id, head_id)
	};

	// If the room's state has not changed between the event_id desired
	// here and the latest event we can also consider this the present state.
	if(head_root && head_root == root_id)
		return true;

	// This result is cacheable because once it's no longer the present
	// it will never be again. Panta chorei kai ouden menei. Note that this
	// is a const member function; the cache variable is an appropriate case
	// for the 'mutable' keyword.
	_not_present = true;
	return false;
}

//
// room::members
//

size_t
ircd::m::room::members::count()
const
{
	const room::state state
	{
		room
	};

	return state.count("m.room.member");
}

size_t
ircd::m::room::members::count(const string_view &membership)
const
{
	// Allow empty membership string to count all memberships
	if(!membership)
		return count();

	// joined members optimization. Only possible when seeking
	// membership="join" on the present state of the room.
	if(!room.event_id && membership == "join")
	{
		size_t ret{0};
		const room::origins origins{room};
		origins._for_each_([&ret](const string_view &)
		{
			++ret;
			return true;
		});

		return ret;
	}

	// The list of event fields to fetch for the closure
	static const event::keys::include keys
	{
		"event_id",    // Added for any upstack usage (but may be unnecessary).
		"content",     // Required because synapse events randomly have no event.membership
	};

	const m::event::fetch::opts fopts
	{
		keys, room.fopts? room.fopts->gopts : db::gopts{}
	};

	const room::state state
	{
		room, &fopts
	};

	size_t ret{0};
	state.for_each("m.room.member", event::closure{[&ret, &membership]
	(const m::event &event)
	{
		ret += m::membership(event) == membership;
	}});

	return ret;
}

void
ircd::m::room::members::for_each(const closure &closure)
const
{
	for_each(string_view{}, closure);
}

bool
ircd::m::room::members::for_each(const closure_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

void
ircd::m::room::members::for_each(const event::closure &closure)
const
{
	for_each(string_view{}, closure);
}

bool
ircd::m::room::members::for_each(const event::closure_bool &closure)
const
{
	const room::state state{room};
	return state.for_each("m.room.member", event::closure_bool{[&closure]
	(const m::event &event)
	{
		return closure(event);
	}});
}

void
ircd::m::room::members::for_each(const string_view &membership,
                                 const closure &closure)
const
{
	for_each(membership, closure_bool{[&closure]
	(const user::id &user_id)
	{
		closure(user_id);
		return true;
	}});
}

/// Iterate the mxid's of the users in the room, optionally with a specific
/// membership state. This query contains internal optimizations as the closure
/// only requires a user::id. The db::gopts set in the room.fopts pointer is
/// still used if provided.
bool
ircd::m::room::members::for_each(const string_view &membership,
                                 const closure_bool &closure)
const
{
	// Setup the list of event fields to fetch for the closure
	static const event::keys::include keys
	{
		"event_id",
		"state_key",
		"content",
	};

	// In this case the fetch opts isn't static so it can maintain the
	// previously given db::gopts, but it will use our keys list.
	const m::event::fetch::opts fopts
	{
		keys, room.fopts? room.fopts->gopts : db::gopts{}
	};

	// Stack-over the the current fetch opts with our new opts for this query,
	// putting them back when we're finished. This requires a const_cast which
	// should be okay here.
	auto &room(const_cast<m::room &>(this->room));
	const auto theirs{room.fopts};
	room.fopts = &fopts;
	const unwind reset{[&room, &theirs]
	{
		room.fopts = theirs;
	}};

	return for_each(membership, event::closure_bool{[&closure]
	(const event &event)
	{
		const user::id &user_id
		{
			at<"state_key"_>(event)
		};

		return closure(user_id);
	}});
}

void
ircd::m::room::members::for_each(const string_view &membership,
                                 const event::closure &closure)
const
{
	for_each(membership, event::closure_bool{[&closure]
	(const m::event &event)
	{
		closure(event);
		return true;
	}});
}

bool
ircd::m::room::members::for_each(const string_view &membership,
                                 const event::closure_bool &closure)
const
{
	if(empty(membership))
		return for_each(closure);

	// joined members optimization. Only possible when seeking
	// membership="join" on the present state of the room.
	if(!room.event_id && membership == "join")
	{
		const room::origins origins{room};
		return origins._for_each_([&closure, this]
		(const string_view &key)
		{
			const string_view &member
			{
				std::get<1>(dbs::room_joined_key(key))
			};

			bool ret{true};
			room.get(std::nothrow, "m.room.member", member, event::closure{[&closure, &ret]
			(const event &event)
			{
				ret = closure(event);
			}});

			return ret;
		});
	}

	return for_each(event::closure_bool{[&membership, &closure]
	(const event &event)
	{
		if(m::membership(event) == membership)
			if(!closure(event))
				return false;

		return true;
	}});
}

//
// room::origins
//

ircd::string_view
ircd::m::room::origins::random(const mutable_buffer &buf,
                               const closure_bool &proffer)
const
{
	string_view ret;
	const auto closure{[&buf, &proffer, &ret]
	(const string_view &origin)
	{
		ret = { data(buf), copy(buf, origin) };
	}};

	random(closure, proffer);
	return ret;
}

bool
ircd::m::room::origins::random(const closure &view,
                               const closure_bool &proffer)
const
{
	return random(*this, view, proffer);
}

bool
ircd::m::room::origins::random(const origins &o,
                               const closure &view,
                               const closure_bool &proffer)
{
	using prototype = bool (const origins &, const closure &, const closure_bool &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::origins::random"
	};

	return call(o, view, proffer);
}

size_t
ircd::m::room::origins::count()
const
{
	size_t ret{0};
	for_each([&ret](const string_view &)
	{
		++ret;
	});

	return ret;
}

/// Tests if argument is the only origin in the room.
/// If a zero or more than one origins exist, returns false. If the only origin
/// in the room is the argument origin, returns true.
bool
ircd::m::room::origins::only(const string_view &origin)
const
{
	ushort ret{2};
	for_each(closure_bool{[&ret, &origin]
	(const string_view &origin_) -> bool
	{
		if(origin == origin_)
			ret = 1;
		else
			ret = 0;

		return ret;
	}});

	return ret == 1;
}

bool
ircd::m::room::origins::has(const string_view &origin)
const
{
	db::index &index
	{
		dbs::room_joined
	};

	char querybuf[dbs::ROOM_JOINED_KEY_MAX_SIZE];
	const auto query
	{
		dbs::room_joined_key(querybuf, room.room_id, origin)
	};

	auto it
	{
		index.begin(query)
	};

	if(!it)
		return false;

	const string_view &key
	{
		lstrip(it->first, "\0"_sv)
	};

	const string_view &key_origin
	{
		std::get<0>(dbs::room_joined_key(key))
	};

	return key_origin == origin;
}

void
ircd::m::room::origins::for_each(const closure &view)
const
{
	for_each(closure_bool{[&view]
	(const string_view &origin)
	{
		view(origin);
		return true;
	}});
}

bool
ircd::m::room::origins::for_each(const closure_bool &view)
const
{
	string_view last;
	char lastbuf[rfc1035::NAME_BUF_SIZE];
	return _for_each_([&last, &lastbuf, &view]
	(const string_view &key)
	{
		const string_view &origin
		{
			std::get<0>(dbs::room_joined_key(key))
		};

		if(origin == last)
			return true;

		if(!view(origin))
			return false;

		last = { lastbuf, copy(lastbuf, origin) };
		return true;
	});
}

bool
ircd::m::room::origins::_for_each_(const closure_bool &view)
const
{
	db::index &index
	{
		dbs::room_joined
	};

	auto it
	{
		index.begin(room.room_id)
	};

	for(; bool(it); ++it)
	{
		const string_view &key
		{
			lstrip(it->first, "\0"_sv)
		};

		if(!view(key))
			return false;
	}

	return true;
}

//
// room::head
//

std::pair<ircd::json::array, int64_t>
ircd::m::room::head::make_refs(const mutable_buffer &buf,
                               const size_t &limit,
                               const bool &need_top)
const
{
	json::stack out{buf};
	json::stack::array array{out};
	const auto depth
	{
		make_refs(array, limit, need_top)
	};
	array.~array();
	return
	{
		json::array{out.completed()},
		depth
	};
}

int64_t
ircd::m::room::head::make_refs(json::stack::array &out,
                               const size_t &limit,
                               const bool &need_top)
const
{
	return make_refs(*this, out, limit, need_top);
}

size_t
ircd::m::room::head::count()
const
{
	size_t ret(0);
	for_each([&ret]
	(const event::idx &event_idx, const event::id &event_id)
	{
		++ret;
	});

	return ret;
}

bool
ircd::m::room::head::has(const event::id &event_id)
const
{
	bool ret{false};
	for_each(closure_bool{[&ret, &event_id]
	(const event::idx &event_idx, const event::id &event_id_)
	{
		ret = event_id_ == event_id;
		return !ret; // for_each protocol: false to break
	}});

	return ret;
}

void
ircd::m::room::head::for_each(const closure &closure)
const
{
	for_each(closure_bool{[&closure]
	(const event::idx &event_idx, const event::id &event_id)
	{
		closure(event_idx, event_id);
		return true;
	}});
}

bool
ircd::m::room::head::for_each(const closure_bool &closure)
const
{
	return for_each(*this, closure);
}

bool
ircd::m::room::head::for_each(const head &h,
                              const closure_bool &c)
{
	using prototype = bool (const head &, const closure_bool &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::head::for_each"
	};

	return call(h, c);
}

int64_t
ircd::m::room::head::make_refs(const head &h,
                               json::stack::array &a,
                               const size_t &limit,
                               const bool &need_top)
{
	using prototype = int64_t (const head &, json::stack::array &, const size_t &, const bool &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::head::make_refs"
	};

	return call(h, a, limit, need_top);
}

void
ircd::m::room::head::modify(const event::id &event_id,
                            const db::op &op,
                            const bool &refs)
{
	using prototype = void (const event::id &, const db::op &, const bool &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::head::modify"
	};

	return call(event_id, op, refs);
}

size_t
ircd::m::room::head::rebuild(const head &h)
{
	using prototype = size_t (const head &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::head::rebuild"
	};

	return call(h);
}

size_t
ircd::m::room::head::reset(const head &h)
{
	using prototype = size_t (const head &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::head::reset"
	};

	return call(h);
}

//
// room::auth
//

void
ircd::m::room::auth::chain_eval(const auth &a,
                                const net::hostport &h)
{
	using prototype = bool (const auth &, const net::hostport &);

	static mods::import<prototype> call
	{
		"s_fetch", "ircd::m::room::auth::chain_eval"
	};

	call(a, h);
}

bool
ircd::m::room::auth::chain_fetch(const auth &a,
                                 const net::hostport &h,
                                 const fetch_closure &c)
{
	using prototype = bool (const auth &, const net::hostport &, const fetch_closure &);

	static mods::import<prototype> call
	{
		"s_fetch", "ircd::m::room::auth::chain_fetch"
	};

	return call(a, h, c);
}

ircd::json::array
ircd::m::room::auth::make_refs(const mutable_buffer &buf,
                               const types &types,
                               const m::id::user &user)
const
{
	json::stack out{buf};
	json::stack::array array{out};
	make_refs(array, types, user);
	array.~array();
	return json::array{out.completed()};
}

void
ircd::m::room::auth::make_refs(json::stack::array &out,
                               const types &types,
                               const m::id::user &user)
const
{
	make_refs(*this, out, types, user);
}

void
ircd::m::room::auth::for_each(const closure &c)
const
{
	for_each(closure_bool{[this, &c]
	(const auto &event_idx)
	{
		c(event_idx);
		return true;
	}});
}

bool
ircd::m::room::auth::for_each(const closure_bool &c)
const
{
	return for_each(*this, c);
}

bool
ircd::m::room::auth::for_each(const auth &a,
                              const closure_bool &c)
{
	using prototype = bool (const auth &, const closure_bool &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::auth::for_each"
	};

	return call(a, c);
}

void
ircd::m::room::auth::make_refs(const auth &a,
                               json::stack::array &out,
                               const types &t,
                               const m::id::user &u)
{
	using prototype = void (const auth &, json::stack::array &, const types &, const m::id::user &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::auth::make_refs"
	};

	return call(a, out, t, u);
}

//
// room::aliases
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

	return state.for_each("m.room.aliases", room::state::keys_bool{[this, &closure]
	(const string_view &state_key)
	{
		return for_each(state_key, closure);
	}});
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
	using prototype = bool (const m::room &, const string_view &, const closure_bool &);

	static mods::import<prototype> call
	{
		"m_room_aliases", "ircd::m::room::aliases::for_each"
	};

	return call(room, server, closure);
}

//
// room::aliases::cache
//

bool
ircd::m::room::aliases::cache::del(const alias &a)
{
	using prototype = bool (const alias &);

	static mods::import<prototype> call
	{
		"m_room_aliases", "ircd::m::room::aliases::cache::del"
	};

	return call(a);
}

bool
ircd::m::room::aliases::cache::set(const alias &a,
                                   const id &i)
{
	using prototype = bool (const alias &, const id &);

	static mods::import<prototype> call
	{
		"m_room_aliases", "ircd::m::room::aliases::cache::set"
	};

	return call(a, i);
}

bool
ircd::m::room::aliases::cache::fetch(std::nothrow_t,
                                     const alias &a,
                                     const net::hostport &hp)
try
{
	fetch(a, hp);
	return true;
}
catch(const std::exception &e)
{
	thread_local char buf[384];
	log::error
	{
		log, "Failed to fetch room_id for %s from %s :%s",
		string_view{a},
		string(buf, hp),
		e.what(),
	};

	return false;
}

void
ircd::m::room::aliases::cache::fetch(const alias &a,
                                     const net::hostport &hp)
{
	using prototype = void (const alias &, const net::hostport &);

	static mods::import<prototype> call
	{
		"m_room_aliases", "ircd::m::room::aliases::cache::fetch"
	};

	return call(a, hp);
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
ircd::m::room::aliases::cache::get(std::nothrow_t,
                                   const alias &a,
                                   const id::closure &c)
{
	using prototype = bool (std::nothrow_t, const alias &, const id::closure &);

	static mods::import<prototype> call
	{
		"m_room_aliases", "ircd::m::room::aliases::cache::get"
	};

	return call(std::nothrow, a, c);
}

bool
ircd::m::room::aliases::cache::has(const alias &a)
{
	using prototype = bool (const alias &);

	static mods::import<prototype> call
	{
		"m_room_aliases", "ircd::m::room::aliases::cache::has"
	};

	return call(a);
}

bool
ircd::m::room::aliases::cache::for_each(const closure_bool &c)
{
	return for_each(string_view{}, c);
}

bool
ircd::m::room::aliases::cache::for_each(const string_view &s,
                                        const closure_bool &c)
{
	using prototype = bool (const string_view &, const closure_bool &);

	static mods::import<prototype> call
	{
		"m_room_aliases", "ircd::m::room::aliases::cache::for_each"
	};

	return call(s, c);
}

//
// room::power
//

decltype(ircd::m::room::power::default_creator_level)
ircd::m::room::power::default_creator_level
{
	100
};

decltype(ircd::m::room::power::default_power_level)
ircd::m::room::power::default_power_level
{
	50
};

decltype(ircd::m::room::power::default_event_level)
ircd::m::room::power::default_event_level
{
	0
};

decltype(ircd::m::room::power::default_user_level)
ircd::m::room::power::default_user_level
{
	0
};

ircd::json::object
ircd::m::room::power::default_content(const mutable_buffer &out,
                                     const m::user::id &creator)
{
	using prototype = json::object (const mutable_buffer &, const m::user::id &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::power::default_content"
	};

	return call(out, creator);
}

//
// room::power::power
//

ircd::m::room::power::power(const m::room &room)
:power
{
	room, room.get(std::nothrow, "m.room.power_levels", "")
}
{
}

ircd::m::room::power::power(const m::room &room,
                            const event::idx &power_event_idx)
:room
{
	room
}
,power_event_idx
{
	power_event_idx
}
{
}

ircd::m::room::power::power(const m::event &power_event,
                            const m::event &create_event)
:power
{
	power_event, m::user::id(unquote(json::get<"content"_>(create_event).get("creator")))
}
{
}

ircd::m::room::power::power(const m::event &power_event,
                            const m::user::id &room_creator_id)
:power
{
	json::get<"content"_>(power_event), room_creator_id
}
{
}

ircd::m::room::power::power(const json::object &power_event_content,
                            const m::user::id &room_creator_id)
:power_event_content
{
	power_event_content
}
,room_creator_id
{
	room_creator_id
}
{
}

bool
ircd::m::room::power::operator()(const m::user::id &user_id,
                                 const string_view &prop,
                                 const string_view &type,
                                 const string_view &state_key)
const
{
	const auto &user_level
	{
		level_user(user_id)
	};

	const auto &required_level
	{
		empty(prop) || prop == "events"?
			level_event(type, state_key):
			level(prop)
	};

	return user_level >= required_level;
}

int64_t
ircd::m::room::power::level_user(const m::user::id &user_id)
const try
{
	int64_t ret
	{
		default_user_level
	};

	const auto closure{[&user_id, &ret]
	(const json::object &content)
	{
		const auto users_default
		{
			content.get<int64_t>("users_default", default_user_level)
		};

		const json::object &users
		{
			content.get("users")
		};

		ret = users.get<int64_t>(user_id, users_default);
	}};

	const bool has_power_levels_event
	{
		view(closure)
	};

	if(!has_power_levels_event)
	{
		if(room_creator_id && user_id == room_creator_id)
			ret = default_creator_level;

		if(room.room_id && creator(room, user_id))
			ret = default_creator_level;
	}

	return ret;
}
catch(const json::error &e)
{
	return default_user_level;
}

int64_t
ircd::m::room::power::level_event(const string_view &type)
const try
{
	int64_t ret
	{
		default_event_level
	};

	const auto closure{[&type, &ret]
	(const json::object &content)
	{
		const auto &events_default
		{
			content.get<int64_t>("events_default", default_event_level)
		};

		const json::object &events
		{
			content.get("events")
		};

		ret = events.get<int64_t>(type, events_default);
	}};

	const bool has_power_levels_event
	{
		view(closure)
	};

	return ret;
}
catch(const json::error &e)
{
	return default_event_level;
}

int64_t
ircd::m::room::power::level_event(const string_view &type,
                                  const string_view &state_key)
const try
{
	if(!defined(state_key))
		return level_event(type);

	int64_t ret
	{
		default_power_level
	};

	const auto closure{[&type, &ret]
	(const json::object &content)
	{
		const auto &state_default
		{
			content.get<int64_t>("state_default", default_power_level)
		};

		const json::object &events
		{
			content.get("events")
		};

		ret = events.get<int64_t>(type, state_default);
	}};

	const bool has_power_levels_event
	{
		view(closure)
	};

	return ret;
}
catch(const json::error &e)
{
	return default_power_level;
}

int64_t
ircd::m::room::power::level(const string_view &prop)
const try
{
	int64_t ret
	{
		default_power_level
	};

	view([&prop, &ret]
	(const json::object &content)
	{
		ret = content.at<int64_t>(prop);
	});

	return ret;
}
catch(const json::error &e)
{
	return default_power_level;
}

size_t
ircd::m::room::power::count_levels()
const
{
	size_t ret{0};
	for_each([&ret]
	(const string_view &, const int64_t &)
	{
		++ret;
	});

	return ret;
}

size_t
ircd::m::room::power::count_collections()
const
{
	size_t ret{0};
	view([&ret]
	(const json::object &content)
	{
		for(const auto &member : content)
			ret += json::type(member.second) == json::OBJECT;
	});

	return ret;
}

size_t
ircd::m::room::power::count(const string_view &prop)
const
{
	size_t ret{0};
	for_each(prop, [&ret]
	(const string_view &, const int64_t &)
	{
		++ret;
	});

	return ret;
}

bool
ircd::m::room::power::has_event(const string_view &type)
const try
{
	bool ret{false};
	view([&type, &ret]
	(const json::object &content)
	{
		const json::object &events
		{
			content.at("events")
		};

		const string_view &level
		{
			unquote(events.at(type))
		};

		ret = json::type(level) == json::NUMBER;
	});

	return ret;
}
catch(const json::error &)
{
	return false;
}

bool
ircd::m::room::power::has_user(const m::user::id &user_id)
const try
{
	bool ret{false};
	view([&user_id, &ret]
	(const json::object &content)
	{
		const json::object &users
		{
			content.at("users")
		};

		const string_view &level
		{
			unquote(users.at(user_id))
		};

		ret = json::type(level) == json::NUMBER;
	});

	return ret;
}
catch(const json::error &)
{
	return false;
}

bool
ircd::m::room::power::has_collection(const string_view &prop)
const
{
	bool ret{false};
	view([&prop, &ret]
	(const json::object &content)
	{
		const auto &value{content.get(prop)};
		if(value && json::type(value) == json::OBJECT)
			ret = true;
	});

	return ret;
}

bool
ircd::m::room::power::has_level(const string_view &prop)
const
{
	bool ret{false};
	view([&prop, &ret]
	(const json::object &content)
	{
		const auto &value(unquote(content.get(prop)));
		if(value && json::type(value) == json::NUMBER)
			ret = true;
	});

	return ret;
}

void
ircd::m::room::power::for_each(const closure &closure)
const
{
	for_each(string_view{}, closure);
}

bool
ircd::m::room::power::for_each(const closure_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

void
ircd::m::room::power::for_each(const string_view &prop,
                               const closure &closure)
const
{
	for_each(prop, closure_bool{[&closure]
	(const string_view &key, const int64_t &level)
	{
		closure(key, level);
		return true;
	}});
}

bool
ircd::m::room::power::for_each(const string_view &prop,
                               const closure_bool &closure)
const
{
	bool ret{true};
	view([&prop, &closure, &ret]
	(const json::object &content)
	{
		const json::object &collection
		{
			// This little cmov gimmick sets collection to be the outer object
			// itself if no property was given, allowing us to reuse this func
			// for all iterations of key -> level mappings.
			prop? json::object{content.get(prop)} : content
		};

		if(prop && json::type(string_view{collection}) != json::OBJECT)
			return;

		for(auto it(begin(collection)); it != end(collection) && ret; ++it)
		{
			const auto &member(*it);
			if(json::type(unquote(member.second)) != json::NUMBER)
				continue;

			const auto &key
			{
				unquote(member.first)
			};

			const auto &val
			{
				lex_cast<int64_t>(member.second)
			};

			ret = closure(key, val);
		}
	});

	return ret;
}

bool
ircd::m::room::power::view(const std::function<void (const json::object &)> &closure)
const
{
	if(power_event_idx)
		if(m::get(std::nothrow, power_event_idx, "content", closure))
			return true;

	closure(power_event_content);
	return !empty(power_event_content);
}

//
// room::stats
//

size_t
ircd::m::room::stats::bytes_json_compressed(const m::room &room)
{
	using prototype = size_t (const m::room &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::stats::bytes_json_compressed"
	};

	return call(room);
}

size_t
ircd::m::room::stats::bytes_json(const m::room &room)
{
	using prototype = size_t (const m::room &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::stats::bytes_json"
	};

	return call(room);
}

size_t
ircd::m::room::stats::bytes_total_compressed(const m::room &room)
{
	using prototype = size_t (const m::room &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::stats::bytes_total_compressed"
	};

	return call(room);
}

size_t
ircd::m::room::stats::bytes_total(const m::room &room)
{
	using prototype = size_t (const m::room &);

	static mods::import<prototype> call
	{
		"m_room", "ircd::m::room::stats::bytes_total"
	};

	return call(room);
}
