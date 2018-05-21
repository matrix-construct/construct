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

bool
ircd::m::room::visible(const user::id &user_id,
                       const event *const &event_)
const
{
	using prototype = bool (const room &, const user &, const event *const &);

	static import<prototype> function
	{
		"m_room_history_visibility", "visible__user"
	};

	return function(*this, user_id, event_);
}

bool
ircd::m::room::visible(const node::id &node_id,
                       const event *const &event_)
const
{
	using prototype = bool (const room &, const node &, const event *const &);

	static import<prototype> function
	{
		"m_room_history_visibility", "visible__node"
	};

	return function(*this, node_id, event_);
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
	const state state{*this};
	state.get(std::nothrow, "m.room.member"_sv, user_id, [&out, &ret]
	(const m::event &event)
	{
		assert(json::get<"type"_>(event) == "m.room.member");
		ret = { data(out), copy(out, unquote(at<"membership"_>(event))) };
	});

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

ircd::m::room::messages::messages(const m::room &room)
:room{room}
,_event{room.fopts}
{
	seek();
}

ircd::m::room::messages::messages(const m::room &room,
                                  const event::id &event_id)
:room{room}
,_event{room.fopts}
{
	seek(event_id);
}

ircd::m::room::messages::messages(const m::room &room,
                                  const uint64_t &depth)
:room{room}
,_event{room.fopts}
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
{
	auto &column
	{
		dbs::event_column.at(json::indexof<event, "depth"_>())
	};

	const event::idx event_idx
	{
		index(event_id)
	};

	uint64_t depth;
	column(byte_view<string_view>(event_idx), [&depth]
	(const string_view &value)
	{
		depth = byte_view<uint64_t>(value);
	});

	char buf[dbs::ROOM_EVENTS_KEY_MAX_SIZE];
	const auto seek_key
	{
		dbs::room_events_key(buf, room.room_id, depth, event_idx)
	};

	this->it = dbs::room_events.begin(seek_key);
	return bool(*this);
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

ircd::m::room::state::state(const m::room &room)
:state
{
	room, room.fopts
}
{
}

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
	fopts
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
	return test(type, event::id::closure_bool{[](const m::event::id &)
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

	size_t ret{0};
	auto &column{dbs::room_state};
	for(auto it{column.begin(room_id)}; bool(it); ++it)
		++ret;

	return ret;
}

size_t
ircd::m::room::state::count(const string_view &type)
const
{
	if(!present())
		return m::state::count(root_id, type);

	char keybuf[dbs::ROOM_STATE_KEY_MAX_SIZE];
	const auto &key
	{
		dbs::room_state_key(keybuf, room_id, type)
	};

	size_t ret{0};
	auto &column{dbs::room_state};
	for(auto it{column.begin(key)}; bool(it); ++it)
		if(std::get<0>(dbs::room_state_key(it->first)) == type)
			++ret;
		else
			break;

	return ret;
}

bool
ircd::m::room::state::test(const event::closure_bool &closure)
const
{
	event::fetch event
	{
		fopts
	};

	return test(event::closure_idx_bool{[&event, &closure]
	(const event::idx &event_idx)
	{
		if(seek(event, event_idx, std::nothrow))
			if(closure(event))
				return true;

		return false;
	}});
}

bool
ircd::m::room::state::test(const event::id::closure_bool &closure)
const
{
	if(!present())
		return m::state::test(root_id, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			return closure(unquote(event_id));
		});

	return test(event::closure_idx_bool{[&closure]
	(const event::idx &idx)
	{
		bool ret{false};
		event::fetch::event_id(idx, std::nothrow, [&ret, &closure]
		(const event::id &id)
		{
			ret = closure(id);
		});

		return ret;
	}});
}

bool
ircd::m::room::state::test(const event::closure_idx_bool &closure)
const
{
	if(!present())
		return m::state::test(root_id, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			return closure(index(unquote(event_id), std::nothrow));
		});

	auto &column{dbs::room_state};
	for(auto it{column.begin(room_id)}; bool(it); ++it)
		if(closure(byte_view<event::idx>(it->second)))
			return true;

	return false;
}

bool
ircd::m::room::state::test(const string_view &type,
                           const event::closure_bool &closure)
const
{
	event::fetch event
	{
		fopts
	};

	return test(type, event::closure_idx_bool{[&event, &closure]
	(const event::idx &event_idx)
	{
		if(seek(event, event_idx, std::nothrow))
			if(closure(event))
				return true;

		return false;
	}});
}

bool
ircd::m::room::state::test(const string_view &type,
                           const event::id::closure_bool &closure)
const
{
	if(!present())
		return m::state::test(root_id, type, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			return closure(unquote(event_id));
		});

	return test(type, event::closure_idx_bool{[&closure]
	(const event::idx &idx)
	{
		bool ret{false};
		event::fetch::event_id(idx, std::nothrow, [&ret, &closure]
		(const event::id &id)
		{
			ret = closure(id);
		});

		return ret;
	}});
}

