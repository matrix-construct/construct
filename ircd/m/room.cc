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

uint
ircd::m::version(const id::room &room_id)
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

	uint ret;
	state.get("m.room.create", "", [&ret]
	(const m::event &event)
	{
		const auto version_string
		{
			unquote(json::get<"content"_>(event).get("version", "1"))
		};

		ret = version_string?
			lex_cast<uint>(version_string):
			1;
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
ircd::m::my(const room &room)
{
	return my(room.room_id);
}

//
// room
//

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
	static const event::keys membership_keys
	{
		event::keys::include{"membership"}
	};

	// Since this is a member function of m::room there might be a supplied
	// fopts. Whatever keys it has are irrelevant, but we can preserve gopts.
	const m::event::fetch::opts fopts
	{
		membership_keys, this->fopts? this->fopts->gopts : db::gopts{}
	};

	string_view ret;
	const auto result_closure{[&out, &ret]
	(const m::event &event)
	{
		ret =
		{
			data(out), copy(out, m::membership(event))
		};
	}};

	const room::state state{*this, &fopts};
	const bool exists
	{
		state.get(std::nothrow, "m.room.member"_sv, user_id, result_closure)
	};

	// This branch is taken when the event exists but the event.membership had
	// no value. Due to wanton protocol violations event.membership may be
	// jsundefined and only event.content.membership will exist in event. This
	// is a rare case so both queries are optimized to only seek for their key.
	if(exists && !ret)
	{
		static const event::keys content_membership_keys
		{
			event::keys::include{"content"}
		};

		const m::event::fetch::opts fopts
		{
			content_membership_keys, this->fopts? this->fopts->gopts : db::gopts{}
		};

		const room::state state{*this, &fopts};
		state.get(std::nothrow, "m.room.member"_sv, user_id, result_closure);
	}

	return ret;
}

bool
ircd::m::room::has(const string_view &type)
const
{
	return get(std::nothrow, type, nullptr);
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
		fopts
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
		fopts:
		room.fopts
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
		fopts:
		room.fopts
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
	fopts? fopts : room.fopts
}
{
	seek(depth);
}

const ircd::m::event &
ircd::m::room::messages::operator*()
{
	return fetch(std::nothrow);
};

bool
ircd::m::room::messages::seek()
{
	this->it = dbs::room_events.begin(room.room_id);
	return bool(*this);
}

bool
ircd::m::room::messages::seek(const event::id &event_id)
try
{
	const event::idx &event_idx
	{
		index(event_id)
	};

	return seek_idx(event_idx);
}
catch(const db::not_found &e)
{
	return false;
}

bool
ircd::m::room::messages::seek(const uint64_t &depth)
{
	char buf[dbs::ROOM_EVENTS_KEY_MAX_SIZE];
	const auto seek_key
	{
		dbs::room_events_key(buf, room.room_id, depth)
	};

	this->it = dbs::room_events.begin(seek_key);
	return bool(*this);
}

bool
ircd::m::room::messages::seek_idx(const event::idx &event_idx)
try
{
	uint64_t depth;
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
	if(event_idx != this->event_idx())
		return false;

	return true;
}
catch(const db::not_found &e)
{
	return false;
}

ircd::m::event::id::buf
ircd::m::room::messages::event_id()
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

const ircd::m::event::idx &
ircd::m::room::messages::event_idx()
{
	assert(bool(*this));
	const auto &key{it->first};
	const auto part{dbs::room_events_key(key)};
	_event_idx = std::get<1>(part);
	return _event_idx;
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
	if(!m::seek(_event, event_idx(), std::nothrow))
		_event = m::event::fetch{room.fopts};

	return _event;
}

//
// room::state
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
	event_id?
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

ircd::m::event::id::buf
ircd::m::room::state::get(const string_view &type,
                          const string_view &state_key)
const
{
	event::id::buf ret;
	get(type, state_key, event::id::closure{[&ret]
	(const event::id &event_id)
	{
		ret = event_id;
	}});

	return ret;
}

ircd::m::event::id::buf
ircd::m::room::state::get(std::nothrow_t,
                          const string_view &type,
                          const string_view &state_key)
const
{
	event::id::buf ret;
	get(std::nothrow, type, state_key, event::id::closure{[&ret]
	(const event::id &event_id)
	{
		ret = event_id;
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
			event_idx, fopts
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
			closure(index(unquote(event_id)));
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
			event_idx, std::nothrow, fopts
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
			return closure(index(unquote(event_id), std::nothrow));
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
	if(!present())
		return m::state::count(root_id);

	db::gopts opts
	{
		this->fopts? this->fopts->gopts : db::gopts{}
	};

	if(!opts.readahead)
		opts.readahead = 0_KiB;

	size_t ret{0};
	auto &column{dbs::room_state};
	for(auto it{column.begin(room_id, opts)}; bool(it); ++it)
		++ret;

	return ret;
}

size_t
ircd::m::room::state::count(const string_view &type)
const
{
	if(!type)
		return count();

	if(!present())
		return m::state::count(root_id, type);

	char keybuf[dbs::ROOM_STATE_KEY_MAX_SIZE];
	const auto &key
	{
		dbs::room_state_key(keybuf, room_id, type)
	};

	db::gopts opts
	{
		this->fopts? this->fopts->gopts : db::gopts{}
	};

	if(!opts.readahead)
		opts.readahead = 0_KiB;

	size_t ret{0};
	auto &column{dbs::room_state};
	for(auto it{column.begin(key, opts)}; bool(it); ++it)
		if(std::get<0>(dbs::room_state_key(it->first)) == type)
			++ret;
		else
			break;

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
		fopts
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
		return !m::state::test(root_id, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			return !closure(unquote(event_id));
		});

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
	if(!present())
		return !m::state::test(root_id, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			return !closure(index(unquote(event_id), std::nothrow));
		});

	db::gopts opts
	{
		this->fopts? this->fopts->gopts : db::gopts{}
	};

	if(!opts.readahead)
		opts.readahead = 0_KiB;

	auto &column{dbs::room_state};
	for(auto it{column.begin(room_id, opts)}; bool(it); ++it)
		if(!closure(byte_view<event::idx>(it->second)))
			return false;

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
	event::fetch event
	{
		fopts
	};

	return for_each(type, event::closure_idx_bool{[&event, &closure]
	(const event::idx &event_idx)
	{
		if(seek(event, event_idx, std::nothrow))
			if(!closure(event))
				return false;

		return true;
	}});
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
	if(!type)
		return for_each(closure);

	if(!present())
		return !m::state::test(root_id, type, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			return !closure(unquote(event_id));
		});

	return for_each(type, event::closure_idx_bool{[&closure]
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
	if(!type)
		return for_each(closure);

	if(!present())
		return !m::state::test(root_id, type, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			return !closure(index(unquote(event_id), std::nothrow));
		});

	char keybuf[dbs::ROOM_STATE_KEY_MAX_SIZE];
	const auto &key
	{
		dbs::room_state_key(keybuf, room_id, type)
	};

	db::gopts opts
	{
		this->fopts? this->fopts->gopts : db::gopts{}
	};

	if(!opts.readahead)
		opts.readahead = 0_KiB;

	auto &column{dbs::room_state};
	for(auto it{column.begin(key, opts)}; bool(it); ++it)
		if(std::get<0>(dbs::room_state_key(it->first)) == type)
		{
			if(!closure(byte_view<event::idx>(it->second)))
				return false;
		}
		else break;

	return true;
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const string_view &state_key_lb,
                               const event::closure_bool &closure)
const
{
	event::fetch event
	{
		fopts
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
		return !m::state::test(root_id, type, state_key_lb, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			return !closure(unquote(event_id));
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
	if(!present())
		return !m::state::test(root_id, type, state_key_lb, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			return !closure(index(unquote(event_id), std::nothrow));
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
		opts.readahead = 0_KiB;

	auto &column{dbs::room_state};
	for(auto it{column.begin(key, opts)}; bool(it); ++it)
		if(std::get<0>(dbs::room_state_key(it->first)) == type)
		{
			if(!closure(byte_view<event::idx>(it->second)))
				return false;
		}
		else break;

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
	if(!present())
		return !m::state::test(root_id, type, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			assert(size(key) >= 2);
			return !closure(unquote(key.at(1)));
		});

	char keybuf[dbs::ROOM_STATE_KEY_MAX_SIZE];
	const auto &key
	{
		dbs::room_state_key(keybuf, room_id, type)
	};

	db::gopts opts
	{
		this->fopts? this->fopts->gopts : db::gopts{}
	};

	if(!opts.readahead)
		opts.readahead = 0_KiB;

	auto &column{dbs::room_state};
	for(auto it{column.begin(key, opts)}; bool(it); ++it)
	{
		const auto part
		{
			dbs::room_state_key(it->first)
		};

		if(std::get<0>(part) == type)
		{
			if(!closure(std::get<1>(part)))
				return false;
		}
		else break;
	}

	return true;
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
	char lastbuf[256]; //TODO: type maxlen
	if(!present())
	{
		m::state::for_each(root_id, [&closure, &last, &lastbuf]
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
		});

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

bool
ircd::m::room::state::present()
const
{
	return !event_id || !root_id || std::get<0>(top(room_id)) == event_id;
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
	static const event::keys keys
	{
		event::keys::include
		{
			"event_id",    // Added for any upstack usage (but may be unnecessary).
			"membership",  // Required for membership test.
			"content",     // Required because synapse events randomly have no event.membership
		}
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
	static const event::keys keys
	{
		event::keys::include
		{
			"event_id",
			"membership",
			"state_key",
			"content",    // Required because synapse events randomly have no event.membership
		}
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
		return false;
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
	using prototype = bool (const m::room &,
	                        const m::room::origins::closure &,
	                        const m::room::origins::closure_bool &);

	static mods::import<prototype> random_origin
	{
		"m_room", "random_origin"
	};

	return random_origin(room, view, proffer);
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
	char lastbuf[256];
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
	auto &index
	{
		dbs::room_head
	};

	auto it
	{
		index.begin(room.room_id)
	};

	for(; it; ++it)
	{
		const event::id &event_id
		{
			dbs::room_head_key(it->first)
		};

		const event::idx &event_idx
		{
			byte_view<event::idx>{it->second}
		};

		if(!closure(event_idx, event_id))
			return false;
	}

	return true;
}

//
// room::power
//

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

bool
ircd::m::room::power::operator()(const m::user::id &user_id,
                                 const string_view &prop,
                                 const string_view &type)
const
{
	const auto &user_level
	{
		level_user(user_id)
	};

	const auto &required_level
	{
		prop == "events"?
			level_event(type):
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

	view([&user_id, &ret]
	(const json::object &content)
	{
		const json::object &users
		{
			content.at("users")
		};

		ret = users.at<int64_t>(user_id);
	});

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

	view([&type, &ret]
	(const json::object &content)
	{
		const json::object &events
		{
			content.at("events")
		};

		ret = events.at<int64_t>(type);
	});

	return ret;
}
catch(const json::error &e)
{
	return default_event_level;
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
			events.at(type)
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
			users.at(user_id)
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
		const auto &value{content.get(prop)};
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
			if(json::type(member.second) != json::NUMBER)
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
	static const event::keys keys
	{
		event::keys::include
		{
			"content",
		}
	};

	const m::event::fetch::opts fopts
	{
		keys, state.fopts? state.fopts->gopts : db::gopts{}
	};

	return state.get(std::nothrow, "m.room.power_levels", "", [&closure]
	(const m::event &event)
	{
		closure(json::get<"content"_>(event));
	});
}