bool
ircd::m::room::state::test(const string_view &type,
                           const event::closure_idx_bool &closure)
const
{
	if(!present())
		return m::state::test(root_id, type, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			return closure(index(unquote(event_id), std::nothrow));
		});

	char keybuf[dbs::ROOM_STATE_KEY_MAX_SIZE];
	const auto &key
	{
		dbs::room_state_key(keybuf, room_id, type)
	};

	auto &column{dbs::room_state};
	for(auto it{column.begin(key)}; bool(it); ++it)
		if(std::get<0>(dbs::room_state_key(it->first)) == type)
		{
			if(closure(byte_view<event::idx>(it->second)))
				return true;
		}
		else break;

	return false;
}

bool
ircd::m::room::state::test(const string_view &type,
                           const keys_bool &closure)
const
{
	if(!present())
		return m::state::test(root_id, type, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			assert(size(key) >= 2);
			return closure(unquote(key.at(1)));
		});

	char keybuf[dbs::ROOM_STATE_KEY_MAX_SIZE];
	const auto &key
	{
		dbs::room_state_key(keybuf, room_id, type)
	};

	auto &column{dbs::room_state};
	for(auto it{column.begin(key)}; bool(it); ++it)
	{
		const auto part
		{
			dbs::room_state_key(it->first)
		};

		if(std::get<0>(part) == type)
		{
			if(closure(std::get<1>(part)))
				return true;
		}
		else break;
	}

	return false;
}

bool
ircd::m::room::state::test(const string_view &type,
                           const string_view &state_key_lb,
                           const event::closure_bool &closure)
const
{
	event::fetch event
	{
		fopts
	};

	return test(type, state_key_lb, event::closure_idx_bool{[&event, &closure]
	(const event::idx &event_idx)
	{
		if(seek(event, event_idx, std::nothrow))
			if(closure(event))
				return true;

		return false;
	}});
}

bool
ircd::m::room::state::test(const string_view &type,
                           const string_view &state_key_lb,
                           const event::id::closure_bool &closure)
const
{
	if(!present())
		return m::state::test(root_id, type, state_key_lb, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			return closure(unquote(event_id));
		});

	return test(type, state_key_lb, event::closure_idx_bool{[&closure]
	(const event::idx &idx)
	{
		bool ret{false};
		event::fetch::event_id(idx, std::nothrow, [&ret, &closure]
		(const event::id &id)
		{
			ret = closure(id);
		});

		return ret;
	}});
}

bool
ircd::m::room::state::test(const string_view &type,
                           const string_view &state_key_lb,
                           const event::closure_idx_bool &closure)
const
{
	if(!present())
		return m::state::test(root_id, type, state_key_lb, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			return closure(index(unquote(event_id), std::nothrow));
		});

	char keybuf[dbs::ROOM_STATE_KEY_MAX_SIZE];
	const auto &key
	{
		dbs::room_state_key(keybuf, room_id, type, state_key_lb)
	};

	auto &column{dbs::room_state};
	for(auto it{column.begin(key)}; bool(it); ++it)
		if(std::get<0>(dbs::room_state_key(it->first)) == type)
		{
			if(closure(byte_view<event::idx>(it->second)))
				return true;
		}
		else break;

	return false;
}

void
ircd::m::room::state::for_each(const event::closure &closure)
const
{
	event::fetch event
	{
		fopts
	};

	for_each(event::closure_idx{[&event, &closure]
	(const event::idx &event_idx)
	{
		if(seek(event, event_idx, std::nothrow))
			closure(event);
	}});
}

void
ircd::m::room::state::for_each(const event::id::closure &closure)
const
{
	if(!present())
		return m::state::for_each(root_id, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			closure(unquote(event_id));
		});

	for_each(event::closure_idx{[&closure]
	(const event::idx &idx)
	{
		event::fetch::event_id(idx, std::nothrow, closure);
	}});
}

void
ircd::m::room::state::for_each(const event::closure_idx &closure)
const
{
	if(!present())
		return m::state::for_each(root_id, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			closure(index(unquote(event_id), std::nothrow));
		});

	auto &column{dbs::room_state};
	for(auto it{column.begin(room_id)}; bool(it); ++it)
		closure(byte_view<event::idx>(it->second));
}

void
ircd::m::room::state::for_each(const string_view &type,
                               const event::closure &closure)
const
{
	event::fetch event
	{
		fopts
	};

	for_each(type, event::closure_idx{[&event, &closure]
	(const event::idx &event_idx)
	{
		if(seek(event, event_idx, std::nothrow))
			closure(event);
	}});
}

void
ircd::m::room::state::for_each(const string_view &type,
                               const event::id::closure &closure)
const
{
	if(!present())
		return m::state::for_each(root_id, type, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			closure(unquote(event_id));
		});

	for_each(type, event::closure_idx{[&closure]
	(const event::idx &idx)
	{
		event::fetch::event_id(idx, std::nothrow, closure);
	}});
}

void
ircd::m::room::state::for_each(const string_view &type,
                               const event::closure_idx &closure)
const
{
	if(!present())
		return m::state::for_each(root_id, type, [&closure]
		(const json::array &key, const string_view &event_id)
		{
			closure(index(unquote(event_id), std::nothrow));
		});

	char keybuf[dbs::ROOM_STATE_KEY_MAX_SIZE];
	const auto &key
	{
		dbs::room_state_key(keybuf, room_id, type)
	};

	auto &column{dbs::room_state};
	for(auto it{column.begin(key)}; bool(it); ++it)
		if(std::get<0>(dbs::room_state_key(it->first)) == type)
			closure(byte_view<event::idx>(it->second));
		else
			break;
}

void
ircd::m::room::state::for_each(const string_view &type,
                               const keys &closure)
const
{
	if(!present())
		return m::state::for_each(root_id, type, [&closure]
		(const json::array &key, const string_view &)
		{
			assert(size(key) >= 2);
			closure(unquote(key.at(1)));
		});

	char keybuf[dbs::ROOM_STATE_KEY_MAX_SIZE];
	const auto &key
	{
		dbs::room_state_key(keybuf, room_id, type)
	};

	auto &column{dbs::room_state};
	for(auto it{column.begin(key)}; bool(it); ++it)
	{
		const auto part
		{
			dbs::room_state_key(it->first)
		};

		if(std::get<0>(part) == type)
			closure(std::get<1>(part));
		else
			break;
	}
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
ircd::m::room::members::test(const event::closure_bool &closure)
const
{
	const room::state state{room};
	return state.test("m.room.member", event::closure_bool{[&closure]
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

	return !test(membership, event::closure_bool{[&closure]
	(const event &event)
	{
		const user::id &user_id
		{
			at<"state_key"_>(event)
		};

		return !closure(user_id);
	}});
}

void
ircd::m::room::members::for_each(const string_view &membership,
                                 const event::closure &closure)
const
{
	test(membership, [&closure]
	(const event &event)
	{
		closure(event);
		return false;
	});
}

bool
ircd::m::room::members::test(const string_view &membership,
                             const event::closure_bool &closure)
const
{
	if(empty(membership))
		return test(closure);

	// joined members optimization. Only possible when seeking
	// membership="join" on the present state of the room.
	if(!room.event_id && membership == "join")
	{
		const room::origins origins{room};
		return origins._test_([&closure, this]
		(const string_view &key)
		{
			const string_view &member
			{
				std::get<1>(dbs::room_joined_key(key))
			};

			bool ret{false};
			room.get(std::nothrow, "m.room.member", member, event::closure{[&closure, &ret]
			(const event &event)
			{
				ret = closure(event);
			}});

			return ret;
		});
	}

	return test(event::closure_bool{[&membership, &closure]
	(const event &event)
	{
		if(m::membership(event) == membership)
			if(closure(event))
				return true;

		return false;
	}});
}

size_t
ircd::m::room::members::count(const string_view &membership)
const
{
	// joined members optimization. Only possible when seeking
	// membership="join" on the present state of the room.
	if(!room.event_id && membership == "join")
	{
		size_t ret{0};
		const room::origins origins{room};
		origins._test_([&ret](const string_view &)
		{
			++ret;
			return false;
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

//
// room::origins
//

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
	test([&view](const string_view &origin)
	{
		view(origin);
		return false;
	});
}

bool
ircd::m::room::origins::test(const closure_bool &view)
const
{
	string_view last;
	char lastbuf[256];
	return _test_([&last, &lastbuf, &view]
	(const string_view &key)
	{
		const string_view &origin
		{
			std::get<0>(dbs::room_joined_key(key))
		};

		if(origin == last)
			return false;

		if(view(origin))
			return true;

		last = { lastbuf, copy(lastbuf, origin) };
		return false;
	});
}

bool
ircd::m::room::origins::_test_(const closure_bool &view)
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

		if(view(key))
			return true;
	}

	return false;
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
// room::state::tuple
//

ircd::m::room::state::tuple::tuple(const m::room &room,
                                   const mutable_buffer &buf_)
{
	const m::room::state state
	{
		room
	};

	window_buffer buf{buf_};
	json::for_each(*this, [&state, &buf]
	(const string_view &type, auto &event)
	{
		state.get(std::nothrow, type, "", [&buf, &event]
		(const event::idx &event_idx)
		{
			buf([&event, &event_idx]
			(const mutable_buffer &buf)
			{
				event = m::event
				{
					event_idx, buf
				};

				return serialized(event);
			});
		});
	});
}

ircd::m::room::state::tuple::tuple(const json::array &pdus)
{
	for(const json::object &pdu : pdus)
	{
		const m::event &event{pdu};
		json::set(*this, at<"type"_>(event), event);
	}
}

std::string
ircd::m::pretty(const room::state::tuple &state)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 2048);

	const auto out{[&s]
	(const string_view &key, const auto &event)
	{
		if(!json::get<"event_id"_>(event))
			return;

		s << std::setw(28) << std::right << key
		  << " :" << json::get<"depth"_>(event)
		  << " " << at<"event_id"_>(event)
		  << " " << json::get<"sender"_>(event)
		  << " " << json::get<"content"_>(event)
		  << std::endl;
	}};

	json::for_each(state, out);
	resizebuf(s, ret);
	return ret;
}

std::string
ircd::m::pretty_oneline(const room::state::tuple &state)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 1024);

	const auto out{[&s]
	(const string_view &key, const auto &event)
	{
		if(!json::get<"event_id"_>(event))
			return;

		s << key << " ";
	}};

	json::for_each(state, out);
	resizebuf(s, ret);
	return ret;
}
